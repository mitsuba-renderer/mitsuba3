from __future__ import annotations as __annotations__ # Delayed parsing of type annotations

from typing import Callable, Tuple

import mitsuba as mi
import drjit as dr

class flat_sensor(mi.Sensor):
    def __init__(self, props):
        mi.Sensor.__init__(self, props)

        # To be updated by RayLoader
        self.pixel_idx = dr.zeros(mi.UInt32, 1)
        self.num_sensors = 0
        self.width = 0
        self.height = 0

    def initialize(self, sensors: list[mi.Sensor]) -> None:
        """
        """
        self.num_sensors = len(sensors)

        # Assert that they have the same film size
        film_size = sensors[0].film().size()
        self.width = film_size[0]
        self.height = film_size[1]
        for i in range(self.num_sensors):
            film_size = sensors[i].film().size()
            if (film_size[0] != self.width or film_size[1] != self.height):
                raise ValueError(
                    f"Sensor {i} has different film size: "
                    f"{film_size[0]}x{film_size[1]} "
                    f"vs {self.width}x{self.height}"
                )

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
            spp = 1
        pixel_idx = dr.repeat(self.pixel_idx, spp)
        sensor_idx = pixel_idx // (self.width * self.height)

        # Convert flat pixel index to 2D coordinates within the target sensor
        pixel_idx_in_sensor = pixel_idx % (self.width * self.height)
        target_pixel_y = mi.Float(pixel_idx_in_sensor // self.width)
        target_pixel_x = mi.Float(pixel_idx_in_sensor % self.width)

        # `sample2` is the normalized, jittered sample for the flat_sensor's film.
        # `sample2.x` = (pixel_in_batch + rand.x) / pixels_per_batch
        # `sample2.y` = (0 + rand.y) / 1 = rand.y
        # We only need the random jitter part, which is sample2.y for the y-axis.
        # For the x-axis, the integer pixel location is encoded, so we can't
        # simply extract the jitter.
        #
        # However, the simplest correct approach is to realize that `sample2`
        # already contains the sub-pixel jitter we need to preserve.
        # The `sampler.next_2d()` call in `sample_rays` provides the jitter.
        # We just need to map it to the correct pixel block in the target sensor.

        # The random jitter from the sampler is contained in `sample2`.
        # `sample2.x` contains jitter for the x-axis.
        # `sample2.y` contains jitter for the y-axis.
        # We add the integer coordinates of the target pixel and re-normalize.
        sample2_override = mi.Point2f(
            (target_pixel_x + sample2.x) / mi.Float(self.width),
            (target_pixel_y + sample2.y) / mi.Float(self.height)
        )

        sensors_ptr = dr.gather(
            mi.SensorPtr, self.sensors_unique_ptr, sensor_idx
        )

        # This is a VCall
        with dr.scoped_set_flag(dr.JitFlag.SymbolicCalls, False):
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
        return self.sample_ray_differential(time, sample1, sample2, sample3, active)

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
        scene: mi.Scene,
        sensors: list[mi.Sensor] | mi.Sensor,
        target_images: list[mi.TensorXf] | mi.TensorXf,
        pixels_per_batch: int = 8192,
        seed: int = 0,
        regular_reshuffle: bool = False,
    ) -> None:
        """
        TODO
        """

        if type(target_images) != list:
            target_images = [target_images]
        if type(sensors) != list:
            sensors = [sensors]

        self.channel_size = target_images[0].shape[-1]

        # Determine pixel format and component format
        reference_film = sensors[0].film()
        # Map channels to pixel format
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
        component_format = 'float32'  # TODO: float16?

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
        self.flat_sensor.initialize(sensors)

        # Initialize pixel_idx with the correct size
        self.flat_sensor.pixel_idx = dr.zeros(mi.UInt32, pixels_per_batch)

        self.scene = scene
        self.num_sensors = len(sensors)
        self.target_images = target_images
        self.pixels_per_batch = pixels_per_batch
        self.seed = seed

        # Calculate total pixels from the actual sensor dimensions, not flat_sensor
        self.width = sensors[0].film().size()[0]
        self.height = sensors[0].film().size()[1]
        self.ttl_pixels = self.num_sensors * self.width * self.height

        if regular_reshuffle:
            # Calculate how many iterations to do a full shuffle cycle
            effective_ttl_pixels = ((self.ttl_pixels + self.pixels_per_batch - 1) //
                                   self.pixels_per_batch) * self.pixels_per_batch
            self.iter_shuffle = effective_ttl_pixels // self.pixels_per_batch
        else:
            self.iter_shuffle = mi.UInt32(-1)
        self.counter = 0

    def shuffle_pixel_index(self, seed: int):
        # Handle the case where total pixels is not divisible by pixels_per_batch
        # by padding with random indices (some pixels may appear twice)
        effective_ttl_pixels = ((self.ttl_pixels + self.pixels_per_batch - 1) //
                               self.pixels_per_batch) * self.pixels_per_batch

        # Shuffle the pixel index buffer once using mi.permute_kensler
        if effective_ttl_pixels > self.ttl_pixels:
            # Pad with random indices from the valid range
            padding_size = effective_ttl_pixels - self.ttl_pixels
            base_indices = dr.arange(mi.UInt32, self.ttl_pixels)
            # Use random indices for padding (with replacement)
            random_indices = (dr.arange(mi.UInt32, padding_size) %
                              mi.UInt32(self.ttl_pixels))
            all_indices = dr.concat(base_indices, random_indices)
        else:
            all_indices = dr.arange(mi.UInt32, effective_ttl_pixels)

        self.perm_pixel_index_buffer = mi.permute_kensler(
            all_indices,
            effective_ttl_pixels,
            seed,
        )

        # Create the corresponding image reference tensor
        # Flatten target images in sensor-major order
        target_image_flat = dr.empty(mi.Float, self.ttl_pixels * self.channel_size)

        offset = 0
        for i in range(self.num_sensors):
            # Ravel each target image tensor and fill into flat buffer
            image_data = dr.ravel(self.target_images[i])
            pixels_per_image = self.width * self.height

            # Copy image data to the flat buffer using direct array copy
            start_idx = offset * self.channel_size
            end_idx = start_idx + pixels_per_image * self.channel_size
            target_indices = dr.arange(mi.UInt32, pixels_per_image * self.channel_size) + start_idx
            source_indices = dr.arange(mi.UInt32, pixels_per_image * self.channel_size)

            data_chunk = dr.gather(mi.Float, image_data, source_indices)
            dr.scatter(target_image_flat, data_chunk, target_indices)

            offset += pixels_per_image

        print(f'target_image_flat = {target_image_flat}')

        # Create permuted target image buffer using efficient gather with shape
        # Clamp indices to valid range to handle padding
        valid_mask = self.perm_pixel_index_buffer < mi.UInt32(self.ttl_pixels)
        clamped_indices = dr.select(valid_mask, self.perm_pixel_index_buffer, mi.UInt32(0))

        self.perm_target_image_buffer = dr.ravel(dr.gather(
            mi.ArrayXf, target_image_flat, clamped_indices,
            shape=(self.channel_size, effective_ttl_pixels)
        ))

        print(f'perm_target_image_buffer = {self.perm_target_image_buffer}')

        # Make sure both buffers are in memory
        dr.eval(self.perm_pixel_index_buffer, self.perm_target_image_buffer)

    def next(self):
        # Potentially reshuffle the pixel index buffer
        if self.counter % self.iter_shuffle == 0:
            self.shuffle_pixel_index(self.seed + self.counter)

        # Calculate the start index for this batch
        batch_start = (self.counter % self.iter_shuffle) * self.pixels_per_batch
        batch_indices = dr.arange(mi.UInt32, self.pixels_per_batch) + batch_start

        # Get pixel indices for this batch
        current_pixel_indices = dr.gather(mi.UInt32, self.perm_pixel_index_buffer, batch_indices)

        # Put this round's pixel index to flat_sensor
        dr.scatter(self.flat_sensor.pixel_idx, current_pixel_indices,
                  dr.arange(mi.UInt32, self.pixels_per_batch))

        # Gather reference tensor for the selected pixels using efficient gather
        # Use dr.gather with shape parameter for multi-channel data
        target_data = dr.ravel(dr.gather(
            mi.ArrayXf, self.perm_target_image_buffer, batch_indices,
            shape=(self.channel_size, self.pixels_per_batch)
        ))
        target_tensor = mi.TensorXf(
            target_data,
            shape=(1, self.pixels_per_batch, self.channel_size)
        )
        # print(f'dr.ravel(target_tensor) = {dr.ravel(target_tensor)}')

        self.counter += 1
        return target_tensor, self.flat_sensor
