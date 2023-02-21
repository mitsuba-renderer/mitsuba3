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

    def __init__(self, props = mi.Properties()):
        super().__init__(props)

        max_depth = props.get('max_depth', 6)
        if max_depth < 0 and max_depth != -1:
            raise Exception("\"max_depth\" must be set to -1 (infinite) or a value >= 0")

        # Map -1 (infinity) to 2^32-1 bounces
        self.max_depth = max_depth if max_depth != -1 else 0xffffffff

        self.rr_depth = props.get('rr_depth', 5)
        if self.rr_depth <= 0:
            raise Exception("\"rr_depth\" must be set to a value greater than zero!")

        # Warn about potential bias due to shapes entering/leaving the frame
        self.sample_border_warning = True

    def aovs(self):
        return []

    def to_string(self):
        return f'{type(self).__name__}[max_depth = {self.max_depth},' \
               f' rr_depth = { self.rr_depth }]'

    def render(self: mi.SamplingIntegrator,
               scene: mi.Scene,
               sensor: Union[int, mi.Sensor] = 0,
               seed: int = 0,
               spp: int = 0,
               develop: bool = True,
               evaluate: bool = True) -> mi.TensorXf:

        if not develop:
            raise Exception("develop=True must be specified when "
                            "invoking AD integrators")

        if isinstance(sensor, int):
            sensor = scene.sensors()[sensor]

        # Disable derivatives in all of the following
        with dr.suspend_grad():
            # Prepare the film and sample generator for rendering
            sampler, spp = self.prepare(
                sensor=sensor,
                seed=seed,
                spp=spp,
                aovs=self.aovs()
            )

            # Generate a set of rays starting at the sensor
            ray, weight, pos, _ = self.sample_rays(scene, sensor, sampler)

            # Launch the Monte Carlo sampling process in primal mode
            L, valid, state = self.sample(
                mode=dr.ADMode.Primal,
                scene=scene,
                sampler=sampler,
                ray=ray,
                depth=mi.UInt32(0),
                δL=None,
                state_in=None,
                reparam=None,
                active=mi.Bool(True)
            )

            # Prepare an ImageBlock as specified by the film
            block = sensor.film().create_block()

            # Only use the coalescing feature when rendering enough samples
            block.set_coalesce(block.coalesce() and spp >= 4)

            # Accumulate into the image block
            alpha = dr.select(valid, mi.Float(1), mi.Float(0))
            if mi.has_flag(sensor.film().flags(), mi.FilmFlags.Special):
                aovs = sensor.film().prepare_sample(L * weight, ray.wavelengths,
                                                    block.channel_count(), alpha=alpha)
                block.put(pos, aovs)
                del aovs
            else:
                block.put(pos, ray.wavelengths, L * weight, alpha)

            # Explicitly delete any remaining unused variables
            del sampler, ray, weight, pos, L, valid, alpha
            gc.collect()

            # Perform the weight division and return an image tensor
            sensor.film().put_block(block)
            self.primal_image = sensor.film().develop()

            return self.primal_image

    def render_forward(self: mi.SamplingIntegrator,
                       scene: mi.Scene,
                       params: Any,
                       sensor: Union[int, mi.Sensor] = 0,
                       seed: int = 0,
                       spp: int = 0) -> mi.TensorXf:

        if isinstance(sensor, int):
            sensor = scene.sensors()[sensor]

        film = sensor.film()
        aovs = self.aovs()

        # Disable derivatives in all of the following
        with dr.suspend_grad():
            # Prepare the film and sample generator for rendering
            sampler, spp = self.prepare(sensor, seed, spp, aovs)

            # When the underlying integrator supports reparameterizations,
            # perform necessary initialization steps and wrap the result using
            # the _ReparamWrapper abstraction defined above
            if hasattr(self, 'reparam'):
                reparam = _ReparamWrapper(
                    scene=scene,
                    params=params,
                    reparam=self.reparam,
                    wavefront_size=sampler.wavefront_size(),
                    seed=seed
                )
            else:
                reparam = None

            # Generate a set of rays starting at the sensor, keep track of
            # derivatives wrt. sample positions ('pos') if there are any
            ray, weight, pos, det = self.sample_rays(scene, sensor,
                                                     sampler, reparam)

            with dr.resume_grad():
                L, valid, _ = self.sample(
                    mode=dr.ADMode.Forward,
                    scene=scene,
                    sampler=sampler,
                    ray=ray,
                    reparam=reparam,
                    active=mi.Bool(True)
                )

                block = film.create_block()
                # Only use the coalescing feature when rendering enough samples
                block.set_coalesce(block.coalesce() and spp >= 4)

                # Deposit samples with gradient tracking for 'pos'.
                # After reparameterizing the camera ray, we need to evaluate
                #   Σ (fi Li det)
                #  ---------------
                #   Σ (fi det)
                if (dr.all(mi.has_flag(sensor.film().flags(), mi.FilmFlags.Special))):
                    aovs = sensor.film().prepare_sample(L * weight * det, ray.wavelengths,
                                                        block.channel_count(),
                                                        weight=det,
                                                        alpha=dr.select(valid, mi.Float(1), mi.Float(0)))
                    block.put(pos, aovs)
                    del aovs
                else:
                    block.put(
                        pos=pos,
                        wavelengths=ray.wavelengths,
                        value=L * weight * det,
                        weight=det,
                        alpha=dr.select(valid, mi.Float(1), mi.Float(0))
                    )

                # Perform the weight division and return an image tensor
                film.put_block(block)
                result_img = film.develop()

                dr.forward_to(result_img)

        return dr.grad(result_img)

    def render_backward(self: mi.SamplingIntegrator,
                        scene: mi.Scene,
                        params: Any,
                        grad_in: mi.TensorXf,
                        sensor: Union[int, mi.Sensor] = 0,
                        seed: int = 0,
                        spp: int = 0) -> None:

        if isinstance(sensor, int):
            sensor = scene.sensors()[sensor]

        film = sensor.film()
        aovs = self.aovs()

        # Disable derivatives in all of the following
        with dr.suspend_grad():
            # Prepare the film and sample generator for rendering
            sampler, spp = self.prepare(sensor, seed, spp, aovs)

            # When the underlying integrator supports reparameterizations,
            # perform necessary initialization steps and wrap the result using
            # the _ReparamWrapper abstraction defined above
            if hasattr(self, 'reparam'):
                reparam = _ReparamWrapper(
                    scene=scene,
                    params=params,
                    reparam=self.reparam,
                    wavefront_size=sampler.wavefront_size(),
                    seed=seed
                )
            else:
                reparam = None

            # Generate a set of rays starting at the sensor, keep track of
            # derivatives wrt. sample positions ('pos') if there are any
            ray, weight, pos, det = self.sample_rays(scene, sensor,
                                                     sampler, reparam)

            with dr.resume_grad():
                L, valid, _ = self.sample(
                    mode=dr.ADMode.Backward,
                    scene=scene,
                    sampler=sampler,
                    ray=ray,
                    reparam=reparam,
                    active=mi.Bool(True)
                )

                # Prepare an ImageBlock as specified by the film
                block = film.create_block()

                # Only use the coalescing feature when rendering enough samples
                block.set_coalesce(block.coalesce() and spp >= 4)

                # Accumulate into the image block
                if mi.has_flag(sensor.film().flags(), mi.FilmFlags.Special):
                    aovs = sensor.film().prepare_sample(L * weight * det, ray.wavelengths,
                                                        block.channel_count(),
                                                        weight=det,
                                                        alpha=dr.select(valid, mi.Float(1), mi.Float(0)))
                    block.put(pos, aovs)
                    del aovs
                else:
                    block.put(
                        pos=pos,
                        wavelengths=ray.wavelengths,
                        value=L * weight * det,
                        weight=det,
                        alpha=dr.select(valid, mi.Float(1), mi.Float(0))
                    )

                sensor.film().put_block(block)

                del valid
                gc.collect()

                # This step launches a kernel
                dr.schedule(block.tensor())
                image = sensor.film().develop()

                # Differentiate sample splatting and weight division steps to
                # retrieve the adjoint radiance
                dr.set_grad(image, grad_in)
                dr.enqueue(dr.ADMode.Backward, image)
                dr.traverse(mi.Float, dr.ADMode.Backward)

            # We don't need any of the outputs here
            del ray, weight, pos, block, sampler
            gc.collect()

            # Run kernel representing side effects of the above
            dr.eval()

    def sample_rays(
        self,
        scene: mi.Scene,
        sensor: mi.Sensor,
        sampler: mi.Sampler,
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
        pos.x = dr.fma(-film_size[0], pos.y, idx)

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
            if not film.sample_border() and self.sample_border_warning:
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
                pos_f = ds.uv + film.crop_offset()

        # With box filter, ignore random offset to prevent numerical instabilities
        splatting_pos = mi.Vector2f(pos) if rfilter.is_box_filter() else pos_f

        return ray, weight, splatting_pos, reparam_det

    def prepare(self,
                sensor: mi.Sensor,
                seed: int = 0,
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

    def sample(self,
               mode: dr.ADMode,
               scene: mi.Scene,
               sampler: mi.Sampler,
               ray: mi.Ray3f,
               depth: mi.UInt32,
               δL: Optional[mi.Spectrum],
               state_in: Any,
               reparam: Optional[
                   Callable[[mi.Ray3f, mi.UInt32, mi.Bool],
                            Tuple[mi.Vector3f, mi.Float]]],
               active: mi.Bool) -> Tuple[mi.Spectrum, mi.Bool]:
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

        Parameter ``reparam`` (see above):
            If provided, this callable takes a ray and a mask of active SIMD
            lanes and returns a reparameterized ray and Jacobian determinant.
            The implementation of the ``sample`` function should then use it to
            correctly account for visibility-induced discontinuities during
            differentiation.

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
                       seed: int = 0,
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
        aovs = self.aovs()

        # Disable derivatives in all of the following
        with dr.suspend_grad():
            # Prepare the film and sample generator for rendering
            sampler, spp = self.prepare(sensor, seed, spp, aovs)

            # When the underlying integrator supports reparameterizations,
            # perform necessary initialization steps and wrap the result using
            # the _ReparamWrapper abstraction defined above
            if hasattr(self, 'reparam'):
                reparam = _ReparamWrapper(
                    scene=scene,
                    params=params,
                    reparam=self.reparam,
                    wavefront_size=sampler.wavefront_size(),
                    seed=seed
                )
            else:
                reparam = None

            # Generate a set of rays starting at the sensor, keep track of
            # derivatives wrt. sample positions ('pos') if there are any
            ray, weight, pos, det = self.sample_rays(scene, sensor,
                                                     sampler, reparam)

            # Launch the Monte Carlo sampling process in primal mode (1)
            L, valid, state_out = self.sample(
                mode=dr.ADMode.Primal,
                scene=scene,
                sampler=sampler.clone(),
                ray=ray,
                depth=mi.UInt32(0),
                δL=None,
                state_in=None,
                reparam=None,
                active=mi.Bool(True)
            )

            # Differentiable camera pose parameters or a reparameterization
            # have an effect on the measurement integral performed at the
            # sensor. We account for this here by differentiating the
            # 'ImageBlock.put()' operation using differentiable sample
            # positions. One important aspect of how this operation works in
            # Mitsuba is that it computes a separate 'weight' channel
            # containing the (potentially quite non-uniform) accumulated filter
            # weights of all samples. This non-uniformity is then divided out
            # at the end. It's crucial that we also account for this when
            # computing derivatives, or they will be unusably noisy.

            sample_pos_deriv = None # disable by default

            with dr.resume_grad():
                if dr.grad_enabled(pos):
                    sample_pos_deriv = film.create_block()

                    # Only use the coalescing feature when rendering enough samples
                    sample_pos_deriv.set_coalesce(sample_pos_deriv.coalesce() and spp >= 4)

                    # Deposit samples with gradient tracking for 'pos'.
                    if (dr.all(mi.has_flag(sensor.film().flags(), mi.FilmFlags.Special))):
                        aovs = sensor.film().prepare_sample(L * weight * det, ray.wavelengths,
                                                            sample_pos_deriv.channel_count(),
                                                            weight=det,
                                                            alpha=dr.select(valid, mi.Float(1), mi.Float(0)))
                        sample_pos_deriv.put(pos, aovs)
                        del aovs
                    else:
                        sample_pos_deriv.put(
                            pos=pos,
                            wavelengths=ray.wavelengths,
                            value=L * weight * det,
                            weight=det,
                            alpha=dr.select(valid, mi.Float(1), mi.Float(0))
                        )

                    # Compute the derivative of the reparameterized image ..
                    tensor = sample_pos_deriv.tensor()
                    dr.forward_to(tensor,
                                  flags=dr.ADFlag.ClearInterior | dr.ADFlag.ClearEdges)

                    dr.schedule(tensor, dr.grad(tensor))

                    # Done with this part, let's detach the image-space position
                    dr.disable_grad(pos)
                    del tensor

            # Probably a little overkill, but why not.. If there are any
            # DrJit arrays to be collected by Python's cyclic GC, then
            # freeing them may enable loop simplifications in dr.eval().
            gc.collect()

            # Launch a kernel with everything so far
            dr.eval(state_out)

            # Garbage collect unused values to simplify kernel about to be run
            del L, valid, params

            # Launch the Monte Carlo sampling process in forward mode
            δL, valid_2, state_out_2 = self.sample(
                mode=dr.ADMode.Forward,
                scene=scene,
                sampler=sampler,
                ray=ray,
                depth=mi.UInt32(0),
                δL=None,
                state_in=state_out,
                reparam=reparam,
                active=mi.Bool(True)
            )

            # Prepare an ImageBlock as specified by the film
            block = film.create_block()

            # Only use the coalescing feature when rendering enough samples
            block.set_coalesce(block.coalesce() and spp >= 4)

            # Accumulate into the image block
            if (dr.all(mi.has_flag(sensor.film().flags(), mi.FilmFlags.Special))):
                aovs = sensor.film().prepare_sample(δL * weight, ray.wavelengths,
                                                    block.channel_count(),
                                                    alpha=dr.select(valid_2, mi.Float(1), mi.Float(0)))
                block.put(pos, aovs)
                del aovs
            else:
                block.put(
                    pos=pos,
                    wavelengths=ray.wavelengths,
                    value=δL * weight,
                    alpha=dr.select(valid_2, mi.Float(1), mi.Float(0))
                )

            # Perform the weight division and return an image tensor
            film.put_block(block)

            # Explicitly delete any remaining unused variables
            del sampler, ray, weight, pos, δL, valid_2, \
                state_out, state_out_2, block

            gc.collect()

            result_grad = film.develop()

            # Potentially add the derivative of the reparameterized samples
            if sample_pos_deriv is not None:
                with dr.resume_grad():
                    film.prepare(aovs)
                    film.put_block(sample_pos_deriv)
                    reparam_result = film.develop()
                    dr.forward_to(reparam_result)
                    result_grad += dr.grad(reparam_result)

        return result_grad

    def render_backward(self: mi.SamplingIntegrator,
                        scene: mi.Scene,
                        params: Any,
                        grad_in: mi.TensorXf,
                        sensor: Union[int, mi.Sensor] = 0,
                        seed: int = 0,
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
        aovs = self.aovs()

        # Disable derivatives in all of the following
        with dr.suspend_grad():
            # Prepare the film and sample generator for rendering
            sampler, spp = self.prepare(sensor, seed, spp, aovs)

            # When the underlying integrator supports reparameterizations,
            # perform necessary initialization steps and wrap the result using
            # the _ReparamWrapper abstraction defined above
            if hasattr(self, 'reparam'):
                reparam = _ReparamWrapper(
                    scene=scene,
                    params=params,
                    reparam=self.reparam,
                    wavefront_size=sampler.wavefront_size(),
                    seed=seed
                )
            else:
                reparam = None

            # Generate a set of rays starting at the sensor, keep track of
            # derivatives wrt. sample positions ('pos') if there are any
            ray, weight, pos, det = self.sample_rays(scene, sensor,
                                                     sampler, reparam)

            # Launch the Monte Carlo sampling process in primal mode (1)
            L, valid, state_out = self.sample(
                mode=dr.ADMode.Primal,
                scene=scene,
                sampler=sampler.clone(),
                ray=ray,
                depth=mi.UInt32(0),
                δL=None,
                state_in=None,
                reparam=None,
                active=mi.Bool(True)
            )

            # Prepare an ImageBlock as specified by the film
            block = film.create_block()

            # Only use the coalescing feature when rendering enough samples
            block.set_coalesce(block.coalesce() and spp >= 4)

            with dr.resume_grad():
                dr.enable_grad(L)

                # Accumulate into the image block.
                # After reparameterizing the camera ray, we need to evaluate
                #   Σ (fi Li det)
                #  ---------------
                #   Σ (fi det)
                if (dr.all(mi.has_flag(sensor.film().flags(), mi.FilmFlags.Special))):
                    aovs = sensor.film().prepare_sample(L * weight * det, ray.wavelengths,
                                                        block.channel_count(),
                                                        weight=det,
                                                        alpha=dr.select(valid, mi.Float(1), mi.Float(0)))
                    block.put(pos, aovs)
                    del aovs
                else:
                    block.put(
                        pos=pos,
                        wavelengths=ray.wavelengths,
                        value=L * weight * det,
                        weight=det,
                        alpha=dr.select(valid, mi.Float(1), mi.Float(0))
                    )

                sensor.film().put_block(block)

                # Probably a little overkill, but why not.. If there are any
                # DrJit arrays to be collected by Python's cyclic GC, then
                # freeing them may enable loop simplifications in dr.eval().
                del valid
                gc.collect()

                # This step launches a kernel
                dr.schedule(state_out, block.tensor())
                image = sensor.film().develop()

                # Differentiate sample splatting and weight division steps to
                # retrieve the adjoint radiance
                dr.set_grad(image, grad_in)
                dr.enqueue(dr.ADMode.Backward, image)
                dr.traverse(mi.Float, dr.ADMode.Backward)
                δL = dr.grad(L)

            # Launch Monte Carlo sampling in backward AD mode (2)
            L_2, valid_2, state_out_2 = self.sample(
                mode=dr.ADMode.Backward,
                scene=scene,
                sampler=sampler,
                ray=ray,
                depth=mi.UInt32(0),
                δL=δL,
                state_in=state_out,
                reparam=reparam,
                active=mi.Bool(True)
            )

            # We don't need any of the outputs here
            del L_2, valid_2, state_out, state_out_2, δL, \
                ray, weight, pos, block, sampler

            gc.collect()

            # Run kernel representing side effects of the above
            dr.eval()

# ---------------------------------------------------------------------------
# Default implementation of Integrator.render_forward/backward
# ---------------------------------------------------------------------------

def render_forward(self: mi.Integrator,
                   scene: mi.Scene,
                   params: Any,
                   sensor: Union[int, mi.Sensor] = 0,
                   seed: int = 0,
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
    that can be obtained obtained via a call to ``mi.traverse()``.

    Note the default implementation of this functionality relies on naive
    automatic differentiation (AD), which records a computation graph of the
    primal rendering step that is subsequently traversed to propagate
    derivatives. This tends to be relatively inefficient due to the need to
    track intermediate program state. In particular, it means that
    differentiation of nontrivial scenes at high sample counts will often run
    out of memory. Integrators like ``rb`` (Radiative Backpropagation) and
    ``prb`` (Path Replay Backpropagation) that are specifically designed for
    differentiation can be significantly more efficient.

    Parameter ``scene`` (``mi.Scene``):
        The scene to be rendered differentially.

    Parameter ``params``:
       An arbitrary container of scene parameters that should receive
       gradients. Typically this will be an instance of type
       ``mi.SceneParameters`` obtained via ``mi.traverse()``. However, it could
       also be a Python list/dict/object tree (DrJit will traverse it to find
       all parameters). Gradient tracking must be explicitly enabled for each of
       these parameters using ``dr.enable_grad(params['parameter_name'])`` (i.e.
       ``render_forward()`` will not do this for you). Furthermore,
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

    # Recorded loops cannot be differentiated, so let's disable them
    with dr.scoped_set_flag(dr.JitFlag.LoopRecord, False):
        image = self.render(
            scene=scene,
            sensor=sensor,
            seed=seed,
            spp=spp,
            develop=True,
            evaluate=False
        )

        # Perform an AD traversal of all registered AD variables that
        # influence 'image' in a differentiable manner
        dr.forward_to(image)

        return dr.grad(image)

def render_backward(self: mi.Integrator,
                    scene: mi.Scene,
                    params: Any,
                    grad_in: mi.TensorXf,
                    sensor: Union[int, mi.Sensor] = 0,
                    seed: int = 0,
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
    to ``mi.traverse()``. Use ``dr.grad()`` to query the resulting gradients of
    these parameters once ``render_backward()`` returns.

    Note the default implementation of this functionality relies on naive
    automatic differentiation (AD), which records a computation graph of the
    primal rendering step that is subsequently traversed to propagate
    derivatives. This tends to be relatively inefficient due to the need to
    track intermediate program state. In particular, it means that
    differentiation of nontrivial scenes at high sample counts will often run
    out of memory. Integrators like ``rb`` (Radiative Backpropagation) and
    ``prb`` (Path Replay Backpropagation) that are specifically designed for
    differentiation can be significantly more efficient.

    Parameter ``scene`` (``mi.Scene``):
        The scene to be rendered differentially.

    Parameter ``params``:
       An arbitrary container of scene parameters that should receive
       gradients. Typically this will be an instance of type
       ``mi.SceneParameters`` obtained via ``mi.traverse()``. However, it could
       also be a Python list/dict/object tree (DrJit will traverse it to find
       all parameters). Gradient tracking must be explicitly enabled for each of
       these parameters using ``dr.enable_grad(params['parameter_name'])`` (i.e.
       ``render_backward()`` will not do this for you).

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

    # Recorded loops cannot be differentiated, so let's disable them
    with dr.scoped_set_flag(dr.JitFlag.LoopRecord, False):
        image = self.render(
            scene=scene,
            sensor=sensor,
            seed=seed,
            spp=spp,
            develop=True,
            evaluate=False
        )

        # Process the computation graph using reverse-mode AD
        dr.backward_from(image * grad_in)

# Monkey-patch render_forward/backward into the Integrator base class
mi.Integrator.render_backward = render_backward
mi.Integrator.render_forward = render_forward

del render_backward
del render_forward

# ------------------------------------------------------------------------------

class _ReparamWrapper:
    """
    This class is an implementation detail of ``ADIntegrator``, which performs
    necessary initialization steps and subsequently wraps a reparameterization
    technique. It serves the following important purposes:

    1. Ensuring the availability of uncorrelated random variates.
    2. Connecting reparameterization calls to relevant shape-related
       variables in the AD graph.
    3. Exposing the underlying RNG state to recorded loops.
    """

    # ReparamWrapper instances can be provided as dr.Loop state
    # variables. For this to work we must declare relevant fields
    DRJIT_STRUCT = { 'rng' : mi.PCG32 }

    def __init__(self,
                 scene : mi.Scene,
                 params: Any,
                 reparam: Callable[
                     [mi.Scene, mi.PCG32, Any,
                      mi.Ray3f, mi.UInt32, mi.Bool],
                     Tuple[mi.Vector3f, mi.Float]],
                 wavefront_size : int,
                 seed : int):

        self.scene = scene
        self.params = params
        self.reparam = reparam

        # Only link the reparameterization CustomOp to differentiable scene
        # parameters with the AD computation graph if they control shape
        # information (vertex positions, etc.)
        if isinstance(params, mi.SceneParameters):
            params = params.copy()
            params.keep(
                [
                    k for k in params.keys() \
                        if (params.flags(k) & mi.ParamFlags.Discontinuous) != 0
                ]
            )

        # Create a uniform random number generator that won't show any
        # correlation with the main sampler. PCG32Sampler.seed() uses
        # the same logic except for the XOR with -1

        idx = dr.arange(mi.UInt32, wavefront_size)
        tmp = dr.opaque(mi.UInt32, 0xffffffff ^ seed)
        v0, v1 = mi.sample_tea_32(tmp, idx)
        self.rng = mi.PCG32(initstate=v0, initseq=v1)

    def __call__(self,
                 ray: mi.Ray3f,
                 depth: mi.UInt32,
                 active: Union[mi.Bool, bool] = True
    ) -> Tuple[mi.Vector3f, mi.Float]:
        """
        This function takes a ray, a path depth value (to potentially disable
        reparameterizations after a certain number of bounces) and a boolean
        active mask as input and returns the reparameterized ray direction and
        the Jacobian determinant of the change of variables.
        """
        return self.reparam(self.scene, self.rng, self.params, ray,
                            depth, active)


# ---------------------------------------------------------------------------
#  Helper functions used by various differentiable integrators
# ---------------------------------------------------------------------------

def mis_weight(pdf_a, pdf_b):
    """
    Compute the Multiple Importance Sampling (MIS) weight given the densities
    of two sampling strategies according to the power heuristic.
    """
    a2 = dr.sqr(pdf_a)
    b2 = dr.sqr(pdf_b)
    w = a2 / (a2 + b2)
    return dr.detach(dr.select(dr.isfinite(w), w, 0))
