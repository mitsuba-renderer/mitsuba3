from __future__ import annotations as __annotations__ # Delayed parsing of type annotations

from typing import Tuple

import mitsuba as mi
import drjit as dr


class FlatSensor(mi.Sensor):
    """
    Sensor used internally by :py:class:`~mitsuba.ad.RayDataLoader`.

    Its film has one row with ``pixels_per_batch`` pixels. Before each render
    call, :py:meth:`~mitsuba.ad.RayDataLoader.next()` stores the selected flat
    source pixel indices in ``pixel_idx``. Sampling this sensor then remaps
    those flat indices to the corresponding source sensor and pixel
    coordinates.
    """

    def __init__(self, props):
        mi.Sensor.__init__(self, props)

        # To be updated by RayDataLoader
        self.pixel_idx = dr.zeros(mi.UInt32, 1)
        self.num_sensors = 0
        self.source_film_width = 0
        self.source_film_height = 0
        self.pixels_per_batch = 0

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
        self.pixel_idx = dr.zeros(mi.UInt32, pixels_per_batch)

        def make_sample_ray_differential_target(sensor: mi.Sensor):
            def sample_ray_differential(
                time: mi.Float,
                sample1: mi.Float,
                sample2: mi.Point2f,
                sample3: mi.Point2f,
                active: mi.Bool = True,
            ) -> Tuple[mi.RayDifferential3f, mi.Spectrum]:
                return sensor.sample_ray_differential(
                    time, sample1, sample2, sample3, active
                )

            return sample_ray_differential

        # Use dr.switch to limit symbolic tracing to the known child sensors.
        self.sensor_sample_ray_differential = [
            make_sample_ray_differential_target(sensor)
            for sensor in sensors
        ]

    def sample_ray_differential(
        self,
        time: mi.Float,
        sample1: mi.Float,
        sample2: mi.Point2f,
        sample3: mi.Point2f,
        active: mi.Bool = True,
    ) -> Tuple[mi.RayDifferential3f, mi.Spectrum]:

        spp = dr.width(sample2) // dr.width(self.pixel_idx)

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
        rays, weights = dr.switch(
            sensor_idx,
            self.sensor_sample_ray_differential,
            time,
            sample1,
            sample2_override,
            sample3,
            active,
            label="RayDataLoader.sample_ray_differential",
        )

        return rays, weights

    def sample_ray(
        self,
        time: mi.Float,
        sample1: mi.Float,
        sample2: mi.Point2f,
        sample3: mi.Point2f,
        active: mi.Bool = True,
    ) -> Tuple[mi.Ray3f, mi.Spectrum]:
        ray, weight = self.sample_ray_differential(
            time, sample1, sample2, sample3, active)
        return mi.Ray3f(ray), weight

    def to_string(self):
        return ('FlatSensor[\n'
                '    num_sensors=%d,\n'
                '    width=%d,\n'
                '    height=%d,\n'
                ']' % (self.num_sensors,
                       self.source_film_width,
                       self.source_film_height))

mi.register_sensor("flat_sensor",
                   lambda props: FlatSensor(props))


