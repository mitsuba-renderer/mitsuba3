from __future__ import annotations as __annotations__ # Delayed parsing of type annotations
# import matplotlib.pyplot as plt
import os
import sys
#import numpy as np
root = os.path.dirname(os.path.abspath(__file__))
print(root)
sys.path.insert(0, f"{root}/build/python")

import drjit as dr
import mitsuba as mi

mi.set_variant('cuda_ad_rgb')

class Rayloader():
    def __init__(self, scene, dummysensor, sensors, image_refs, batch_size, spp):
        self.scene = scene
        self.dummysensor = dummysensor
        self.sensors = sensors
        self.iteration = 0
        self.batch_size = batch_size
        self.permute_seed = 2
        self.num_sensors = len(sensors)
        self.image_refs = image_refs
        if type(image_refs) != list :
            self.image_refs = [image_refs]
            
        self.channel_size = self.image_refs[0].shape[2]
        
        # TODO film_size here for different sensors
        self.film_size = self.sensors[0].film().size()
        self.spp = spp
        
        # Build vectorized pointer type of sensors
        self.sensors_unique_ptr = dr.zeros(mi.SensorPtr, self.num_sensors)
        for i in range(self.num_sensors): # Use recorded loop if many sensors
            dr.scatter(self.sensors_unique_ptr, mi.SensorPtr(self.sensors[i]), i)
        
        self.dummy_film = mi.load_dict({
            'type': 'hdrfilm',
            'width': self.batch_size,
            'height': 1,
            'filter': {'type': 'box'}
            })
        self.dummysensor.dummy_film = self.dummy_film
        
        # num_pixel_arr: # pixels per sensor
        # accumulate_num_pixel: for sensor i, # pixels accumulated for aensors 0 to i
        num_pixel_arr = []
        self.accumulate_num_pixel = []
        for i in range(self.num_sensors):
            num_pixel_arr.append(self.sensors[i].film().size()[0] * self.sensors[i].film().size()[1])
            self.accumulate_num_pixel.append(dr.sum(num_pixel_arr[0 : i+1]))
        
        # total # pixels of all sensors     
        self.total_num_pixel = dr.sum(num_pixel_arr)
        
        #faltten the reference image into 1 N*1 array
        self.image_ref_flat = dr.zeros(mi.Float, self.total_num_pixel * self.channel_size)
        start_pixel_idx = 0
        for i in range(self.num_sensors):
            self.image_ref_flat[start_pixel_idx * self.channel_size : self.accumulate_num_pixel[i] * self.channel_size] = dr.ravel(self.image_refs[i])
            start_pixel_idx = self.accumulate_num_pixel[i]
        
        # start_accumulate_num_pixel: on i-th position is the # pixels before the i-th sensor
        self.start_accumulate_num_pixel = mi.UInt32([0] + self.accumulate_num_pixel)    
        self.accumulate_num_pixel = mi.UInt32(self.accumulate_num_pixel)
            

    def next(self):
        
        # generate seed for permuation in each iteration
        if (self.iteration * self.batch_size + self.batch_size > self.total_num_pixel):
            self.iteration = 0
            self.permute_seed += 3
            
        # permuted pixel index of size self.batch_size
        start = self.iteration * self.batch_size
        pixel_index = mi.permute_kensler(dr.arange(mi.UInt32, self.batch_size) + start, self.total_num_pixel, self.permute_seed)
        
        # TODO pixel indexes for different film sizes of each 
        sensor_index = pixel_index // (self.film_size[0] * self.film_size[1])
        
        num_pixel_base = dr.gather(mi.UInt32, self.start_accumulate_num_pixel, sensor_index)
        
        # pixel index within each sensor
        pixel_index_individual = pixel_index - sensor_index * (self.film_size[0] * self.film_size[1])
        ray_index = dr.repeat(sensor_index, self.spp)

        sensor_gather = dr.gather(mi.SensorPtr, self.sensors_unique_ptr, ray_index)
        self.iteration += 1

        sampler, spp = self.prepare(
                sensor = self.dummysensor,
                seed = 7,
                spp = self.spp,
                aovs=[]
            )

        ray, weight, pos, _ = self.sample_rays(
            pixel_index_individual, self.scene, sensor_gather, sampler, self.dummy_film)

        self.dummysensor.rays = ray
        self.dummysensor.weight = weight
         
       
        
        # pixel_index_ref: positions of selected pixels in reference image
        # pixel_index_color: channel positions of selected pixels in reference image
        pixel_index_ref = num_pixel_base + pixel_index_individual 
        rgb_index_mask = dr.tile(dr.arange(mi.UInt32, self.channel_size), self.batch_size)
        pixel_index_color = dr.repeat(pixel_index_ref * self.channel_size, self.channel_size) + rgb_index_mask
        
        # reference tensor for the selected pixels
        ref_tensor = dr.gather(mi.Float, self.image_ref_flat, pixel_index_color)
        ref_tensor = mi.TensorXf(ref_tensor, shape=(1, self.batch_size, self.channel_size))
        
        return ref_tensor
    
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
        - a ray weight (usually 1 if the sensor's response function is sampled
          perfectly)
        - the continuous 2D image-space positions associated with each ray

        When a reparameterization function is provided via the 'reparam'
        argument, it will be applied to the returned image-space position (i.e.
        the sample positions will be moving). The other two return values
        remain detached.
        """

        dummy_film_size = dummy_film.crop_size() #[self.batch_size, 1]
        rfilter = dummy_film.rfilter()
        

        spp = sampler.sample_count()

        idx = dr.repeat(pixel_index, spp)

        # positions of pixels in each sensor
        # TODO: for sensors of different film sizes
        pos = mi.Vector2i()
        pos.y = idx // self.film_size[0]
        pos.x = dr.fma(-self.film_size[0], pos.y, idx)

        # Cast to floating point and add random offset
        pos_f = mi.Vector2f(pos) + sampler.next_2d()

        # Re-scale the position to [0, 1]^2
        # TODO: for sensors of different film sizes
        scale = dr.rcp(mi.ScalarVector2f(self.sensors[0].film().crop_size()))
        offset = -mi.ScalarVector2f(self.sensors[0].film().crop_offset()) * scale
        pos_adjusted = dr.fma(pos_f, scale, offset)

        aperture_sample = mi.Vector2f(0.0)

        wavelength_sample = 0
        if mi.is_spectral:
            wavelength_sample = sampler.next_1d()

        with dr.resume_grad():
            ray, weight = sensor.sample_ray_differential(
                mi.Float(0),
                wavelength_sample,
                pos_adjusted,
                aperture_sample,
                mi.Bool(True)
            )

        reparam_det = 1.0

        if reparam is not None:
            if rfilter.is_box_filter():
                raise Exception(
                    "ADIntegrator detected the potential for image-space "
                    "motion due to differentiable shape or camera pose "
                    "parameters. This is, however, incompatible with the box "
                    "reconstruction filter that is currently used. Please "
                    "specify a smooth reconstruction filter in your scene "
                    "description (e.g. 'gaussian', which is actually the "
                    "default)")

            # This is less serious, so let's just warn once
            if not self.sensors[0].film().sample_border() and self.sample_border_warning:
                self.sample_border_warning = True

                mi.Log(mi.LogLevel.Warn,
                    "ADIntegrator detected the potential for image-space "
                    "motion due to differentiable shape or camera pose "
                    "parameters. To correctly account for shapes entering "
                    "or leaving the viewport, it is recommended that you set "
                    "the film's 'sample_border' parameter to True.")

            with dr.resume_grad():
                # Reparameterize the camera ray
                reparam_d, reparam_det = reparam(ray=dr.detach(ray),
                                                 depth=mi.UInt32(0))

                # TODO better understand why this is necessary
                # Reparameterize the camera ray to handle camera translations
                if dr.grad_enabled(ray.o):
                    reparam_d, _ = reparam(ray=ray, depth=mi.UInt32(0))

                # Create a fake interaction along the sampled ray and use it to
                # recompute the position with derivative tracking
                it = dr.zeros(mi.Interaction3f)
                it.p = ray.o + reparam_d
                ds, _ = sensor.sample_direction(it, aperture_sample)

                # Return a reparameterized image position
                pos_f = ds.uv + self.sensors[0].film().crop_offset()

        # With box filter, ignore random offset to prevent numerical instabilities
        splatting_pos = mi.Vector2f(pos) if rfilter.is_box_filter() else pos_f
        
        return ray, weight, splatting_pos, reparam_det