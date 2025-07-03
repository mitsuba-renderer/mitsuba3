from __future__ import annotations # Delayed parsing of type annotations

import drjit as dr
import mitsuba as mi

from mitsuba.python.ad.integrators.common import ADIntegrator, RBIntegrator, PSIntegrator, mis_weight
from mitsuba.python.ad.integrators.prb_projective import PathProjectiveIntegrator

from .ad_threepoint import ThreePointIntegrator
from .prb_threepoint import PRBThreePointIntegrator

from .common import det_over_det, sensor_to_surface_reparam_det

class PathProjectiveFixIntegrator(PathProjectiveIntegrator):
    r"""
    .. _integrator-prb_projective:

    Projective sampling Path Replay Backpropagation (PRB) (:monosp:`prb_projective`)
    --------------------------------------------------------------------------------

    .. pluginparameters::

     * - max_depth
       - |int|
       - Specifies the longest path depth in the generated output image (where -1
         corresponds to :math:`\infty`). A value of 1 will only render directly
         visible light sources. 2 will lead to single-bounce (direct-only)
         illumination, and so on. (Default: -1)

     * - rr_depth
       - |int|
       - Specifies the path depth, at which the implementation will begin to use
         the *russian roulette* path termination criterion. For example, if set to
         1, then path generation many randomly cease after encountering directly
         visible surfaces. (Default: 5)

     * - sppc
       - |int|
       - Number of samples per pixel used to estimate the continuous
         derivatives. This is overriden by whatever runtime `spp` value is
         passed to the `render()` method. If this value was not set and no
         runtime value `spp` is used, the `sample_count` of the film's sampler
         will be used.

     * - sppp
       - |int|
       - Number of samples per pixel used to to estimate the gradients resulting
         from primary visibility changes (on the first segment of the light
         path: from the sensor to the first bounce) derivatives. This is
         overriden by whatever runtime `spp` value is passed to the `render()`
         method. If this value was not set and no runtime value `spp` is used,
         the `sample_count` of the film's sampler will be used.

     * - sppi
       - |int|
       - Number of samples per pixel used to to estimate the gradients resulting
         from indirect visibility changes  derivatives. This is overriden by
         whatever runtime `spp` value is passed to the `render()` method. If
         this value was not set and no runtime value `spp` is used, the
         `sample_count` of the film's sampler will be used.

     * - guiding
       - |string|
       - Guiding type, must be one of: "none", "grid", or "octree". This
         specifies the guiding method used for indirectly observed
         discontinuities. (Default: "octree")

     * - guiding_proj
       - |bool|
       - Whether or not to use projective sampling to generate the set of
         samples that are used to build the guiding structure. (Default: True)

     * - guiding_rounds
       - |int|
       - Number of sampling iterations used to build the guiding data structure.
         A higher number of rounds will use more samples and hence should result
         in a more accurate guiding structure. (Default: 1)

    This plugin implements a projective sampling Path Replay Backpropagation
    (PRB) integrator with the following features:

    - Emitter sampling (a.k.a. next event estimation).

    - Russian Roulette stopping criterion.

    - Projective sampling. This means that it can handle discontinuous
      visibility changes, such as a moving shape's gradients.

    - Detached sampling. This means that the properties of ideal specular
      objects (e.g., the IOR of a glass vase) cannot be optimized.

    In order to estimate the indirect visibility discontinuous derivatives, this
    integrator starts by sampling a boundary segment and then attempts to
    connect it to the sensor and an emitter. It is effectively building lights
    paths from the middle outwards. In order to stay within the specified
    `max_depth`, the integrator starts by sampling a path to the sensor by using
    reservoir sampling to decide whether or not to use a longer path. Once a
    path to the sensor is found, the other half of the full light path is
    sampled.

    See the paper :cite:`Zhang2023Projective` for details on projective
    sampling, and guiding structures for indirect visibility discontinuities.

    .. warning::
        This integrator is not supported in variants which track polarization
        states.

    .. tabs::

        .. code-tab:: python

            'type': 'prb_projective',
            'sppc': 32,
            'sppp': 32,
            'sppi': 128,
            'guiding': 'octree',
            'guiding_proj': True,
            'guiding_rounds': 1
    """

    def __init__(self, props):
        super().__init__(props)

        # Override the radiative backpropagation flag to allow the parent class
        # to call the sample() method following the logics defined in the
        # ``PRBThreePointIntegrator``
        self.radiative_backprop = props.get('radiative_backprop', True)

        # Specify the seed ray generation strategy
        self.project_seed = props.get('project_seed', 'both')
        if self.project_seed not in ['both', 'bsdf', 'emitter']:
            raise Exception(f"Project seed must be one of 'both', 'bsdf', "
                            f"'emitter', got '{self.project_seed}'")
        
        # Indicator if interior derivative should be included
        # (sppc cannot be used to disable it if spp is overwritten in runtime)
        self.include_interior = props.get('include_interior', True)

        # Indicator if the boundary derivative should be included
        self.include_boundary = props.get('include_boundary', True)

    def rb_render_forward(self: mi.SamplingIntegrator,
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


            # Launch the Monte Carlo sampling process in primal mode (1)
            L, valid, aovs, state_out = PRBThreePointIntegrator.sample(self,
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
            δL, valid_2, δaovs, state_out_2 = PRBThreePointIntegrator.sample(self,
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

            # The shared state contains the first intersection point
            pi = state_out[1]

            with dr.resume_grad():
                # Prepare an ImageBlock as specified by the film
                block = film.create_block(normalize=True)

                # Only use the coalescing feature when rendering enough samples
                block.set_coalesce(block.coalesce() and spp >= 4)

                # Recompute the first intersection point with derivative tracking
                si = pi.compute_surface_interaction(ray, 
                                                    ray_flags=mi.RayFlags.All | mi.RayFlags.FollowShape,
                                                    active=valid)
                pos = dr.select(valid, sensor.sample_direction(si, [0, 0], active=valid)[0].uv, pos)

                D = sensor_to_surface_reparam_det(sensor, si, ignore_near_plane=True, active=valid)

                # Accumulate into the image block
                # particle-style to match the measurement of the boundary term
                ADIntegrator._splat_to_block(
                    block, film, pos,
                    value=L * weight * dr.rcp(mi.ScalarFloat(spp)) * det_over_det(D),
                    weight=0,
                    alpha=dr.select(valid, mi.Float(1), mi.Float(0)),
                    aovs=aovs,
                    wavelengths=ray.wavelengths
                )
                # Perform the weight division
                film.put_block(block)
                
                result_img = film.develop()

                # Propagate the gradients to the image tensor
                dr.forward_to(result_img, flags=dr.ADFlag.ClearNone | dr.ADFlag.AllowNoGrad)
                first_hit = dr.grad(result_img)

            film.clear()

            # Prepare an ImageBlock as specified by the film
            block = film.create_block(normalize=True)

            # Only use the coalescing feature when rendering enough samples
            block.set_coalesce(block.coalesce() and spp >= 4)

            # Accumulate into the image block
            # particle-style to match the measurement of the boundary term
            ADIntegrator._splat_to_block(
                block, film, pos,
                value=δL * weight * dr.rcp(mi.ScalarFloat(spp)),
                weight=0,
                alpha=dr.select(valid_2, mi.Float(1), mi.Float(0)),
                aovs=[δaov * weight for δaov in δaovs],
                wavelengths=ray.wavelengths
            )
            
            # Perform the weight division and return an image tensor
            film.put_block(block)
            result_grad = film.develop()


        # Explicitly delete any remaining unused variables
        del sampler, ray, weight, pos, L, valid, aovs, δL, δaovs, \
            valid_2, params, state_out, state_out_2#, block
        
        return result_grad + first_hit
    
    def rb_render_backward(self: mi.SamplingIntegrator,
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

            def splatting_and_backward_gradient_image(value: mi.Spectrum,
                                                      weight: mi.Float,
                                                      alpha: mi.Float,
                                                      aovs: Sequence[mi.Float]):
                '''
                Backward propagation of the gradient image through the sample
                splatting and weight division steps.
                '''

                # Prepare an ImageBlock as specified by the film
                block = film.create_block(normalize=True)

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
                        value=L * weight * dr.rcp(mi.ScalarFloat(spp)),
                        weight=0,
                        alpha=1.0,
                        aovs=[aov * weight for aov in aovs]
                    )

                    δL = dr.grad(L)
                    δaovs = dr.grad(aovs)

            # Clear the dummy data splatted on the film above
            film.clear()

            # Launch the Monte Carlo sampling process in primal mode (1)
            L, valid, aovs, state_out = PRBThreePointIntegrator.sample(self,
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
            L_2, valid_2, aovs_2, state_out_2 = PRBThreePointIntegrator.sample(self,
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

            # The shared state contains the first intersection point
            pi = state_out[1]

            with dr.resume_grad():
                # Prepare an ImageBlock as specified by the film
                block = film.create_block(normalize=True)

                # Only use the coalescing feature when rendering enough samples
                block.set_coalesce(block.coalesce() and spp >= 4)

                # Recompute the first intersection point with derivative tracking
                si = pi.compute_surface_interaction(ray, 
                                                    ray_flags=mi.RayFlags.All | mi.RayFlags.FollowShape,
                                                    active=valid)
                pos = dr.select(valid, sensor.sample_direction(si, [0, 0], active=valid)[0].uv, pos)

                D = sensor_to_surface_reparam_det(sensor, si, ignore_near_plane=True, active=valid)

                # Accumulate into the image block
                # particle-style to match the measurement of the boundary term
                ADIntegrator._splat_to_block(
                    block, film, pos,
                    value=L * weight * dr.rcp(mi.ScalarFloat(spp)) * det_over_det(D),
                    weight=0,
                    alpha=dr.select(valid, mi.Float(1), mi.Float(0)),
                    aovs=aovs,
                    wavelengths=ray.wavelengths
                )

                # film the weight division and return an image tensor
                film.put_block(block)
                image = film.develop()
                dr.set_grad(image, grad_in)

                dr.enqueue(dr.ADMode.Backward, image)
                dr.traverse(dr.ADMode.Backward)
                
                dr.eval()
                
            # We don't need any of the outputs here
            del L_2, valid_2, aovs_2, state_out, state_out_2, \
                δL, δaovs, ray, weight, pos, sampler


            # Run kernel representing side effects of the above
            dr.eval()

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
        if self.include_interior and self.radiative_backprop and sppc > 0:
            result_grad += self.rb_render_forward(scene, None, sensor, seed, sppc)

        # Discontinuous derivative (and the non-RB continuous derivative)
        if (self.include_boundary and (sppp > 0 or sppi > 0)) or \
           (self.include_interior and sppc > 0 and not self.radiative_backprop):

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
        if self.include_interior and self.radiative_backprop and sppc > 0:
            self.rb_render_backward(scene, None, grad_in, sensor, seed, sppc)

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
        if self.include_boundary and sppp > 0 and has_silhouettes:
            with dr.suspend_grad():
                self.proj_detail.init_primarily_visible_silhouette(scene, sensor)

            sampler, spp = self.prepare(sensor, 0xffffffff ^ seed, sppp, aovs)
            result_img += self.render_primarily_visible_silhouette(scene, sensor, sampler, spp)

        # Indirect discontinuous derivative
        if self.include_boundary and sppi > 0 and has_silhouettes:
            with dr.suspend_grad():
                self.proj_detail.init_indirect_silhouette(scene, sensor, 0xafafafaf ^ seed)

            sampler, spp = self.prepare(sensor, 0xaa00aa00 ^ seed, sppi, aovs)
            result_img += self.render_indirect_silhouette(scene, sensor, sampler, spp)

        ## Continuous derivative (only if radiative backpropagation is not used)
        if self.include_interior and sppc > 0 and (not self.radiative_backprop):
            if isinstance(sensor, int):
                sensor = scene.sensors()[sensor]

            film = sensor.film()

            # Disable derivatives in all of the following
            with dr.suspend_grad():
                # Prepare the film and sample generator for rendering
                sampler, spp = self.prepare(sensor, seed, spp, self.aov_names())

                # Generate a set of rays starting at the sensor
                ray, weight, pos = self.sample_rays(scene, sensor, sampler)

                with dr.resume_grad():
                    L, valid, aovs, si = ThreePointIntegrator.sample(self,
                        mode     = mode,
                        scene    = scene,
                        sampler  = sampler,
                        ray      = ray,
                        active   = mi.Bool(True)
                    )
                    
                    block = film.create_block(normalize=True)
                    # Only use the coalescing feature when rendering enough samples
                    block.set_coalesce(block.coalesce() and spp >= 4)

                    # Keep track of derivatives wrt. sample positions ('pos')
                    pos = dr.select(valid, sensor.sample_direction(si, [0, 0], active=valid)[0].uv, pos)

                    # The film-to-sensor determinant is not needed here 
                    # because differentiation w.r.t. sensors is not supported,
                    # so it would cancel anyway in det_over_det(D).
                    D = sensor_to_surface_reparam_det(sensor, si, ignore_near_plane=True)
                    
                    # Accumulate into the image block, 
                    # particle-style to match the measurement of the boundary term
                    ADIntegrator._splat_to_block(
                        block, film, pos,
                        value=L * weight * dr.rcp(mi.ScalarFloat(spp)) * det_over_det(D),
                        weight=0,
                        alpha=dr.select(valid, mi.Float(1), mi.Float(0)),
                        aovs=aovs,
                        wavelengths=ray.wavelengths
                    )

                    film.put_block(block)
                    result_img += film.develop()

        return result_img

    def sample_radiance_difference(self, scene, ss, curr_depth, sampler, active):
        """
        See ``PSIntegrator.sample_radiance_difference()`` for a description of
        this interface and the role of the various parameters and return values.
        """

        # ----------- Estimate the radiance of the background -----------

        ray_bg = mi.Ray3f(
            ss.p + (1 + dr.max(dr.abs(ss.p))) * (ss.d * ss.offset + ss.n * mi.math.ShapeEpsilon),
            ss.d
        )
        radiance_bg, _, _, _ = self.sample(
            dr.ADMode.Primal, scene, sampler, ray_bg, curr_depth, None, None, active, False, None)

        # Since this integrator uses the material-form parameterization, the boundary
        # derivative for the foreground radiance is included in the interior term
        radiance_fg = 0

        # Compute the radiance "difference"
        radiance_diff = radiance_fg - radiance_bg
        active_diff = active & (dr.max(dr.abs(radiance_diff)) > 0)

        return radiance_diff, active_diff

mi.register_integrator("prb_projective_fix", lambda props: PathProjectiveFixIntegrator(props))

del PSIntegrator
