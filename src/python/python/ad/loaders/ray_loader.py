from __future__ import annotations as __annotations__ # Delayed parsing of type annotations

from typing import Tuple

import mitsuba as mi
import drjit as dr

# TODO: Support ptracer & projective
# TODO: fp16

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
            return dr.zeros(mi.RayDifferential3f), dr.zeros(mi.Spectrum)

        # Compute the corresponding sensor index
        pixel_idx = dr.repeat(self.pixel_idx, spp)
        sensor_idx = pixel_idx // (self.source_film_width * self.source_film_height)

        # Convert flat pixel index to 2D coordinates within the target sensor
        pixel_idx_in_sensor = pixel_idx % (self.source_film_width * self.source_film_height)
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
        sensors: list[mi.Sensor] | mi.Sensor,
        target_images: list[mi.TensorXf] | mi.TensorXf,
        pixels_per_batch: int,
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
        self.ttl_pixels = self.num_sensors * self.width * self.height

        # Assert that pixels_per_batch is smaller than ttl_pixels
        if pixels_per_batch <= 0 or pixels_per_batch > self.ttl_pixels:
            raise ValueError(
                f"pixels_per_batch ({pixels_per_batch}) must be "
                f"greater than 0 and less than or equal to ttl_pixels ({self.ttl_pixels})"
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

        self.target_images = target_images
        self.pixels_per_batch = pixels_per_batch
        self.seed = seed
        self.regular_reshuffle = regular_reshuffle
        self.counter = 0

        # Handle the case where total pixels is not divisible by pixels_per_batch
        # by padding with random indices (some pixels may appear twice)
        self.effective_ttl_pixels = (
            ((self.ttl_pixels + self.pixels_per_batch - 1) //
            self.pixels_per_batch) * self.pixels_per_batch
        )

        # Calculate how many iterations to do a full shuffle cycle
        self.iter_shuffle = (
            self.effective_ttl_pixels // self.pixels_per_batch
        )

        # Calculate the pixel batch multiplier
        self.pixel_batch_multiplier = float(self.ttl_pixels / pixels_per_batch)

    def shuffle_pixel_index(self, seed: mi.UInt32):
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
        """
        Get the next batch of pixel indices and corresponding target tensor.
        This method reshuffles the pixel index buffer every `iter_shuffle` iterations.
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
