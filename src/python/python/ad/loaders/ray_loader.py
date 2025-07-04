from __future__ import annotations as __annotations__ # Delayed parsing of type annotations

from typing import Callable, Tuple

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
        """
        Initialize the flat sensor with multiple sensors and a target pixels
        per batch.
        """
        self.num_sensors = len(sensors)
        film_size = sensors[0].film().size()
        self.real_film_width = film_size[0]
        self.real_film_height = film_size[1]
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
            spp = 1
        pixel_idx = dr.repeat(self.pixel_idx, spp)
        sensor_idx = pixel_idx // (self.real_film_width * self.real_film_height)

        # Convert flat pixel index to 2D coordinates within the target sensor
        pixel_idx_in_sensor = pixel_idx % (self.real_film_width * self.real_film_height)
        target_pixel_y = mi.Float(pixel_idx_in_sensor // self.real_film_width)
        target_pixel_x = mi.Float(pixel_idx_in_sensor % self.real_film_width)

        # The flat sensor has film size (pixels_per_batch, 1)
        # sample2.x is normalized to [0, 1] for the flat sensor's width
        # sample2.y is normalized to [0, 1] for the flat sensor's height (which is 1)

        # Extract the batch position and jitter from sample2.x
        # sample2.x contains: (batch_pixel_position + jitter_x) / pixels_per_batch
        batch_pixel_pos = dr.floor(sample2.x * mi.Float(self.pixels_per_batch))
        jitter_x = sample2.x * mi.Float(self.pixels_per_batch) - batch_pixel_pos

        # sample2.y is already the jitter for the y dimension (since flat sensor height is 1)
        jitter_y = sample2.y

        # Create the remapped sample coordinates for the target sensor
        sample2_override = mi.Point2f(
            (target_pixel_x + jitter_x) / mi.Float(self.real_film_width),
            (target_pixel_y + jitter_y) / mi.Float(self.real_film_height)
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
        Rayloader for efficient batch rendering with multiple sensors and target images.
        """

        if type(target_images) != list:
            target_images = [target_images]
        if type(sensors) != list:
            sensors = [sensors]

        reference_film = sensors[0].film()
        self.num_sensors = len(sensors)
        self.width = reference_film.size()[0]
        self.height = reference_film.size()[1]
        self.channel_size = target_images[0].shape[-1]

        # Assert that all sensors and target_images have the same size
        if len(sensors) != len(target_images):
            raise ValueError(
                f"Number of sensors ({len(sensors)}) does not match "
                f"number of target images ({len(target_images)})"
            )
        for i in range(self.num_sensors):
            film_size = sensors[i].film().size()
            if (film_size[0] != self.width or film_size[1] != self.height):
                raise ValueError(
                    f"Sensor {i} has different film size: "
                    f"{film_size[0]}x{film_size[1]} "
                    f"vs {self.width}x{self.height}"
                )
            tensor_shape = target_images[i].shape
            if (tensor_shape[0] != self.height or
                tensor_shape[1] != self.width or
                tensor_shape[2] != sensors[i].film().base_channels_count()):
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
        component_format = 'float32'  # TODO: float16 not supported

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

        # Dummy initialization of pixel_idx, to be updated by `next()`
        self.flat_sensor.pixel_idx = None
        self.perm_target_image_flat = None

        self.scene = scene
        self.target_images = target_images
        self.pixels_per_batch = pixels_per_batch
        self.seed = seed
        self.ttl_pixels = self.num_sensors * self.width * self.height
        self.counter = 0

        # Handle the case where total pixels is not divisible by pixels_per_batch
        # by padding with random indices (some pixels may appear twice)
        self.effective_ttl_pixels = (
            ((self.ttl_pixels + self.pixels_per_batch - 1) //
            self.pixels_per_batch) * self.pixels_per_batch
        )

        if regular_reshuffle:
            # Calculate how many iterations to do a full shuffle cycle
            self.iter_shuffle = (
                self.effective_ttl_pixels // self.pixels_per_batch
            )
        else:
            self.iter_shuffle = mi.UInt32(-1)

    def shuffle_pixel_index(self, seed: int):
        """
        Shuffle the pixel index buffer to randomize the order of pixels
        for the next batch of rendering.
        """
        # Shuffle the pixel index buffer
        self.perm_pixel_index_buffer = mi.permute_kensler(
            dr.arange(mi.UInt32, self.effective_ttl_pixels) % self.ttl_pixels,
            self.effective_ttl_pixels,
            seed,
        )
        # print(f"self.perm_pixel_index_buffer: {self.perm_pixel_index_buffer}")

        # Create the corresponding image reference tensor
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
        # print(f"target_image_flat: {target_image_flat}")
        # Permuted target image buffer
        self.perm_target_image_flat = dr.ravel(dr.gather(
            mi.ArrayXf,
            target_image_flat,
            self.perm_pixel_index_buffer,
            shape=(self.channel_size, self.effective_ttl_pixels)
        ))
        # print(f"self.perm_target_image_flat: {self.perm_target_image_flat}")

        # Make sure the buffer is in memory
        del target_image_flat
        dr.eval(self.perm_pixel_index_buffer, self.perm_target_image_flat)

    def next(self):
        """
        Get the next batch of pixel indices and corresponding target tensor.
        This method reshuffles the pixel index buffer every `iter_shuffle` iterations.
        """
        # Potentially reshuffle the pixel index buffer
        if self.counter % self.iter_shuffle == 0:
            self.shuffle_pixel_index(self.seed + self.counter)

        # Calculate the start index for this batch
        batch_start = (self.counter % self.iter_shuffle) * self.pixels_per_batch
        batch_indices = dr.arange(mi.UInt32, self.pixels_per_batch) + batch_start

        # Get pixel indices for this batch
        current_pixel_indices = dr.gather(
            mi.UInt32, self.perm_pixel_index_buffer, batch_indices)

        # Put this round's pixel index to flat_sensor
        self.flat_sensor.pixel_idx = current_pixel_indices

        # Gather reference tensor for the selected pixels using efficient gather
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