class RayDataLoader:
    """
    Minibatch loader for rendering rays and matching target pixels.

    This class treats every pixel of every source sensor as one flattened sample.
    Calling :py:meth:`next()` returns a target tensor with shape
    ``(1, pixels_per_batch, channels)`` together with an internal sensor that is
    configured to render the same flattened pixels.

    The loader exposes several scalar layout members: ``num_sensors``,
    ``width``, ``height``, ``channel_size``, ``total_pixels``,
    ``effective_total_pixels``, ``iter_shuffle``, and tile counts
    (``tiles_per_row``, ``tiles_per_col``, ``tiles_per_sensor``). The member
    ``pixel_batch_multiplier`` is equal to ``total_pixels / pixels_per_batch``
    and can be used to scale minibatch losses to full-image magnitude.
    """

    def __init__(
        self,
        sensors: list[mi.Sensor] | mi.Sensor,
        target_images: list[mi.TensorXf] | mi.TensorXf,
        pixels_per_batch: int,
        seed: int = 0,
        regular_reshuffle: bool = False,
        tile_size: int = 4,
    ) -> None:
        """
        Initialize the ray data loader.

        Parameter ``sensors`` (``mi.Sensor`` or ``list[mi.Sensor]``):
            Source sensor(s). They must share a full film size, have no crop
            window, disable ``sample_border``, and use the same number of base
            channels as the target images.

        Parameter ``target_images`` (``mi.TensorXf`` or ``list[mi.TensorXf]``):
            Target image tensor(s), one per sensor. Each tensor must have shape
            ``(height, width, channels)`` matching the corresponding sensor
            film.

        Parameter ``pixels_per_batch`` (``int``):
            Number of flattened pixels returned by each
            :py:meth:`~mitsuba.ad.RayDataLoader.next()` call. It must be
            positive and no larger than ``total_pixels``.

        Parameter ``seed`` (``int``):
            Base seed used for deterministic pixel permutation.

        Parameter ``regular_reshuffle`` (``bool``):
            When enabled, reshuffle at the beginning of every full pass through
            the padded pixel set. Otherwise, reuse the initial permutation.

        Parameter ``tile_size`` (``int``):
            Side length of square tiles used for pixel shuffling. A value of
            ``1`` gives a fully random pixel permutation, intermediate values
            improve spatial coherence, and values at least as large as the
            image dimensions fall back to whole-image permutation.
        """

        # Validate tile_size parameter
        if tile_size <= 0:
            raise ValueError(f"tile_size ({tile_size}) must be positive")

        if isinstance(target_images, tuple):
            target_images = list(target_images)
        elif not isinstance(target_images, list):
            target_images = [target_images]
        if isinstance(sensors, tuple):
            sensors = list(sensors)
        elif not isinstance(sensors, list):
            sensors = [sensors]

        if not sensors:
            raise ValueError("At least one sensor must be provided")
        if not target_images:
            raise ValueError("At least one target image must be provided")

        reference_shape = target_images[0].shape
        if len(reference_shape) != 3:
            raise ValueError(
                "Target images must have shape (height, width, channels)"
            )

        reference_film = sensors[0].film()
        self.num_sensors = len(sensors)
        self.width = reference_film.size()[0]
        self.height = reference_film.size()[1]
        self.channel_size = reference_shape[2]
        self.total_pixels = self.num_sensors * self.width * self.height

        # Tile-related properties
        self.tile_size = tile_size
        self.tiles_per_row = (self.width + tile_size - 1) // tile_size
        self.tiles_per_col = (self.height + tile_size - 1) // tile_size
        self.tiles_per_sensor = self.tiles_per_row * self.tiles_per_col

        # Validate that pixels_per_batch is valid
        if pixels_per_batch <= 0 or pixels_per_batch > self.total_pixels:
            raise ValueError(
                f"pixels_per_batch ({pixels_per_batch}) must be "
                f"greater than 0 and less than or equal to "
                f"total_pixels ({self.total_pixels})"
            )
        # Validate that all sensors and target_images have the same size
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
            crop_size = film.crop_size()
            crop_offset = film.crop_offset()
            if (crop_size[0] != film_size[0] or
                crop_size[1] != film_size[1] or
                crop_offset[0] != 0 or
                crop_offset[1] != 0):
                raise ValueError(
                    f"Sensor {i} uses a crop window, which is not supported "
                    "by RayDataLoader."
                )
            if film.sample_border():
                raise ValueError(
                    f"Sensor {i} has sample_border enabled, "
                    "which is not supported by RayDataLoader."
                )
            tensor_shape = target_images[i].shape
            if len(tensor_shape) != 3:
                raise ValueError(
                    f"Target image {i} must have shape "
                    "(height, width, channels)"
                )
            if (tensor_shape[0] != self.height or
                tensor_shape[1] != self.width):
                raise ValueError(
                    f"Target image {i} has different shape: "
                    f"{tensor_shape[0]}x{tensor_shape[1]}x{tensor_shape[2]} "
                    f"vs {self.height}x{self.width}x{self.channel_size}"
                )
            if tensor_shape[2] != self.channel_size:
                raise ValueError(
                    f"Target image {i} has {tensor_shape[2]} channels, "
                    f"expected {self.channel_size}"
                )
            film_channels = film.base_channels_count()
            if film_channels != self.channel_size:
                raise ValueError(
                    f"Sensor {i} film has {film_channels} channels, "
                    f"expected {self.channel_size}"
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
        # float16 is not supported
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

        self.target_images = target_images
        self.pixels_per_batch = pixels_per_batch
        self.seed = seed
        self.regular_reshuffle = regular_reshuffle
        self.counter = 0
        self.perm_target_image_flat = None

        # Handle case where total pixels is not divisible by pixels_per_batch
        # by padding with repeated indices
        self.effective_total_pixels = (
            ((self.total_pixels + self.pixels_per_batch - 1) //
            self.pixels_per_batch) * self.pixels_per_batch
        )

        # Calculate how many iterations to do a shuffle cycle
        self.iter_shuffle = (
            self.effective_total_pixels // self.pixels_per_batch
        )

        # Calculate the pixel batch multiplier
        self.pixel_batch_multiplier = float(self.total_pixels / pixels_per_batch)

        self._initialize_target_image_buffer()
        if 1 < self.tile_size < max(self.width, self.height):
            self._initialize_tile_pixel_table()

    def _initialize_target_image_buffer(self):
        """Initialize the flattened target image buffer for efficient gathering."""
        total_size = self.total_pixels * self.channel_size
        self.target_image_flat = dr.empty(mi.Float, total_size)
        chunk_size = self.width * self.height * self.channel_size
        for i in range(self.num_sensors):
            dr.scatter(
                self.target_image_flat,
                dr.ravel(self.target_images[i]),
                mi.UInt32(dr.arange(mi.UInt32, chunk_size) + i * chunk_size),
            )
        dr.eval(self.target_image_flat)

    def _update_target_image_buffer(self):
        """Gather the target image buffer in the current pixel order."""
        # Permuted target image buffer
        self.perm_target_image_flat = dr.ravel(dr.gather(
            mi.ArrayXf,
            self.target_image_flat,
            self.perm_pixel_index_buffer,
            shape=(self.channel_size, self.effective_total_pixels)
        ))

        # Make sure the buffer is in memory
        dr.eval(self.perm_pixel_index_buffer, self.perm_target_image_flat)

    def _initialize_tile_pixel_table(self):
        """Create a padded table of valid pixels for every tile."""
        pixels_per_tile = self.tile_size * self.tile_size
        total_tiles = self.num_sensors * self.tiles_per_sensor

        tile_indices = dr.arange(mi.UInt32, total_tiles)
        sensor_indices = tile_indices // self.tiles_per_sensor
        local_tile_indices = tile_indices % self.tiles_per_sensor
        tile_rows = local_tile_indices // self.tiles_per_row
        tile_cols = local_tile_indices % self.tiles_per_row

        tile_start_y = tile_rows * self.tile_size
        tile_start_x = tile_cols * self.tile_size
        tile_end_y = dr.minimum(tile_start_y + self.tile_size, self.height)
        tile_end_x = dr.minimum(tile_start_x + self.tile_size, self.width)

        tile_y_offsets = dr.arange(mi.UInt32, self.tile_size)
        tile_x_offsets = dr.arange(mi.UInt32, self.tile_size)

        expanded_tile_start_y = dr.repeat(tile_start_y, pixels_per_tile)
        expanded_tile_start_x = dr.repeat(tile_start_x, pixels_per_tile)
        expanded_tile_end_y = dr.repeat(tile_end_y, pixels_per_tile)
        expanded_tile_end_x = dr.repeat(tile_end_x, pixels_per_tile)
        expanded_sensor_indices = dr.repeat(sensor_indices, pixels_per_tile)

        tile_offsets_y = dr.tile(
            dr.repeat(tile_y_offsets, self.tile_size), total_tiles
        )
        tile_offsets_x = dr.tile(
            dr.tile(tile_x_offsets, self.tile_size), total_tiles
        )

        final_y = expanded_tile_start_y + tile_offsets_y
        final_x = expanded_tile_start_x + tile_offsets_x
        valid = (final_y < expanded_tile_end_y) & (final_x < expanded_tile_end_x)

        pixel_indices = (
            expanded_sensor_indices * (self.width * self.height) +
            final_y * self.width + final_x
        )
        self.tile_pixel_table = dr.select(valid, pixel_indices, self.total_pixels)
        dr.eval(self.tile_pixel_table)

    def _create_padded_buffer(self, pixel_indices):
        """
        Create a padded buffer from pixel indices to fit
        ``effective_total_pixels``.

        Parameter ``pixel_indices`` (``mi.UInt32``):
            Dr.Jit array containing pixel indices.

        Returns a padded buffer of size ``effective_total_pixels``.
        """
        actual_count = len(pixel_indices)
        buffer_indices = dr.arange(mi.UInt32, self.effective_total_pixels)
        source_indices = buffer_indices % actual_count
        return dr.gather(mi.UInt32, pixel_indices, source_indices)

    def shuffle_pixel_index(self, seed: mi.UInt32):
        """Shuffle pixel index buffer using tile-based permutation for GPU coherence."""
        if self.tile_size == 1:
            # Pixel-based permutation
            self.perm_pixel_index_buffer = mi.permute_kensler(
                dr.arange(mi.UInt32, self.effective_total_pixels) %
                self.total_pixels,
                self.effective_total_pixels,
                seed,
            )
        else:
            self._shuffle_pixel_index_tiled(seed)

        # Update the target image buffer after shuffling
        self._update_target_image_buffer()

    def _shuffle_pixel_index_tiled(self, seed: mi.UInt32):
        """Tile-based permutation for GPU coherence."""
        # Special case: if tile is larger than image, just sample all pixels
        if self.tile_size >= max(self.width, self.height):
            self._shuffle_pixel_index_large_tiles(seed)
            return

        total_tiles = self.num_sensors * self.tiles_per_sensor
        tile_permutation = mi.permute_kensler(
            dr.arange(mi.UInt32, total_tiles),
            total_tiles,
            seed,
        )
        self._shuffle_pixel_index_normal_tiles(tile_permutation)

    def _shuffle_pixel_index_large_tiles(self, seed: mi.UInt32):
        """Handle large tiles that cover entire image."""
        all_pixel_indices = dr.arange(mi.UInt32, self.total_pixels)
        permuted_pixels = mi.permute_kensler(
            all_pixel_indices, self.total_pixels, seed
        )
        self.perm_pixel_index_buffer = self._create_padded_buffer(permuted_pixels)

    def _shuffle_pixel_index_normal_tiles(self, tile_permutation):
        """Handle normal tile-based sampling with proper boundary coverage."""
        pixels_per_tile = self.tile_size * self.tile_size
        total_tiles = self.num_sensors * self.tiles_per_sensor
        table_size = total_tiles * pixels_per_tile
        table_indices = dr.arange(mi.UInt32, table_size)
        tile_slots = table_indices // pixels_per_tile
        tile_offsets = table_indices % pixels_per_tile
        permuted_tiles = dr.gather(mi.UInt32, tile_permutation, tile_slots)
        permuted_table_indices = permuted_tiles * pixels_per_tile + tile_offsets

        padded_pixel_indices = dr.gather(
            mi.UInt32, self.tile_pixel_table, permuted_table_indices
        )
        valid_indices = dr.compress(padded_pixel_indices < self.total_pixels)
        all_pixel_indices = dr.gather(
            mi.UInt32, padded_pixel_indices, valid_indices
        )

        # Pad if necessary
        self.perm_pixel_index_buffer = self._create_padded_buffer(all_pixel_indices)

    def next(self):
        """Get the next batch of pixel indices and corresponding target tensor.

        This method reshuffles the pixel index buffer every `iter_shuffle`
        iterations. It reuses and mutates the same flat sensor on every call.
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
