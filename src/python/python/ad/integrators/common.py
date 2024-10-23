from __future__ import annotations as __annotations__ # Delayed parsing of type annotations

import mitsuba as mi
import drjit as dr
import gc


class ADIntegrator(mi.CppADIntegrator):
    """
    Abstract base class of numerous differentiable integrators in Mitsuba

    .. pluginparameters::

     * - max_depth
       - |int|
       - Specifies the longest path depth in the generated output image (where -1
         corresponds to :math:`\\infty`). A value of 1 will only render directly
         visible light sources. 2 will lead to single-bounce (direct-only)
         illumination, and so on. (Default: 6)
     * - rr_depth
       - |int|
       - Specifies the path depth, at which the implementation will begin to use
         the *russian roulette* path termination criterion. For example, if set to
         1, then path generation many randomly cease after encountering directly
         visible surfaces. (Default: 5)
    """

    def __init__(self, props):
        super().__init__(props)

        max_depth = props.get('max_depth', 6)
        if max_depth < 0 and max_depth != -1:
            raise Exception("\"max_depth\" must be set to -1 (infinite) or a value >= 0")

        # Map -1 (infinity) to 2^32-1 bounces
        self.max_depth = max_depth if max_depth != -1 else 0xffffffff

        self.rr_depth = props.get('rr_depth', 5)
        if self.rr_depth <= 0:
            raise Exception("\"rr_depth\" must be set to a value greater than zero!")

    def to_string(self):
        return f'{type(self).__name__}[max_depth = {self.max_depth},' \
               f' rr_depth = { self.rr_depth }]'

    def render(self: mi.SamplingIntegrator,
               scene: mi.Scene,
               sensor: Union[int, mi.Sensor] = 0,
               seed: mi.UInt32 = 0,
               spp: int = 0,
               develop: bool = True,
               evaluate: bool = True) -> mi.TensorXf:
        if not develop:
            raise Exception("develop=True must be specified when "
                            "invoking AD integrators")

        if isinstance(sensor, int):
            sensor = scene.sensors()[sensor]

        film = sensor.film()

        # Disable derivatives in all of the following
        with dr.suspend_grad():
            # Prepare the film and sample generator for rendering
            sampler, spp = self.prepare(
                sensor=sensor,
                seed=seed,
                spp=spp,
                aovs=self.aov_names()
            )

            # Generate a set of rays starting at the sensor
            ray, weight, pos = self.sample_rays(scene, sensor, sampler)

            # Launch the Monte Carlo sampling process in primal mode
            L, valid, aovs, _ = self.sample(
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

            # Prepare an ImageBlock as specified by the film
            block = film.create_block()

            # Only use the coalescing feature when rendering enough samples
            block.set_coalesce(block.coalesce() and spp >= 4)

            # Accumulate into the image block
            ADIntegrator._splat_to_block(
                block, film, pos,
                value=L * weight,
                weight=1.0,
                alpha=dr.select(valid, mi.Float(1), mi.Float(0)),
                aovs=aovs,
                wavelengths=ray.wavelengths
            )

            # Explicitly delete any remaining unused variables
            del sampler, ray, weight, pos, L, valid

            # Perform the weight division and return an image tensor
            film.put_block(block)

            return film.develop()

    def render_forward(self: mi.SamplingIntegrator,
                       scene: mi.Scene,
                       params: Any,
                       sensor: Union[int, mi.Sensor] = 0,
                       seed: mi.UInt32 = 0,
                       spp: int = 0) -> mi.TensorXf:

        if isinstance(sensor, int):
            sensor = scene.sensors()[sensor]

        film = sensor.film()

        # Disable derivatives in all of the following
        with dr.suspend_grad():
            # Prepare the film and sample generator for rendering
            sampler, spp = self.prepare(sensor, seed, spp, self.aov_names())

            # Generate a set of rays starting at the sensor, keep track of
            # derivatives wrt. sample positions ('pos') if there are any
            ray, weight, pos = self.sample_rays(scene, sensor, sampler)

            with dr.resume_grad():
                L, valid, aovs, _ = self.sample(
                    mode=dr.ADMode.Forward,
                    scene=scene,
                    sampler=sampler,
                    ray=ray,
                    active=mi.Bool(True)
                )

                block = film.create_block()
                # Only use the coalescing feature when rendering enough samples
                block.set_coalesce(block.coalesce() and spp >= 4)

                ADIntegrator._splat_to_block(
                    block, film, pos,
                    value=L * weight,
                    weight=1,
                    alpha=dr.select(valid, mi.Float(1), mi.Float(0)),
                    aovs=aovs,
                    wavelengths=ray.wavelengths
                )

                # Perform the weight division
                film.put_block(block)
                result_img = film.develop()

                # Propagate the gradients to the image tensor
                dr.forward_to(result_img)

        return dr.grad(result_img)

    def render_backward(self: mi.SamplingIntegrator,
                        scene: mi.Scene,
                        params: Any,
                        grad_in: mi.TensorXf,
                        sensor: Union[int, mi.Sensor] = 0,
                        seed: mi.UInt32 = 0,
                        spp: int = 0) -> None:

        if isinstance(sensor, int):
            sensor = scene.sensors()[sensor]

        film = sensor.film()

        # Disable derivatives in all of the following
        with dr.suspend_grad():
            # Prepare the film and sample generator for rendering
            sampler, spp = self.prepare(sensor, seed, spp, self.aov_names())

            # Generate a set of rays starting at the sensor, keep track of
            # derivatives wrt. sample positions ('pos') if there are any
            ray, weight, pos = self.sample_rays(scene, sensor, sampler)

            with dr.resume_grad():
                L, valid, aovs, _ = self.sample(
                    mode=dr.ADMode.Backward,
                    scene=scene,
                    sampler=sampler,
                    ray=ray,
                    active=mi.Bool(True)
                )

                # Prepare an ImageBlock as specified by the film
                block = film.create_block()

                # Only use the coalescing feature when rendering enough samples
                block.set_coalesce(block.coalesce() and spp >= 4)

                # Accumulate into the image block
                ADIntegrator._splat_to_block(
                    block, film, pos,
                    value=L * weight,
                    weight=1,
                    alpha=dr.select(valid, mi.Float(1), mi.Float(0)),
                    aovs=aovs,
                    wavelengths=ray.wavelengths
                )

                film.put_block(block)

                del valid

                # This step launches a kernel
                dr.schedule(block.tensor())
                image = film.develop()

                # Differentiate sample splatting and weight division steps to
                # retrieve the adjoint radiance
                dr.set_grad(image, grad_in)
                dr.enqueue(dr.ADMode.Backward, image)
                dr.traverse(dr.ADMode.Backward)

            # We don't need any of the outputs here
            del ray, weight, pos, block, sampler

            # Run kernel representing side effects of the above
            dr.eval()

    def sample_rays(
        self,
        scene: mi.Scene,
        sensor: mi.Sensor,
        sampler: mi.Sampler,
    ) -> Tuple[mi.RayDifferential3f, mi.Spectrum, mi.Vector2f, mi.Float]:
        """
        Sample a 2D grid of primary rays for a given sensor

        Returns a tuple containing

        - the set of sampled rays
        - a ray weight (usually 1 if the sensor's response function is sampled
          perfectly)
        - the continuous 2D image-space positions associated with each ray
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

    def prepare(self,
                sensor: mi.Sensor,
                seed: mi.UInt32 = 0,
                spp: int = 0,
                aovs: list = []):
        """
        Given a sensor and a desired number of samples per pixel, this function
        computes the necessary number of Monte Carlo samples and then suitably
        seeds the sampler underlying the sensor.

        Returns the created sampler and the final number of samples per pixel
        (which may differ from the requested amount depending on the type of
        ``Sampler`` being used)

        Parameter ``sensor`` (``int``, ``mi.Sensor``):
            Specify a sensor to render the scene from a different viewpoint.

        Parameter ``seed` (``int``)
            This parameter controls the initialization of the random number
            generator during the primal rendering step. It is crucial that you
            specify different seeds (e.g., an increasing sequence) if subsequent
            calls should produce statistically independent images (e.g. to
            de-correlate gradient-based optimization steps).

        Parameter ``spp`` (``int``):
            Optional parameter to override the number of samples per pixel for the
            primal rendering step. The value provided within the original scene
            specification takes precedence if ``spp=0``.
        """

        film = sensor.film()
        original_sampler =  sensor.sampler()
        sampler = original_sampler.clone()

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

    def _splat_to_block(block: mi.ImageBlock,
                       film: mi.Film,
                       pos: mi.Point2f,
                       value: mi.Spectrum,
                       weight: mi.Float,
                       alpha: mi.Float,
                       aovs: Sequence[mi.Float],
                       wavelengths: mi.Spectrum):
        '''Helper function to splat values to a imageblock'''
        if (dr.all(mi.has_flag(film.flags(), mi.FilmFlags.Special))):
            aovs = film.prepare_sample(value, wavelengths,
                                       block.channel_count(),
                                       weight=weight,
                                       alpha=alpha)
            block.put(pos, aovs)
            del aovs
        else:
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
            block.put(pos, aovs)


    def sample(self,
               mode: dr.ADMode,
               scene: mi.Scene,
               sampler: mi.Sampler,
               ray: mi.Ray3f,
               depth: mi.UInt32,
               δL: Optional[mi.Spectrum],
               δaovs: Optional[mi.Spectrum],
               state_in: Any,
               active: mi.Bool) -> Tuple[mi.Spectrum, mi.Bool, List[mi.Float]]:
        """
        This function does the main work of differentiable rendering and
        remains unimplemented here. It is provided by subclasses of the
        ``RBIntegrator`` interface.

        In those concrete implementations, the function performs a Monte Carlo
        random walk, implementing a number of different behaviors depending on
        the ``mode`` argument. For example in primal mode (``mode ==
        drjit.ADMode.Primal``), it behaves like a normal rendering algorithm
        and estimates the radiance incident along ``ray``.

        In forward mode (``mode == drjit.ADMode.Forward``), it estimates the
        derivative of the incident radiance for a set of scene parameters being
        differentiated. (This requires that these parameters are attached to
        the AD graph and have gradients specified via ``dr.set_grad()``)

        In backward mode (``mode == drjit.ADMode.Backward``), it takes adjoint
        radiance ``δL`` and accumulates it into differentiable scene parameters.

        You are normally *not* expected to directly call this function. Instead,
        use ``mi.render()`` , which performs various necessary
        setup steps to correctly use the functionality provided here.

        The parameters of this function are as follows:

        Parameter ``mode`` (``drjit.ADMode``)
            Specifies whether the rendering algorithm should run in primal or
            forward/backward derivative propagation mode

        Parameter ``scene`` (``mi.Scene``):
            Reference to the scene being rendered in a differentiable manner.

        Parameter ``sampler`` (``mi.Sampler``):
            A pre-seeded sample generator

        Parameter ``depth`` (``mi.UInt32``):
            Path depth of `ray` (typically set to zero). This is mainly useful
            for forward/backward differentiable rendering phases that need to
            obtain an incident radiance estimate. In this case, they may
            recursively invoke ``sample(mode=dr.ADMode.Primal)`` with a nonzero
            depth.

        Parameter ``δL`` (``mi.Spectrum``):
            When back-propagating gradients (``mode == drjit.ADMode.Backward``)
            the ``δL`` parameter should specify the adjoint radiance associated
            with each ray. Otherwise, it must be set to ``None``.

        Parameter ``state_in`` (``Any``):
            The primal phase of ``sample()`` returns a state vector as part of
            its return value. The forward/backward differential phases expect
            that this state vector is provided to them via this argument. When
            invoked in primal mode, it should be set to ``None``.

        Parameter ``active`` (``mi.Bool``):
            This mask array can optionally be used to indicate that some of
            the rays are disabled.

        The function returns a tuple ``(spec, valid, state_out)`` where

        Output ``spec`` (``mi.Spectrum``):
            Specifies the estimated radiance and differential radiance in
            primal and forward mode, respectively.

        Output ``valid`` (``mi.Bool``):
            Indicates whether the rays intersected a surface, which can be used
            to compute an alpha channel.

        Output ``aovs`` (``List[mi.Float]``):
            Integrators may return one or more arbitrary output variables (AOVs).
            The implementation has to guarantee that the number of returned AOVs
            matches the length of self.aov_names().

        """

        raise Exception('RBIntegrator does not provide the sample() method. '
                        'It should be implemented by subclasses that '
                        'specialize the abstract RBIntegrator interface.')


class RBIntegrator(ADIntegrator):
    """
    Abstract base class of radiative-backpropagation style differentiable
    integrators.
    """

    def render_forward(self: mi.SamplingIntegrator,
                       scene: mi.Scene,
                       params: Any,
                       sensor: Union[int, mi.Sensor] = 0,
                       seed: mi.UInt32 = 0,
                       spp: int = 0) -> mi.TensorXf:
        """
        Evaluates the forward-mode derivative of the rendering step.

        Forward-mode differentiation propagates gradients from scene parameters
        through the simulation, producing a *gradient image* (i.e., the derivative
        of the rendered image with respect to those scene parameters). The gradient
        image is very helpful for debugging, for example to inspect the gradient
        variance or visualize the region of influence of a scene parameter. It is
        not particularly useful for simultaneous optimization of many parameters,
        since multiple differentiation passes are needed to obtain separate
        derivatives for each scene parameter. See ``Integrator.render_backward()``
        for an efficient way of obtaining all parameter derivatives at once, or
        simply use the ``mi.render()`` abstraction that hides both
        ``Integrator.render_forward()`` and ``Integrator.render_backward()`` behind
        a unified interface.

        Before calling this function, you must first enable gradient tracking and
        furthermore associate concrete input gradients with one or more scene
        parameters, or the function will just return a zero-valued gradient image.
        This is typically done by invoking ``dr.enable_grad()`` and
        ``dr.set_grad()`` on elements of the ``SceneParameters`` data structure
        that can be obtained obtained via a call to
        ``mi.traverse()``.

        Parameter ``scene`` (``mi.Scene``):
            The scene to be rendered differentially.

        Parameter ``params``:
           An arbitrary container of scene parameters that should receive
           gradients. Typically this will be an instance of type
           ``mi.SceneParameters`` obtained via ``mi.traverse()``. However, it
           could also be a Python list/dict/object tree (DrJit will traverse it
           to find all parameters). Gradient tracking must be explicitly enabled
           for each of these parameters using ``dr.enable_grad(params['parameter_name'])``
           (i.e. ``render_forward()`` will not do this for you). Furthermore,
           ``dr.set_grad(...)`` must be used to associate specific gradient values
           with each parameter.

        Parameter ``sensor`` (``int``, ``mi.Sensor``):
            Specify a sensor or a (sensor index) to render the scene from a
            different viewpoint. By default, the first sensor within the scene
            description (index 0) will take precedence.

        Parameter ``seed` (``int``)
            This parameter controls the initialization of the random number
            generator. It is crucial that you specify different seeds (e.g., an
            increasing sequence) if subsequent calls should produce statistically
            independent images (e.g. to de-correlate gradient-based optimization
            steps).

        Parameter ``spp`` (``int``):
            Optional parameter to override the number of samples per pixel for the
            differential rendering step. The value provided within the original
            scene specification takes precedence if ``spp=0``.
        """

        if isinstance(sensor, int):
            sensor = scene.sensors()[sensor]

        film = sensor.film()

        # Disable derivatives in all of the following
        with dr.suspend_grad():
            # Prepare the film and sample generator for rendering
            sampler, spp = self.prepare(sensor, seed, spp, self.aov_names())

            # Generate a set of rays starting at the sensor, keep track of
            # derivatives wrt. sample positions ('pos') if there are any
            ray, weight, pos = self.sample_rays(scene, sensor, sampler)

            # Launch the Monte Carlo sampling process in primal mode (1)
            L, valid, aovs, state_out = self.sample(
                mode=dr.ADMode.Primal,
                scene=scene,
                sampler=sampler.clone(),
                ray=ray,
                depth=mi.UInt32(0),
                δL=None,
                state_in=None,
                active=mi.Bool(True)
            )

            # Launch the Monte Carlo sampling process in forward mode (2)
            δL, valid_2, δaovs, state_out_2 = self.sample(
                mode=dr.ADMode.Forward,
                scene=scene,
                sampler=sampler,
                ray=ray,
                depth=mi.UInt32(0),
                δL=None,
                δaovs=None,
                state_in=state_out,
                active=mi.Bool(True)
            )

            # Prepare an ImageBlock as specified by the film
            block = film.create_block()

            # Only use the coalescing feature when rendering enough samples
            block.set_coalesce(block.coalesce() and spp >= 4)

            # Accumulate into the image block
            ADIntegrator._splat_to_block(
                block, film, pos,
                value=δL * weight,
                weight=1.0,
                alpha=dr.select(valid_2, mi.Float(1), mi.Float(0)),
                aovs=[δaov * weight for δaov in δaovs],
                wavelengths=ray.wavelengths
            )

            # Perform the weight division and return an image tensor
            film.put_block(block)

            # Explicitly delete any remaining unused variables
            del sampler, ray, weight, pos, L, valid, aovs, δL, δaovs, \
                valid_2, params, state_out, state_out_2, block

            result_grad = film.develop()

        return result_grad

    def render_backward(self: mi.SamplingIntegrator,
                        scene: mi.Scene,
                        params: Any,
                        grad_in: mi.TensorXf,
                        sensor: Union[int, mi.Sensor] = 0,
                        seed: mi.UInt32 = 0,
                        spp: int = 0) -> None:
        """
        Evaluates the reverse-mode derivative of the rendering step.

        Reverse-mode differentiation transforms image-space gradients into scene
        parameter gradients, enabling simultaneous optimization of scenes with
        millions of free parameters. The function is invoked with an input
        *gradient image* (``grad_in``) and transforms and accumulates these into
        the gradient arrays of scene parameters that previously had gradient
        tracking enabled.

        Before calling this function, you must first enable gradient tracking for
        one or more scene parameters, or the function will not do anything. This is
        typically done by invoking ``dr.enable_grad()`` on elements of the
        ``SceneParameters`` data structure that can be obtained obtained via a call
        to ``mi.traverse()``. Use ``dr.grad()`` to query the
        resulting gradients of these parameters once ``render_backward()`` returns.

        Parameter ``scene`` (``mi.Scene``):
            The scene to be rendered differentially.

        Parameter ``params``:
           An arbitrary container of scene parameters that should receive
           gradients. Typically this will be an instance of type
           ``mi.SceneParameters`` obtained via ``mi.traverse()``. However, it
           could also be a Python list/dict/object tree (DrJit will traverse it
           to find all parameters). Gradient tracking must be explicitly enabled
           for each of these parameters using ``dr.enable_grad(params['parameter_name'])``
           (i.e. ``render_backward()`` will not do this for you).

        Parameter ``grad_in`` (``mi.TensorXf``):
            Gradient image that should be back-propagated.

        Parameter ``sensor`` (``int``, ``mi.Sensor``):
            Specify a sensor or a (sensor index) to render the scene from a
            different viewpoint. By default, the first sensor within the scene
            description (index 0) will take precedence.

        Parameter ``seed` (``int``)
            This parameter controls the initialization of the random number
            generator. It is crucial that you specify different seeds (e.g., an
            increasing sequence) if subsequent calls should produce statistically
            independent images (e.g. to de-correlate gradient-based optimization
            steps).

        Parameter ``spp`` (``int``):
            Optional parameter to override the number of samples per pixel for the
            differential rendering step. The value provided within the original
            scene specification takes precedence if ``spp=0``.
        """

        if isinstance(sensor, int):
            sensor = scene.sensors()[sensor]

        film = sensor.film()

        # Disable derivatives in all of the following
        with dr.suspend_grad():
            # Prepare the film and sample generator for rendering
            sampler, spp = self.prepare(sensor, seed, spp, self.aov_names())

            # Generate a set of rays starting at the sensor, keep track of
            # derivatives wrt. sample positions ('pos') if there are any
            ray, weight, pos = self.sample_rays(scene, sensor, sampler)

            def splatting_and_backward_gradient_image(value: mi.Spectrum,
                                                      weight: mi.Float,
                                                      alpha: mi.Float,
                                                      aovs: Sequence[mi.Float]):
                '''
                Backward propagation of the gradient image through the sample
                splatting and weight division steps.
                '''

                # Prepare an ImageBlock as specified by the film
                block = film.create_block()

                # Only use the coalescing feature when rendering enough samples
                block.set_coalesce(block.coalesce() and spp >= 4)

                ADIntegrator._splat_to_block(
                    block, film, pos,
                    value=value,
                    weight=weight,
                    alpha=alpha,
                    aovs=aovs,
                    wavelengths=ray.wavelengths
                )

                film.put_block(block)

                image = film.develop()

                dr.set_grad(image, grad_in)
                dr.enqueue(dr.ADMode.Backward, image)
                dr.traverse(dr.ADMode.Backward)

            # Differentiate sample splatting and weight division steps to
            # retrieve the adjoint radiance (e.g. 'δL')
            with dr.resume_grad():
                with dr.suspend_grad(pos, ray, weight):
                    L = dr.full(mi.Spectrum, 1.0, dr.width(ray))
                    dr.enable_grad(L)
                    aovs = []
                    for _ in self.aov_names():
                        aov = dr.ones(mi.Float, dr.width(ray))
                        dr.enable_grad(aov)
                        aovs.append(aov)
                    splatting_and_backward_gradient_image(
                        value=L * weight,
                        weight=1.0,
                        alpha=1.0,
                        aovs=[aov * weight for aov in aovs]
                    )

                    δL = dr.grad(L)
                    δaovs = dr.grad(aovs)

            # Clear the dummy data splatted on the film above
            film.clear()

            # Launch the Monte Carlo sampling process in primal mode (1)
            L, valid, aovs, state_out = self.sample(
                mode=dr.ADMode.Primal,
                scene=scene,
                sampler=sampler.clone(),
                ray=ray,
                depth=mi.UInt32(0),
                δL=None,
                δaovs=None,
                state_in=None,
                active=mi.Bool(True)
            )

            # Launch Monte Carlo sampling in backward AD mode (2)
            L_2, valid_2, aovs_2, state_out_2 = self.sample(
                mode=dr.ADMode.Backward,
                scene=scene,
                sampler=sampler,
                ray=ray,
                depth=mi.UInt32(0),
                δL=δL,
                δaovs=δaovs,
                state_in=state_out,
                active=mi.Bool(True)
            )

            # We don't need any of the outputs here
            del L_2, valid_2, aovs_2, state_out, state_out_2, \
                δL, δaovs, ray, weight, pos, sampler


            # Run kernel representing side effects of the above
            dr.eval()


class PSIntegrator(ADIntegrator):
    """
    Abstract base class of projective-sampling/path-space style differentiable
    integrators.
    """

    def __init__(self, props):
        super().__init__(props)

        self.proj_detail = mi.ad.ProjectiveDetail(self)

        # Effective sample count per pixel for the continuous derivative, the
        # primarily visible discontinuous derivative, and the indirect
        # discontinuous derivative.
        # These values are *ONLY* used if no runtime spp value is provided or it
        # is equal to 0
        self.sppc = props.get('sppc', None)
        self.sppp = props.get('sppp', None)
        self.sppi = props.get('sppi', None)

        self.guiding = props.get('guiding', 'octree')
        self.guiding_proj = props.get('guiding_proj', True)
        self.guiding_rounds = props.get('guiding_rounds', 1)

        if self.guiding not in ['none', 'grid', 'octree']:
            raise Exception("\"guiding\" must be set to \"none\", \"grid\", or \"octree\"")

        ############## Additional internal parameters and flags ###############
        # All values below should be considered as safe & sane defaults for most
        # use cases. Modifying them requires a deeper understanding of the
        # internals of the integrator or of the guiding structures it uses. The
        # parameters exposed through the plugin's properties are, in comparison,
        # higher-level parameters.

        # If set to be "True", use radiative backpropagation to compute the
        # continuous derivative. This can only be set by the PSIntegrator
        # implementation and should not be modified here.
        self.radiative_backprop = True

        # Guiding grid resolution, only used when guiding == 'grid'
        self.guiding_grid_reso = [10000, 100, 100]

        # Number of samples per pixel to be projected for guiding. If set to 0,
        # it will use `ceil(prod(guiding_resolution)/num_pixels)`.
        self.proj_seed_spp = 512

        # Samples with a smaller contribution than "value * mass_ttl" will be
        # clamped for numerical stability
        self.clamp_mass_thres = 1e-8

        # Apply a power transform on the sample contribution as x'=x^(1-this) to
        # suppress outliers. If set to 0, no transform will be applied.
        self.scale_mass = 0.

        ##### MESH PROJECTION #####
        # Mesh projection algorithm {'hybrid', 'walk', 'jump'}
        self.proj_mesh_algo = 'hybrid'

        # Maximum number of mesh local walks
        self.proj_mesh_max_walk = 30

        # Maximum number of mesh jumps
        self.proj_mesh_max_jump = 2

        ##### OCTREE #####
        # Maximum depth of the octree
        self.octree_max_depth = 9

        # Maximum number of leaves in the octree (~16.8 million)
        self.octree_max_leaf_cnt = 2 ** 24

        # Number of extra samples per leaf
        self.octree_extra_leaf_sample = 256

        # Pre-partition the x-axis into "this" slices to account for the
        # extra heterogeneity along the x-dimension.
        self.octree_highres_x_slices = 100

        # Boundary samples with contribution smaller than this value
        # will be ignored during octree construction. If set to be 0, the
        # integrator will compute a threshold with an expensive estimate.
        self.octree_contruction_thres = 0

        # If `octree_contruction_thres` is 0, the integrator computes the
        # mean contribution of all non-zero boundary samples, and sets
        # `octree_contruction_thres` as "mean * octree_construction_mean_mult".
        # The same threshold will be reused unless it is manually reset to 0.
        self.octree_construction_mean_mult = 0.1

        # If set to be "True", launch one kernel for all rounds of
        # projections. Otherwise a recorded loop simulates the multi-round
        # initialization.
        self.octree_scatter_inc = True

        ##### OTHER #####
        # Warn about potential bias due to shapes entering/leaving the frame
        self.sample_border_warning = True


    def override_spp(self, integrator_spp: int, runtime_spp: int, sampler_spp: int):
        """
        Utility method to override the intergrator's spp value with the one
        received at runtime in `render`/`render_backward`/`render_forward`.

        The runtime value is overriden only if it is 0 and if the integrator
        has defined a spp value. If the integrator hasn't defined a value, the
        sampler's spp is used.
        """
        out = runtime_spp
        if runtime_spp == 0:
            if integrator_spp is not None:
                out = integrator_spp
            else:
                out = sampler_spp

        return out

    def render_ad(self,
                  scene: mi.Scene,
                  sensor: Union[int, mi.Sensor],
                  seed: mi.UInt32,
                  spp: int,
                  mode: dr.ADMode) -> mi.TensorXf:
        """
        Renders and accumulates the outputs of the primarily visible
        discontinuities, indirect discontinuities and continuous derivatives.
        It outputs an attached tensor which should subsequently be traversed by
        a call to `dr.forward`/`dr.backward`/`dr.enqueue`/`dr.traverse`.

        Note: The continuous derivatives are only attached if
        `radiative_backprop` is `False`. When using RB for the continuous
        derivatives it should be manually added to the gradient obtained by
        traversing the result of this method.
        """
        if isinstance(sensor, int):
            sensor = scene.sensors()[sensor]

        film = sensor.film()
        aovs = self.aov_names()
        shape = (film.crop_size()[1],
                 film.crop_size()[0],
                 film.base_channels_count() + len(aovs))
        result_img = dr.zeros(mi.TensorXf, shape=shape)

        sampler_spp = sensor.sampler().sample_count()
        sppc = self.override_spp(self.sppc, spp, sampler_spp)
        sppp = self.override_spp(self.sppp, spp, sampler_spp)
        sppi = self.override_spp(self.sppi, spp, sampler_spp)

        silhouette_shapes = scene.silhouette_shapes()
        has_silhouettes = len(silhouette_shapes) > 0

        # This isn't serious, so let's just warn once
        if has_silhouettes and not film.sample_border() and self.sample_border_warning:
            self.sample_border_warning = False
            mi.Log(mi.LogLevel.Warn,
                "PSIntegrator detected the potential for image-space "
                "motion due to differentiable shape parameters. To correctly "
                "account for shapes entering or leaving the viewport, it is "
                "recommended that you set the film's 'sample_border' parameter "
                "to True.")

        # Primarily visible discontinuous derivative
        if sppp > 0 and has_silhouettes:
            with dr.suspend_grad():
                self.proj_detail.init_primarily_visible_silhouette(scene, sensor)

            sampler, spp = self.prepare(sensor, 0xffffffff ^ seed, sppp, aovs)
            result_img += self.render_primarily_visible_silhouette(scene, sensor, sampler, spp)

        # Indirect discontinuous derivative
        if sppi > 0 and has_silhouettes:
            with dr.suspend_grad():
                self.proj_detail.init_indirect_silhouette(scene, sensor, 0xafafafaf ^ seed)

            sampler, spp = self.prepare(sensor, 0xaa00aa00 ^ seed, sppi, aovs)
            result_img += self.render_indirect_silhouette(scene, sensor, sampler, spp)

        ## Continuous derivative (only if radiative backpropagation is not used)
        if sppc > 0 and (not self.radiative_backprop):
            with dr.suspend_grad():
                sampler, spp = self.prepare(sensor, seed, sppc, aovs)
                ray, weight, pos = self.sample_rays(scene, sensor, sampler)

                # Launch the Monte Carlo sampling process in differentiable mode
                L, valid, aovs, _ = self.sample(
                    mode     = mode,
                    scene    = scene,
                    sampler  = sampler,
                    ray      = ray,
                    depth    = 0,
                    δL       = None,
                    state_in = None,
                    active   = mi.Bool(True),
                    project  = False,
                    si_shade = None
                )

            block = film.create_block()
            block.set_coalesce(block.coalesce() and sppc >= 4)

            ADIntegrator._splat_to_block(
                block, film, pos,
                value=L * weight,
                weight=1.0,
                alpha=dr.select(valid, mi.Float(1), mi.Float(0)),
                aovs=[aov * weight for aov in aovs],
                wavelengths=ray.wavelengths
            )

            film.put_block(block)
            result_img += film.develop()

        return result_img

    def render_forward(self,
                       scene: mi.Scene,
                       params: Any,
                       sensor: Union[int, mi.Sensor] = 0,
                       seed: mi.UInt32 = 0,
                       spp: int = 0) -> mi.TensorXf:
        if isinstance(sensor, int):
            sensor = scene.sensors()[sensor]

        film = sensor.film()
        shape = (film.crop_size()[1],
                 film.crop_size()[0],
                 film.base_channels_count() + len(self.aov_names()))
        result_grad = dr.zeros(mi.TensorXf, shape=shape)

        sampler_spp = sensor.sampler().sample_count()
        sppc = self.override_spp(self.sppc, spp, sampler_spp)
        sppp = self.override_spp(self.sppp, spp, sampler_spp)
        sppi = self.override_spp(self.sppi, spp, sampler_spp)

        # Continuous derivative (if RB is used)
        if self.radiative_backprop and sppc > 0:
            result_grad += RBIntegrator.render_forward(
                self, scene, None, sensor, seed, sppc)

        # Discontinuous derivative (and the non-RB continuous derivative)
        if sppp > 0 or sppi > 0 or \
           (sppc > 0 and not self.radiative_backprop):

            # Compute an image with all derivatives attached
            ad_img = self.render_ad(scene, sensor, seed, spp, dr.ADMode.Forward)

            # We should only complain about the parameters not being attached
            # if `ad_img` isn't attached and we haven't used RB for the
            # continuous derivatives.
            if dr.grad_enabled(ad_img) or not self.radiative_backprop:
                dr.forward_to(ad_img)
                grad_img = dr.grad(ad_img)
                result_grad += grad_img

        return result_grad

    def render_backward(self,
                        scene: mi.Scene,
                        params: Any,
                        grad_in: mi.TensorXf,
                        sensor: Union[int, mi.Sensor] = 0,
                        seed: mi.UInt32 = 0,
                        spp: int = 0) -> None:
        if isinstance(sensor, int):
            sensor = scene.sensors()[sensor]

        sampler_spp = sensor.sampler().sample_count()
        sppc = self.override_spp(self.sppc, spp, sampler_spp)
        sppp = self.override_spp(self.sppp, spp, sampler_spp)
        sppi = self.override_spp(self.sppi, spp, sampler_spp)

        # Continuous derivative (if RB is used)
        if self.radiative_backprop and sppc > 0:
            RBIntegrator.render_backward(
                self, scene, None, grad_in, sensor, seed, sppc)

        # Discontinuous derivative (and the non-RB continuous derivative)
        if sppp > 0 or sppi > 0 or \
           (sppc > 0 and not self.radiative_backprop):

            # Compute an image with all derivatives attached
            ad_img = self.render_ad(
                scene, sensor, seed, spp, dr.ADMode.Backward)

            dr.set_grad(ad_img, grad_in)
            dr.enqueue(dr.ADMode.Backward, ad_img)
            dr.traverse(dr.ADMode.Backward)

        dr.eval()

    ################# Primarily visible discontinuous derivative ###############

    def render_primarily_visible_silhouette(self,
                                            scene: mi.Scene,
                                            sensor: mi.Sensor,
                                            sampler: mi.Sampler,
                                            spp: int) -> mi.TensorXf:
        """
        Renders the primarily visible discontinuities.

        This method returns the AD-attached image. The result must still be
        traversed using one of the Dr.Jit functions to propagate gradients.
        """
        film = sensor.film()
        aovs = self.aov_names()

        # Explicit sampling to handle the primarily visible discontinuous derivative
        with dr.suspend_grad():
            # Get the viewpoint
            sensor_center = sensor.world_transform() @ mi.Point3f(0)

            # Sample silhouette point
            ss = self.proj_detail.sample_primarily_visible_silhouette(
                scene, sensor_center, sampler.next_2d(), True)
            active = ss.is_valid() & (ss.pdf > 0)

            # Jacobian (motion correction included)
            J = self.proj_detail.perspective_sensor_jacobian(sensor, ss)

            ΔL = self.proj_detail.eval_primary_silhouette_radiance_difference(
                scene, sampler, ss, sensor, active=active)
            active &= dr.any(ΔL != 0)

        # ∂z/∂ⲡ * normal
        si = dr.zeros(mi.SurfaceInteraction3f)
        si.p = ss.p
        si.prim_index = ss.prim_index
        si.uv = ss.uv
        p = ss.shape.differential_motion(dr.detach(si), active)
        motion = dr.dot(p, ss.n)

        # Compute the derivative
        derivative = ΔL * motion * dr.rcp(ss.pdf) * J

        # Prepare a new imageblock and compute splatting coordinates
        film.prepare(aovs)
        with dr.suspend_grad():
            it = dr.zeros(mi.Interaction3f)
            it.p = ss.p
            sensor_ds, _ = sensor.sample_direction(it, mi.Point2f(0))

        # Particle tracer style imageblock to accumulate primarily visible derivatives
        block = film.create_block(normalize=True)
        block.set_coalesce(block.coalesce() and spp >= 4)
        block.put(
            pos=sensor_ds.uv,
            wavelengths=[],
            value=derivative * dr.rcp(mi.ScalarFloat(spp)),
            weight=0,
            alpha=1,
            active=active
        )
        film.put_block(block)

        return film.develop()

    #################### Indirect discontinuous derivatives ####################

    def sample_radiance_difference(self, scene, ss, curr_depth, sampler, active):
        """
        Sample the radiance difference of two rays that hit and miss the
        silhouette point `ss.p` with direction `ss.d`.

        Parameters ``curr_depth`` (``mi.UInt32``):
            The current depth of the boundary segment, including the boundary
            segment itself.

        This function returns a tuple ``(ΔL, active)`` where

        Output ``ΔL`` (``mi.Spectrum``):
            The estimated radiance difference of the foreground and background.

        Output ``active`` (``mi.Bool``):
            Indicates if the radiance difference is valid.
        """
        raise Exception('PSIntegrator does not provide the '
                        'sample_radiance_difference() method. '
                        'It should be implemented by subclasses that '
                        'specialize the abstract PSIntegrator interface.')

    def sample_importance(self, scene, sensor, ss, max_depth, sampler,
                          preprocess, active):
        """
        Sample the incident importance at the silhouette point `ss.p` with
        direction `-ss.d`. If multiple connections to the sensor are valid, this
        method uses reservoir sampling to pick one.

        Parameters ``max_depth`` (``mi.UInt32``):
            The maximum number of ray segments to reach the sensor.

        The function returns a tuple ``(importance, uv, depth, boundary_p,
        valid)`` where

        Output ``importance`` (``mi.Spectrum``):
            The sampled importance along the constructed path.

        Output ``uv`` (``mi.Point2f``):
            The sensor splatting coordinates.

        Output ``depth`` (``mi.UInt32``):
            The number of segments of the sampled path from the boundary
            segment to the sensor, including the boundary segment itself.

        Output ``boundary_p`` (``mi.Point3f``):
            The attached sensor-side intersection point of the boundary segment.

        Output ``valid`` (``mi.Bool``):
            Indicates if a valid path is found.
        """
        raise Exception('PSIntegrator does not provide the '
                        'sample_importance() method. '
                        'It should be implemented by subclasses that '
                        'specialize the abstract PSIntegrator interface.')

    def render_indirect_silhouette(self,
                                   scene: mi.Scene,
                                   sensor: mi.Sensor,
                                   sampler: mi.Sampler,
                                   spp: int) -> mi.TensorXf:
        film = sensor.film()
        film.prepare(self.aov_names())

        if self.proj_detail.guiding_distr is not None:
            # Draw samples from the guiding distribution
            sample, rcp_pdf_guiding = self.proj_detail.guiding_distr.sample(sampler)

            # Evaluate the discontinuous derivative integrand
            value, sensor_uv = self.proj_detail.eval_indirect_integrand(
                scene, sensor, sample, sampler, preprocess=False)
            active = dr.any(value != 0)

            # Account for the guiding sampling density and spp
            value *= rcp_pdf_guiding * dr.rcp(spp)

            # Splat the result to the film
            block = film.create_block(normalize=True)
            block.set_coalesce(block.coalesce() and spp >= 4)
            block.put(
                pos=sensor_uv,
                wavelengths=[],
                value=value,
                weight=0,
                alpha=1,
                active=active
            )
            film.put_block(block)

        return film.develop()

    ########################### Integrator interface ###########################

    def sample(self,
               mode: dr.ADMode,
               scene: mi.Scene,
               sampler: mi.Sampler,
               ray: mi.Ray3f,
               depth: mi.UInt32,
               δL: Optional[mi.Spectrum],
               δaovs: Optional[mi.Spectrum],
               state_in: Any,
               active: mi.Bool,
               project: bool = False,
               si_shade: Optional[mi.SurfaceInteraction3f] = None
    ) -> Tuple[mi.Spectrum, mi.Bool, List[mi.Float], Any]:
        """
        See ADIntegrator.sample() for a description of this function's purpose.

        Parameter ``depth`` (``mi.UInt32``):
            Path depth of `ray` (typically set to zero). This is mainly useful
            for forward/backward differentiable rendering phases that need to
            obtain an incident radiance estimate. In this case, they may
            recursively invoke ``sample(mode=dr.ADMode.Primal)`` with a nonzero
            depth.

        Parameter ``project`` (``bool``):
            If set to ``True``, the integrator also returns the sampled
            ``seedrays`` along the Monte Carlo path. This is useful for
            projective integrators to handle discontinuous derivatives.

        Parameter ``si_shade`` (``mi.SurfaceInteraction3f``):
            If set to a valid surface interaction, the integrator will use this
            as the first ray interaction point to skip one ray tracing with the
            given ``ray``. This is useful to estimate the incident radiance at a
            given surface point that is already known to the integrator.

        Output ``spec`` (``mi.Spectrum``):
            Specifies the estimated radiance and differential radiance in primal
            and forward mode, respectively.

        Output ``valid`` (``mi.Bool``):
            Indicates whether the rays intersected a surface, which can be used
            to compute an alpha channel.

        Output ``aovs`` (``Sequence[mi.Float]``):
            Integrators may return one or more arbitrary output variables (AOVs).
            The implementation has to guarantee that the number of returned AOVs
            matches the length of self.aov_names().

        Output ``seedray`` / ``state_out`` (``any``):
            If ``project`` is true, the integrator returns the seed rays to be
            projected as the third output. The seed rays is a python list of
            rays and their validity mask. It is possible that no segment can be
            projected along a light path.

            If ``project`` is false, the integrator returns the state vector
            returned by the primal phase of ``sample()`` as the third output.
            This is only used by the radiative-backpropagation style
            integrators.
        """

        raise Exception('PSIntegrator does not provide the sample() method. '
                        'It should be implemented by subclasses that '
                        'specialize the abstract PSIntegrator interface.')

# ---------------------------------------------------------------------------
#  Helper functions used by various differentiable integrators
# ---------------------------------------------------------------------------

def mis_weight(pdf_a, pdf_b):
    """
    Compute the Multiple Importance Sampling (MIS) weight given the densities
    of two sampling strategies according to the power heuristic.
    """
    a2 = dr.square(pdf_a)
    b2 = dr.square(pdf_b)
    w = a2 / (a2 + b2)
    return dr.detach(dr.select(dr.isfinite(w), w, 0))
