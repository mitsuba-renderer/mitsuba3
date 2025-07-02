from __future__ import annotations
from typing import Callable, Tuple

import mitsuba as mi
import drjit as dr

class custom_sensor(mi.Sensor):
    def __init__(self, props):
        mi.Sensor.__init__(self, props)

        self.rays = dr.zeros(mi.Ray3f)
        self.weight = mi.Float(0)
        self.dummy_film = None

    def sample_ray_differential(self,
                                time,   # mi.Float
                                sample1,  # wavelength_sample,  # mi.Float
                                sample2,  # position_sample,
                                sample3,  # aperture_sample,
                                mask: mi.Bool = True):
        return mi.RayDifferential3f(self.rays), self.weight

    def sample_ray(self,
                   time,   # mi.Float
                   wavelength_sample,  # mi.Float
                   position_sample,
                   aperture_sample,
                   mask: mi.Bool = True):

        return self.rays, self.weight

    def film(self):
        return self.dummy_film

    def to_string(self):
        return ('Custom_sensor[\n'
                '    ray=%s,\n'
                '    weight=%s,\n'
                ']' % (self.rays, self.weight))

mi.register_sensor("custom_sensor",
                   lambda props: custom_sensor(props))

class Rayloader():
    def __init__(self, scene, sensors, image_refs, batch_size, spp):
        self.scene = scene
        self.scene_params = mi.traverse(scene)
        self.dummysensor = mi.load_dict({
            'type': 'custom_sensor'
        })
        self.sensors = sensors
        self.iteration = 0
        self.batch_size = batch_size
        self.permute_seed = 2
        self.num_sensors = len(sensors)
        self.image_refs = image_refs
        if type(image_refs) != list:
            self.image_refs = [image_refs]

        self.channel_size = self.image_refs[0].shape[2]

        # TODO film_size here for different sensors
        self.film_size = self.sensors[0].film().size()
        self.spp = spp

        # Build vectorized pointer type of sensors
        self.sensors_unique_ptr = dr.zeros(mi.SensorPtr, self.num_sensors)
        # Use recorded loop if many sensors
        for i in range(self.num_sensors):
            dr.scatter(self.sensors_unique_ptr,
                      mi.SensorPtr(self.sensors[i]), i)

        self.dummy_film = mi.load_dict({
            'type': 'hdrfilm',
            'width': self.batch_size,
            'height': 1,
            'filter': {'type': 'box'}
        })
        self.dummysensor.dummy_film = self.dummy_film

        # num_pixel_arr: # pixels per sensor
        # accumulate_num_pixel: for sensor i, # pixels accumulated for
        # sensors 0 to i
        num_pixel_arr = []
        self.accumulate_num_pixel = []
        for i in range(self.num_sensors):
            sensor_pixels = (self.sensors[i].film().size()[0] *
                           self.sensors[i].film().size()[1])
            num_pixel_arr.append(sensor_pixels)
            self.accumulate_num_pixel.append(dr.sum(num_pixel_arr[0:i+1]))

        # total # pixels of all sensors
        self.total_num_pixel = dr.sum(num_pixel_arr)

        # flatten the reference image into 1 N*1 array
        total_pixels = self.total_num_pixel * self.channel_size
        self.image_ref_flat = dr.zeros(mi.Float, total_pixels)
        start_pixel_idx = 0
        for i in range(self.num_sensors):
            start_idx = start_pixel_idx * self.channel_size
            end_idx = self.accumulate_num_pixel[i] * self.channel_size
            self.image_ref_flat[start_idx:end_idx] = dr.ravel(
                self.image_refs[i])
            start_pixel_idx = self.accumulate_num_pixel[i]

        # start_accumulate_num_pixel: on i-th position is the # pixels
        # before the i-th sensor
        self.start_accumulate_num_pixel = mi.UInt32(
            [0] + self.accumulate_num_pixel)
        self.accumulate_num_pixel = mi.UInt32(self.accumulate_num_pixel)


    def next(self):
        # generate seed for permutation in each iteration
        total_batch_pixels = self.iteration * self.batch_size + self.batch_size
        if total_batch_pixels > self.total_num_pixel:
            self.iteration = 0
            self.permute_seed += 3

        # permuted pixel index of size self.batch_size
        start = self.iteration * self.batch_size
        pixel_range = dr.arange(mi.UInt32, self.batch_size) + start
        pixel_index = mi.permute_kensler(pixel_range, self.total_num_pixel,
                                        self.permute_seed)

        # TODO pixel indexes for different film sizes of each
        film_pixels = self.film_size[0] * self.film_size[1]
        sensor_index = pixel_index // film_pixels

        num_pixel_base = dr.gather(mi.UInt32,
                                  self.start_accumulate_num_pixel,
                                  sensor_index)

        # pixel index within each sensor
        pixel_index_individual = pixel_index - sensor_index * film_pixels
        ray_index = dr.repeat(sensor_index, self.spp)

        sensor_gather = dr.gather(mi.SensorPtr, self.sensors_unique_ptr,
                                 ray_index)
        self.iteration += 1

        sampler, _ = self.prepare(
            sensor=self.dummysensor,
            seed=7,
            spp=self.spp,
            aovs=[]
        )

        ray, weight, _ = self.sample_rays(
            pixel_index_individual, self.scene, sensor_gather, sampler,
            self.dummy_film)

        self.dummysensor.rays = ray
        self.dummysensor.weight = weight

        # pixel_index_ref: positions of selected pixels in reference image
        # pixel_index_color: channel positions of selected pixels in
        # reference image
        pixel_index_ref = num_pixel_base + pixel_index_individual
        rgb_index_mask = dr.tile(dr.arange(mi.UInt32, self.channel_size),
                                self.batch_size)
        pixel_color_base = dr.repeat(pixel_index_ref * self.channel_size,
                                    self.channel_size)
        pixel_index_color = pixel_color_base + rgb_index_mask

        # reference tensor for the selected pixels
        ref_tensor = dr.gather(mi.Float, self.image_ref_flat,
                              pixel_index_color)
        ref_tensor = mi.TensorXf(ref_tensor,
                                shape=(1, self.batch_size, self.channel_size))

        return_dict = {
            'scene': self.scene,
            'sensor': self.dummysensor,
            'params': self.scene_params,
            'spp': self.spp
        }
        return ref_tensor, return_dict

    def prepare(self,
                sensor: mi.Sensor,
                seed: int = 0,
                spp: int = 0,
                aovs: list = []):

        film = sensor.film()
        sampler = sensor.sampler().clone()

        if spp != 0:
            sampler.set_sample_count(spp)

        spp = sampler.sample_count()
        sampler.set_samples_per_wavefront(spp)

        film_size = film.crop_size()

        if film.sample_border():
            film_size += 2 * film.rfilter().border_size()

        wavefront_size = dr.prod(film_size) * spp

        if wavefront_size > 2**32:
            raise Exception(
                "The total number of Monte Carlo samples required by this "
                "rendering task (%i) exceeds 2^32 = 4294967296. Please use "
                "fewer samples per pixel or render using multiple passes."
                % wavefront_size)

        sampler.seed(seed, wavefront_size)
        film.prepare(aovs)

        return sampler, spp

    def sample_rays(
        self,
        pixel_index,
        scene: mi.Scene,
        sensor: mi.Sensor,
        sampler: mi.Sampler,
        dummy_film,
        reparam: Callable[[mi.Ray3f, mi.UInt32, mi.Bool],
                          Tuple[mi.Vector3f, mi.Float]] = None
    ) -> Tuple[mi.RayDifferential3f, mi.Spectrum, mi.Vector2f, mi.Float]:
        """
        Sample a 2D grid of primary rays for a given sensor

        Returns a tuple containing

        - the set of sampled rays
        - a ray weight (usually 1 if the sensor's response function is
          sampled perfectly)
        - the continuous 2D image-space positions associated with each ray

        When a reparameterization function is provided via the 'reparam'
        argument, it will be applied to the returned image-space position
        (i.e. the sample positions will be moving). The other two return
        values remain detached.
        """

        dummy_film_size = dummy_film.crop_size()  # [self.batch_size, 1]
        rfilter = dummy_film.rfilter()


        spp = sampler.sample_count()

        idx = dr.repeat(pixel_index, spp)

        # positions of pixels in each sensor
        # TODO: for sensors of different film sizes
        pos = mi.Vector2i()
        pos.y = idx // self.film_size[0]
        pos.x = dr.fma(mi.Int32(-self.film_size[0]), pos.y, idx)

        # Cast to floating point and add random offset
        pos_f = mi.Vector2f(pos) + sampler.next_2d()

        # Re-scale the position to [0, 1]^2
        # TODO: for sensors of different film sizes
        crop_size = self.sensors[0].film().crop_size()
        scale = dr.rcp(mi.ScalarVector2f(crop_size))
        crop_offset = self.sensors[0].film().crop_offset()
        offset = -mi.ScalarVector2f(crop_offset) * scale
        pos_adjusted = dr.fma(pos_f, scale, offset)

        aperture_sample = mi.Vector2f(0.0)

        wavelength_sample = 0
        if mi.is_spectral:
            wavelength_sample = sampler.next_1d()

        # with dr.resume_grad():
        with dr.scoped_set_flag(dr.JitFlag.SymbolicCalls, False):
            ray, weight = sensor.sample_ray_differential(
                mi.Float(0),
                wavelength_sample,
                pos_adjusted,
                aperture_sample,
                mi.Bool(True)
            )

        # With box filter, ignore random offset to prevent numerical
        # instabilities
        splatting_pos = (mi.Vector2f(pos) if rfilter.is_box_filter()
                        else pos_f)

        return ray, weight, splatting_pos
