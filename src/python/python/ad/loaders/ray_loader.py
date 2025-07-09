from __future__ import annotations as __annotations__ # Delayed parsing of type annotations

from typing import Tuple

import mitsuba as mi
import drjit as dr

class flat_sensor(mi.Sensor):
    def __init__(self, props):
        mi.Sensor.__init__(self, props)

        # To be updated by RayLoader
        self.pixel_idx = dr.zeros(mi.UInt32, 1)

    def initialize(
        self,
        sensors: list[mi.Sensor],
        pixels_per_batch: int,
    ) -> None:
        """Initialize the flat sensor with multiple sensors."""
        self.num_sensors = len(sensors)
        film_size = sensors[0].film().size()
        self.source_film_width = film_size[0]
        self.source_film_height = film_size[1]
        self.pixels_per_batch = pixels_per_batch

        # Build vectorized pointer type of sensors
        self.sensors_unique_ptr = dr.zeros(mi.SensorPtr, self.num_sensors)
        for i in range(self.num_sensors):
            dr.scatter(self.sensors_unique_ptr,
                      mi.SensorPtr(sensors[i]), i)

    def sample_ray_differential(
        self,
        time: mi.Float,
        sample1: mi.Float,
        sample2: mi.Point2f,
        sample3: mi.Point2f,
        active: mi.Bool = True,
    ) -> Tuple[mi.RayDifferential3f, mi.Spectrum]:

        spp = dr.width(sample2) // dr.width(self.pixel_idx)
        if spp == 0:
            # This should not be traced as a virtual function
            return dr.zeros(mi.RayDifferential3f), dr.zeros(mi.Spectrum)

        # Compute the corresponding sensor index
        pixel_idx = dr.repeat(self.pixel_idx, spp)
        sensor_idx = pixel_idx // (self.source_film_width *
                                  self.source_film_height)

        # Convert flat pixel index to 2D coordinates within the target sensor
        pixel_idx_in_sensor = (
            pixel_idx % (self.source_film_width * self.source_film_height)
        )
        target_pixel_y = mi.Float(pixel_idx_in_sensor // self.source_film_width)
        target_pixel_x = mi.Float(pixel_idx_in_sensor % self.source_film_width)

        # Retrieve the jitter for the sample2 coordinates
        jitter = sample2 * mi.ScalarVector2f(self.film().crop_size())
        jitter -= mi.Vector2i(jitter)

        # Create the remapped sample coordinates for real sensors
        sample2_override = mi.Point2f(
            (target_pixel_x + jitter.x) / float(self.source_film_width),
            (target_pixel_y + jitter.y) / float(self.source_film_height)
        )

        # Dispatch the ray sampling to the corresponding sensors
        sensors_ptr = dr.gather(
            mi.SensorPtr, self.sensors_unique_ptr, sensor_idx
        )
        rays, weights = sensors_ptr.sample_ray_differential(
            time=time,
            sample1=sample1,
            sample2=sample2_override,
            sample3=sample3,
            active=active,
        )

        return rays, weights

    def sample_ray(
        self,
        time: mi.Float,
        sample1: mi.Float,
        sample2: mi.Point2f,
        sample3: mi.Point2f,
        active: mi.Bool = True,
    ) -> Tuple[mi.RayDifferential3f, mi.Spectrum]:
        return self.sample_ray_differential(
            time, sample1, sample2, sample3, active)

    def to_string(self):
        return ('Flat sensor[\n'
                '    num_sensors=%d,\n'
                '    width=%d,\n'
                '    height=%d,\n'
                ']' % (self.num_sensors, self.width, self.height))

mi.register_sensor("flat_sensor",
                   lambda props: flat_sensor(props))


class Rayloader():
    def __init__(
        self,
        sensors: list[mi.Sensor] | mi.Sensor,
        target_images: list[mi.TensorXf] | mi.TensorXf,
        pixels_per_batch: int,
        seed: int = 0,
        regular_reshuffle: bool = False,
        tile_size: int = 1,
    ) -> None:
        """Rayloader for efficient batch rendering with multiple sensors."""

        # Validate tile_size parameter
        if tile_size <= 0:
            raise ValueError(f"tile_size ({tile_size}) must be positive")

        if type(target_images) != list:
            target_images = [target_images]
        if type(sensors) != list:
            sensors = [sensors]

        reference_film = sensors[0].film()
        self.num_sensors = len(sensors)
        self.width = reference_film.size()[0]
        self.height = reference_film.size()[1]
        self.channel_size = target_images[0].shape[-1]
        self.ttl_pixels = self.num_sensors * self.width * self.height

        # Tile-related properties
        self.tile_size = tile_size
        self.tiles_per_row = dr.ceil(self.width / tile_size)
        self.tiles_per_col = dr.ceil(self.height / tile_size)
        self.tiles_per_sensor = self.tiles_per_row * self.tiles_per_col

        # Assert that pixels_per_batch is valid
        if pixels_per_batch <= 0 or pixels_per_batch > self.ttl_pixels:
            raise ValueError(
                f"pixels_per_batch ({pixels_per_batch}) must be "
                f"greater than 0 and less than or equal to "
                f"ttl_pixels ({self.ttl_pixels})"
            )
        # Assert that all sensors and target_images have the same size
        if len(sensors) != len(target_images):
            raise ValueError(
                f"Number of sensors ({len(sensors)}) does not match "
                f"number of target images ({len(target_images)})"
            )
        for i in range(self.num_sensors):
            film = sensors[i].film()
            film_size = film.size()
            if (film_size[0] != self.width or film_size[1] != self.height):
                raise ValueError(
                    f"Sensor {i} has different film size: "
                    f"{film_size[0]}x{film_size[1]} "
                    f"vs {self.width}x{self.height}"
                )
            if film.sample_border():
                raise ValueError(
                    f"Sensor {i} has sample_border enabled, "
                    "which is not supported by RayLoader."
                )
            tensor_shape = target_images[i].shape
            if (tensor_shape[0] != self.height or
                tensor_shape[1] != self.width or
                tensor_shape[2] != film.base_channels_count()):
                raise ValueError(
                    f"Target image {i} has different shape: "
                    f"{tensor_shape[0]}x{tensor_shape[1]}x{tensor_shape[2]} "
                    f"vs {self.height}x{self.width}x{self.channel_size}"
                )

        # Determine pixel format and component format
        if self.channel_size == 1:
            pixel_format = 'luminance'
        elif self.channel_size == 3:
            pixel_format = 'rgb'
        elif self.channel_size == 4:
            pixel_format = 'rgba'
        else:
            raise ValueError(
                f"Unsupported channel size: {self.channel_size}. "
                "Only 1, 3, or 4 channels are supported."
            )
        # Determine component format
        component_format = 'float32'

        self.flat_sensor = mi.load_dict({
            'type': 'flat_sensor',
            'flat_film': {
                'type': 'hdrfilm',
                'width': pixels_per_batch,
                'height': 1,
                'pixel_format': pixel_format,
                'component_format': component_format,
                'sample_border': False,
                'filter': {'type': 'box'},
            },
            'sampler': {
                'type': 'independent',
                'sample_count': 1,
            },
        })
        self.flat_sensor.initialize(sensors, pixels_per_batch)

        # Dummy initialization of pixel_idx, updated by `next()`
        self.flat_sensor.pixel_idx = None
        self.perm_target_image_flat = None

        self.target_images = target_images
        self.pixels_per_batch = pixels_per_batch
        self.seed = seed
        self.regular_reshuffle = regular_reshuffle
        self.counter = 0

        # Handle case where total pixels is not divisible by pixels_per_batch
        # by padding with repeated indices
        self.effective_ttl_pixels = (
            ((self.ttl_pixels + self.pixels_per_batch - 1) //
            self.pixels_per_batch) * self.pixels_per_batch
        )

        # Calculate how many iterations to do a shuffle cycle
        self.iter_shuffle = (
            self.effective_ttl_pixels // self.pixels_per_batch
        )

        # Calculate the pixel batch multiplier
        self.pixel_batch_multiplier = float(self.ttl_pixels / pixels_per_batch)

    def _create_padded_buffer(self, pixel_indices):
        """Create a padded buffer from pixel indices to fit effective_ttl_pixels.

        Args:
            pixel_indices: DrJit array of pixel indices

        Returns:
            Padded buffer of size effective_ttl_pixels
        """
        buffer = dr.empty(mi.UInt32, self.effective_ttl_pixels)
        actual_count = len(pixel_indices)

        if actual_count >= self.effective_ttl_pixels:
            # Take only what we need
            buffer_indices = dr.arange(mi.UInt32, self.effective_ttl_pixels)
            final_indices = dr.gather(mi.UInt32, pixel_indices, buffer_indices)
            dr.scatter(buffer, final_indices, buffer_indices)
        else:
            # Use all pixels and pad the rest
            buffer_indices = dr.arange(mi.UInt32, actual_count)
            dr.scatter(buffer, pixel_indices, buffer_indices)

            # Pad remaining slots by repeating the pattern
            remaining_count = self.effective_ttl_pixels - actual_count
            remaining_indices = dr.arange(mi.UInt32, remaining_count) + actual_count
            padding_source_indices = dr.arange(mi.UInt32, remaining_count) % actual_count
            padding_values = dr.gather(mi.UInt32, pixel_indices, padding_source_indices)
            dr.scatter(buffer, padding_values, remaining_indices)

        return buffer

    def flat_tile_idx_to_tile_coords(self, flat_tile_idx: int, sensor_idx: int) -> tuple[int, int]:
        """Convert flat tile index to (tile_row, tile_col) coordinates within sensor."""
        local_tile_idx = flat_tile_idx % self.tiles_per_sensor
        tile_row = local_tile_idx // self.tiles_per_row
        tile_col = local_tile_idx % self.tiles_per_row
        return tile_row, tile_col

    def tile_coords_to_pixel_range(self, tile_row: int, tile_col: int) -> tuple[int, int, int, int]:
        """Convert tile coordinates to pixel coordinate ranges (start_y, end_y, start_x, end_x)."""
        start_y = tile_row * self.tile_size
        end_y = min(start_y + self.tile_size, self.height)
        start_x = tile_col * self.tile_size
        end_x = min(start_x + self.tile_size, self.width)
        return start_y, end_y, start_x, end_x

    def generate_tile_pixels(self, tile_idx: int, sensor_idx: int) -> list[int]:
        """Generate all pixel indices within a tile, handling boundary clamping."""
        tile_row, tile_col = self.flat_tile_idx_to_tile_coords(tile_idx, sensor_idx)
        start_y, end_y, start_x, end_x = self.tile_coords_to_pixel_range(tile_row, tile_col)

        pixel_indices = []
        sensor_pixel_offset = sensor_idx * self.width * self.height

        for y in range(start_y, end_y):
            for x in range(start_x, end_x):
                pixel_flat_idx = y * self.width + x + sensor_pixel_offset
                pixel_indices.append(pixel_flat_idx)

        return pixel_indices

    def shuffle_pixel_index(self, seed: mi.UInt32):
        """Shuffle pixel index buffer using tile-based permutation for GPU coherence."""
        if self.tile_size == 1:
            # Pixel-based permutation
            self.perm_pixel_index_buffer = mi.permute_kensler(
                dr.arange(mi.UInt32, self.effective_ttl_pixels) % self.ttl_pixels,
                self.effective_ttl_pixels,
                seed,
            )
        else:
            self._shuffle_pixel_index_tiled(seed)

    def _shuffle_pixel_index_tiled(self, seed: mi.UInt32):
        """Tile-based permutation for GPU coherence."""
        # Step 1: Create tile ordering permutation
        total_tiles = self.num_sensors * self.tiles_per_sensor
        tile_permutation = mi.permute_kensler(
            dr.arange(mi.UInt32, total_tiles),
            total_tiles,
            seed,
        )

        # Step 2: Treat sampled pixels as tile centers, generate tiles.

        # Special case: if tile is larger than image, just sample all pixels
        if self.tile_size >= max(self.width, self.height):
            self._shuffle_pixel_index_large_tiles(seed)
        else:
            self._shuffle_pixel_index_normal_tiles(tile_permutation)

    def _shuffle_pixel_index_large_tiles(self, seed: mi.UInt32):
        """Handle large tiles that cover entire image."""
        all_pixel_indices = dr.arange(mi.UInt32, self.ttl_pixels)
        permuted_pixels = mi.permute_kensler(
            all_pixel_indices, self.ttl_pixels, seed
        )
        self.perm_pixel_index_buffer = self._create_padded_buffer(permuted_pixels)

    def _shuffle_pixel_index_normal_tiles(self, tile_permutation):
        """Handle normal tile-based sampling with proper boundary coverage."""
        pixels_per_tile = self.tile_size * self.tile_size
        num_tiles_to_sample = (
            (self.effective_ttl_pixels + pixels_per_tile - 1) //
            pixels_per_tile
        )
        total_tiles = self.num_sensors * self.tiles_per_sensor
        num_tiles_to_sample = min(num_tiles_to_sample, total_tiles)

        # Get permuted tile indices for sampling
        sampled_tile_indices = dr.arange(mi.UInt32, num_tiles_to_sample)
        permuted_sampled_indices = dr.gather(
            mi.UInt32, tile_permutation, sampled_tile_indices % total_tiles
        )

        # Convert to sensor and tile coordinates
        sensor_indices = permuted_sampled_indices // self.tiles_per_sensor
        local_tile_indices = (
            permuted_sampled_indices % self.tiles_per_sensor
        )
        tile_rows = local_tile_indices // self.tiles_per_row
        tile_cols = local_tile_indices % self.tiles_per_row

        # Compute tile bounds (top-left corner approach for full coverage)
        tile_start_y = tile_rows * self.tile_size
        tile_start_x = tile_cols * self.tile_size
        tile_end_y = dr.minimum(tile_start_y + self.tile_size, self.height)
        tile_end_x = dr.minimum(tile_start_x + self.tile_size, self.width)

        # Generate all pixels for all tiles using DrJit broadcasts
        # Create offset grids for tile_size x tile_size
        tile_y_offsets = dr.arange(mi.UInt32, self.tile_size)
        tile_x_offsets = dr.arange(mi.UInt32, self.tile_size)

        # Generate coordinates for all pixels in all tiles
        total_pixels_generated = num_tiles_to_sample * pixels_per_tile

        # Expand tile starts and sensor indices to match all pixels
        expanded_tile_start_y = dr.repeat(tile_start_y, pixels_per_tile)
        expanded_tile_start_x = dr.repeat(tile_start_x, pixels_per_tile)
        expanded_tile_end_y = dr.repeat(tile_end_y, pixels_per_tile)
        expanded_tile_end_x = dr.repeat(tile_end_x, pixels_per_tile)
        expanded_sensor_indices = dr.repeat(sensor_indices, pixels_per_tile)

        # Create tile offset pattern repeated for all tiles
        tile_offsets_y = dr.tile(
            dr.repeat(tile_y_offsets, self.tile_size), num_tiles_to_sample
        )
        tile_offsets_x = dr.tile(
            dr.tile(tile_x_offsets, self.tile_size), num_tiles_to_sample
        )

        # Compute final pixel coordinates
        final_y = expanded_tile_start_y + tile_offsets_y
        final_x = expanded_tile_start_x + tile_offsets_x

        # Clamp to tile bounds (for boundary tiles)
        final_y = dr.minimum(final_y, expanded_tile_end_y - 1)
        final_x = dr.minimum(final_x, expanded_tile_end_x - 1)

        # Convert to flat pixel indices
        sensor_offsets = expanded_sensor_indices * (
            self.width * self.height
        )
        all_pixel_indices = (
            sensor_offsets + final_y * self.width + final_x
        )

        # Pad if necessary
        self.perm_pixel_index_buffer = self._create_padded_buffer(all_pixel_indices)

        # Create the corresponding image reference tensor (same for both tile and pixel modes)
        ttl_size = self.ttl_pixels * self.channel_size
        # Unpermuted target image buffer
        target_image_flat = dr.empty(mi.Float, ttl_size)
        chunk_size = self.width * self.height * self.channel_size
        for i in range(self.num_sensors):
            dr.scatter(
                target_image_flat,
                dr.ravel(self.target_images[i]),
                mi.UInt32(dr.arange(mi.UInt32, chunk_size) + i * chunk_size),
            )
        # Permuted target image buffer
        self.perm_target_image_flat = dr.ravel(dr.gather(
            mi.ArrayXf,
            target_image_flat,
            self.perm_pixel_index_buffer,
            shape=(self.channel_size, self.effective_ttl_pixels)
        ))

        # Make sure the buffer is in memory
        del target_image_flat
        dr.eval(self.perm_pixel_index_buffer, self.perm_target_image_flat)

    def next(self):
        """Get the next batch of pixel indices and corresponding target tensor.

        This method reshuffles the pixel index buffer every `iter_shuffle`
        iterations.
        """
        counter = dr.opaque(mi.UInt32, self.counter)

        # Potentially reshuffle the pixel index buffer
        if (self.counter == 0) or (
            self.regular_reshuffle and self.counter % self.iter_shuffle == 0
        ):
            self.shuffle_pixel_index(self.seed + counter)

        # Gather the pixel indices for the current batch
        batch_indices = (
            dr.arange(mi.UInt32, self.pixels_per_batch) +
            (counter % self.iter_shuffle) * self.pixels_per_batch
        )
        self.flat_sensor.pixel_idx = dr.gather(
            mi.UInt32, self.perm_pixel_index_buffer, batch_indices)

        # Gather the target tensor for the current batch
        target_data = dr.ravel(dr.gather(
            mi.ArrayXf, self.perm_target_image_flat, batch_indices,
            shape=(self.channel_size, self.pixels_per_batch)
        ))
        target_tensor = mi.TensorXf(
            target_data,
            shape=(1, self.pixels_per_batch, self.channel_size)
        )

        self.counter += 1
        return target_tensor, self.flat_sensor
