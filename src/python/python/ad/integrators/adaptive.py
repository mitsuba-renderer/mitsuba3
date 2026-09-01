from __future__ import annotations # Delayed parsing of type annotations

import drjit as dr
import mitsuba as mi

from .common import ADIntegrator

class AdaptiveSamplingIntegrator(ADIntegrator):
    '''
    This plugin implements an adaptive sampling integrator. It is intended to be
    used as a wrapper to another integrator, and will render the image using the
    wrapped integrator, allocating more samples in regions of higher variance.
    The implementation is based on "Global adaptive sampling hierarchies in
    production ray tracing".

    The `spp` argument passed to mi.render (or set on the sampler) will define
    the maximum number of samples computed for a pixel.

    A callback can be provided to the integrator using the `set_callback` method
    to monitor the progress of the adaptive rendering. The callback will be
    called after each pass, and should return True to continue rendering, or
    False to stop. The signature of the callback is:

    callback(pass_i: int, accum_spp: int, pass_spp: int, img: mi.TensorXf, odd_img:
             mi.TensorXf, error: mi.TensorXf, active: mi.TensorXf) -> bool

    Parameters:
        threshold: The threshold for the error metric (default: 0.005)
        pass_spp: The initial number of samples per pixel to use per pass (default: 8)
        error_filter_size: The size of the filter to use for error aggregation.
        Should be an odd number (default: 3) integrator: The integrator to use
        for rendering (default: None)
    '''
    def __init__(self, props):
        super().__init__(props)

        self.threshold = props.get('threshold', 1e-3)
        self.pass_spp  = props.get('pass_spp', 8)
        self.error_filter_size = props.get('error_filter_size', 3)
        self.callback  = None

        if len(props.unqueried()) != 1:
            raise Exception('`adaptive_sampling` integrator should have a single nested integrator!')

        for name in props.unqueried():
            obj = props.get(name)
            if issubclass(type(obj), mi.Integrator):
                self.integrator = obj

    def set_callback(self, callback):
        '''
        Set the callback function for the integrator

        The callback function should have the following signature:

        callback(pass_i: int, accum_spp: int, pass_spp: int, img: mi.TensorXf, odd_img: mi.TensorXf, error: mi.TensorXf, active: mi.TensorXf) -> bool
        '''
        self.callback = callback

    def compute_error_metric(self, img_a: mi.TensorXf, img_b: mi.TensorXf):
        '''
        Compute the error metric between two images
        '''
        intensity = sum([img_a[:, :, c] for c in range(3)])
        # R+G+B > 1 is highly exposed, and should receive less samples as less visible error
        intensity[intensity < 1.0] = dr.sqrt(intensity)
        error_diff = sum([dr.abs(img_a[:, :, c] - img_b[:, :, c]) for c in range(3)])
        error =  error_diff / (intensity + 1e-4) # Avoid division by zero
        return mi.TensorXf(error.array, shape=(error.shape[0], error.shape[1], 1))

    def filter_or(self, mask, size=3):
        '''
        Apply a filter to the mask to aggregate the error in a neighborhood
        '''
        if size % 2 == 0:
            size += 1  # Should be an odd number

        height, width = mask.shape[0], mask.shape[1]

        xx, yy = dr.meshgrid(
            dr.arange(mi.Int32, width),
            dr.arange(mi.Int32, height),
        )

        def lookup(offset_x, offset_y):
            x2 = xx + offset_x
            y2 = yy + offset_y
            idx = x2 + y2 * width
            active = (x2 >= 0) & (x2 < width) & (y2 >= 0) & (y2 < height)
            return mi.TensorXb(dr.gather(type(mask.array), mask.array, idx, active), shape=(height, width, 1))

        ret = dr.zeros(mi.TensorXb)
        for x in range(size):
            for y in range(size):
                ret |= lookup(-size // 2 + x, -size // 2 + y)

        return ret

    def sample_rays(
        self,
        scene: mi.Scene,
        sensor: mi.Sensor,
        sampler: mi.Sampler,
        active: mi.TensorXf = True,
    ) -> Tuple[mi.RayDifferential3f, mi.Spectrum, mi.Vector2f, mi.Float]:
        """
        Modified implementation of ADIntegrator.sample_rays that compresses the
        wavefront based on the active mask in order to only render a subset of
        the pixels.
        """
        film = sensor.film()
        film_size = film.crop_size()
        rfilter = film.rfilter()
        border_size = rfilter.border_size()

        if film.sample_border():
            film_size += 2 * border_size

        spp = sampler.sample_count()

        # Compute discrete sample position
        idx = dr.arange(mi.UInt32, dr.prod(film_size) * spp)

        if not isinstance(active, bool):
            active_indices = dr.compress(dr.repeat(active, spp).array)
            idx = dr.gather(mi.UInt32, idx, active_indices)

        # Try to avoid a division by an unknown constant if we can help it
        log_spp = dr.log2i(spp)
        if 1 << log_spp == spp:
            idx >>= dr.opaque(mi.UInt32, log_spp)
        else:
            idx //= dr.opaque(mi.UInt32, spp)

        # Compute the position on the image plane
        pos = mi.Vector2i()
        pos.y = idx // film_size[0]
        pos.x = dr.fma(mi.UInt32(mi.Int32(-film_size[0])), pos.y, idx)

        if film.sample_border():
            pos -= border_size

        pos += mi.Vector2i(film.crop_offset())

        # Cast to floating point and add random offset
        pos_f = mi.Vector2f(pos) + sampler.next_2d()

        # Re-scale the position to [0, 1]^2
        scale = dr.rcp(mi.ScalarVector2f(film.crop_size()))
        offset = -mi.ScalarVector2f(film.crop_offset()) * scale
        pos_adjusted = dr.fma(pos_f, scale, offset)

        aperture_sample = mi.Vector2f(0.0)
        if sensor.needs_aperture_sample():
            aperture_sample = sampler.next_2d()

        time = sensor.shutter_open()
        if sensor.shutter_open_time() > 0:
            time += sampler.next_1d() * sensor.shutter_open_time()

        wavelength_sample = 0
        if mi.is_spectral:
            wavelength_sample = sampler.next_1d()

        with dr.resume_grad():
            ray, weight = sensor.sample_ray_differential(
                time=time,
                sample1=wavelength_sample,
                sample2=pos_adjusted,
                sample3=aperture_sample
            )

        # With box filter, ignore random offset to prevent numerical instabilities
        splatting_pos = mi.Vector2f(pos) if rfilter.is_box_filter() else pos_f

        return ray, weight, splatting_pos

    def _splat_to_block(block: mi.ImageBlock,
                        film: mi.Film,
                        pos: mi.Point2f,
                        value: mi.Spectrum,
                        weight: mi.Float,
                        alpha: mi.Float,
                        aovs: Sequence[mi.Float],
                        wavelengths: mi.Spectrum,
                        active: mi.Bool = True):
        '''
        Modified implementation of ADIntegrator._splat_to_block that takes a
        extra mask argument to only splat the active samples.
        '''
        value = mi.unpolarized_spectrum(value)
        if (dr.all(mi.has_flag(film.flags(), mi.FilmFlags.Special))):
            aovs = film.prepare_sample(value, wavelengths,
                                       block.channel_count(),
                                       weight=weight,
                                       alpha=alpha)
            block.put(pos, aovs, active)
            del aovs
        else:
            if mi.is_polarized:
                value = mi.unpolarized_spectrum(value)
            if mi.is_spectral:
                rgb = mi.spectrum_to_srgb(value, wavelengths)
            elif mi.is_monochromatic:
                rgb = mi.Color3f(value.x)
            else:
                rgb = value
            if mi.has_flag(film.flags(), mi.FilmFlags.Alpha):
                aovs = [rgb.x, rgb.y, rgb.z, alpha, weight] + aovs
            else:
                aovs = [rgb.x, rgb.y, rgb.z, weight] + aovs
            block.put(pos, aovs, active)


    def render(self: mi.SamplingIntegrator,
               scene: mi.Scene,
               sensor: Union[int, mi.Sensor] = 0,
               seed: mi.UInt32 = 0,
               spp: int = 0,
               develop: bool = True,
               evaluate: bool = True) -> mi.TensorXf:
        if isinstance(sensor, int):
            sensor = scene.sensors()[sensor]
        film = sensor.film()
        film_size = film.crop_size()

        max_spp = spp
        pass_spp = self.pass_spp

        if pass_spp >= max_spp:
            return self.integrator.render(scene, sensor, seed, spp, develop, evaluate)

        nb_passes = max_spp // pass_spp

        mi.Log(mi.LogLevel.Debug, f'Adaptive rendering, maximum number of passes: {nb_passes}')

        adaptive_active = dr.ones(mi.TensorXb, shape=(film_size[1], film_size[0], 1))

        # Add RGB odd samples channel for adaptive sampling
        film.prepare(self.aov_names() + ['R_odd', 'B_odd', 'G_odd', 'W_odd'])

        original_sampler = sensor.sampler()
        sampler = original_sampler.clone()

        pixel_count = dr.count(adaptive_active.array)[0]
        accum_spp = 0
        initial_pixel_count = pixel_count
        initial_spp = pass_spp

        # Disable derivatives in all of the following
        with dr.suspend_grad():
            for i in range(nb_passes):
                if pixel_count == 0 or accum_spp >= max_spp or pass_spp == 0:
                    break

                if not pass_spp > 1:
                    raise Exception(f'pass_spp should be greater than one, got {pass_spp}')
                if not pass_spp % 2 == 0:
                    raise Exception(f'pass_spp should be an even number, got {pass_spp}')

                # Prepare the film and sample generator for rendering
                sampler.set_sample_count(pass_spp)
                pass_spp = sampler.sample_count()
                sampler.set_samples_per_wavefront(pass_spp)

                if film.sample_border():
                    raise Exception('sample_border is not supported with adaptive sampling!')

                wavefront_size = dr.count(adaptive_active.array)[0] * pass_spp

                if wavefront_size > 2**32:
                    raise Exception(
                        "The total number of Monte Carlo samples required by this "
                        "rendering task (%i) exceeds 2^32 = 4294967296. Please use "
                        "fewer samples per pixel or render using multiple passes."
                        % wavefront_size)

                sampler.seed(seed + i, wavefront_size)

                # Generate a set of rays starting at the sensor
                ray, weight, pos = self.sample_rays(scene, sensor, sampler, adaptive_active)

                # Launch the Monte Carlo sampling process in primal mode
                L, alpha, aovs, _ = self.sample(
                    mode=dr.ADMode.Primal,
                    scene=scene,
                    sampler=sampler,
                    ray=ray,
                    depth=mi.UInt32(0),
                    δL=None,
                    δaovs=None,
                    state_in=None,
                    active=mi.Bool(True)
                )

                rgb = L * weight
                if mi.is_polarized:
                    rgb = mi.Color3f(rgb[0][0])

                odd_mask = dr.arange(mi.UInt32, dr.width(ray)) % 2 == 0
                rgb_odd = dr.select(odd_mask, rgb, 0.0)

                block = film.create_block()
                block.set_coalesce(block.coalesce() and pass_spp >= 4)
                AdaptiveSamplingIntegrator._splat_to_block(
                    block, film, pos,
                    value=L * weight,
                    weight=1.0,
                    alpha=alpha,
                    aovs=aovs + [rgb_odd.x, rgb_odd.y, rgb_odd.z, dr.select(odd_mask, 1.0, 0.0)],
                    wavelengths=ray.wavelengths,
                )

                film.put_block(block) # Will trigger a kernel evaluation
                img = film.develop()

                dr.eval(img) # Trigger kernel evaluation

                # Re-weight the odd sample image as it was divided by the
                # weights of the all samples
                odd_weight = img[:, :, -1][:, :, None]
                odd_img = img[:, :, -4:-1] / odd_weight
                img = img[:, :, :-4]

                # Check for convergence
                error = self.compute_error_metric(img[:, :, :3], odd_img)
                non_converged = error > self.threshold

                if self.error_filter_size > 1:
                    non_converged = self.filter_or(non_converged, size=self.error_filter_size)

                adaptive_active &= non_converged

                accum_spp += pass_spp

                dr.eval(odd_img, error, adaptive_active) # Trigger kernel evaluation (before callback)

                if self.callback is not None:
                    if not self.callback(i, accum_spp, pass_spp, img, odd_img, error, adaptive_active):
                        break

                pixel_count = dr.count(adaptive_active.array)[0]
                if pixel_count > 0:
                    pixel_ratio = initial_pixel_count / float(pixel_count)
                    pass_spp = min(int(initial_spp * pixel_ratio), max_spp - accum_spp)
                    if pass_spp % 2 != 0:
                        pass_spp -= 1

            return img

    def sample(self,
               mode: dr.ADMode,
               scene: mi.Scene,
               sampler: mi.Sampler,
               ray: mi.Ray3f,
               δL: Optional[mi.Spectrum],
               state_in: Optional[mi.Spectrum],
               active: mi.Bool,
               **kwargs # Absorbs unused arguments
    ):
        if type(self.integrator).__bases__[0] in [mi.Integrator, mi.SamplingIntegrator]:
            return *self.integrator.sample(
                scene=scene,
                sampler=sampler,
                ray=ray,
                medium=None,
                active=active
            ), None
        else:
            return self.integrator.sample(
                mode=mode,
                scene=scene,
                sampler=sampler,
                ray=ray,
                δL=None,
                δaovs=None,
                state_in=None,
                active=active
            )

    def render_forward(self, *args, **kwargs) -> None:
        return self.integrator.render_forward(*args, **kwargs)

    def render_backward(self, *args, **kwargs) -> None:
        return self.integrator.render_backward(*args, **kwargs)

    def aov_names(self):
        return self.integrator.aov_names()

mi.register_integrator("adaptive_sampling", lambda props: AdaptiveSamplingIntegrator(props))

del ADIntegrator
