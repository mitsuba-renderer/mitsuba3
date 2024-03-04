from __future__ import annotations # Delayed parsing of type annotations

import drjit as dr
import mitsuba as mi

from .common import PSIntegrator, mis_weight

class PathProjectiveIntegrator(PSIntegrator):
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
        # ``RBIntegrator``
        self.radiative_backprop = True

        # Specify the seed ray generation strategy
        self.project_seed = props.get('project_seed', 'both')
        if self.project_seed not in ['both', 'bsdf', 'emitter']:
            raise Exception(f"Project seed must be one of 'both', 'bsdf', "
                            f"'emitter', got '{self.project_seed}'")

    @dr.syntax
    def sample(self,
               mode: dr.ADMode,
               scene: mi.Scene,
               sampler: mi.Sampler,
               ray: mi.Ray3f,
               depth: mi.UInt32,
               δL: Optional[mi.Spectrum],
               state_in: Any,
               active: mi.Bool,
               project: bool = False,
               si_shade: Optional[mi.SurfaceInteraction3f] = None,
               **kwargs # Absorbs unused arguments
    ) -> Tuple[mi.Spectrum, mi.Bool, List[mi.Float], Any]:
        """
        See ``PSIntegrator.sample()`` for a description of this interface and
        the role of the various parameters and return values.
        """

        # Rendering a primal image? (vs performing forward/reverse-mode AD)
        primal = mode == dr.ADMode.Primal

        # Should we use ``si_shade`` as the first interaction and ignore the ray?
        ignore_ray = si_shade is not None

        # Standard BSDF evaluation context for path tracing
        bsdf_ctx = mi.BSDFContext()

        # --------------------- Configure loop state ----------------------
        ray = mi.Ray3f(dr.detach(ray))                # Current ray
        depth = mi.UInt32(depth)                      # Depth of the current path segment
        L = mi.Spectrum(0 if primal else state_in)    # Radiance accumulator
        δL = mi.Spectrum(δL if δL is not None else 0) # Differential/adjoint radiance
        β = mi.Spectrum(1)                            # Path throughput weight
        η = mi.Float(1)                               # Index of refraction
        active = mi.Bool(active)                      # Active SIMD lanes
        depth_init = mi.UInt32(depth)                 # Initial depth

        # Variables caching information
        prev_si         = dr.zeros(mi.SurfaceInteraction3f)
        prev_bsdf_pdf   = mi.Float(1.0)
        prev_bsdf_delta = mi.Bool(True)

        # Projective seed ray information
        cnt_seed = mi.UInt32(0)            # Number of valid seed rays encountered
        ray_seed = dr.zeros(mi.Ray3f)      # Seed ray to be projected
        active_seed = mi.Bool(active)      # Active SIMD lanes

        while dr.hint(active,
                      max_iterations=self.max_depth,
                      label="PRB Projective (%s)" % mode.name):
            active_next = mi.Bool(active)

            # Compute a surface interaction that tracks derivatives arising
            # from differentiable shape parameters (position, normals, etc).
            # In primal mode, this is just an ordinary ray tracing operation.
            use_si_shade = ignore_ray & (depth == depth_init)
            with dr.resume_grad(when=not primal):
                si = scene.ray_intersect(ray,
                                         ray_flags=mi.RayFlags.All,
                                         coherent=(depth == 0),
                                         active=active_next & ~use_si_shade)
            if dr.hint(ignore_ray, mode='scalar'):
                si[use_si_shade] = si_shade

            # Get the BSDF, potentially computes texture-space differentials
            bsdf = si.bsdf(ray)

            # ---------------------- Direct emission ----------------------

            # Hide the environment emitter if necessary
            if dr.hint(self.hide_emitters, mode='scalar'):
                active_next &= ~((depth == 0) & ~si.is_valid())

            # Compute MIS weight for emitter sample from previous bounce
            ds = mi.DirectionSample3f(scene, si=si, ref=prev_si)

            mis = mis_weight(
                prev_bsdf_pdf,
                scene.pdf_emitter_direction(prev_si, ds, ~prev_bsdf_delta)
            )

            with dr.resume_grad(when=not primal):
                Le = β * mis * ds.emitter.eval(si, active_next)

            # ---------------------- Emitter sampling ----------------------

            # Should we continue tracing to reach one more vertex?
            active_next &= (depth + 1 < self.max_depth) & si.is_valid()

            # Is emitter sampling even possible on the current vertex?
            active_em = active_next & mi.has_flag(bsdf.flags(), mi.BSDFFlags.Smooth)

            # If so, randomly sample an emitter without derivative tracking.
            ds, em_weight = scene.sample_emitter_direction(
                si, sampler.next_2d(), True, active_em)
            active_em &= (ds.pdf != 0.0)

            with dr.resume_grad(when=not primal):
                if dr.hint(not primal, mode='scalar'):
                    # Given the detached emitter sample, *recompute* its
                    # contribution with AD to enable light source optimization
                    ds.d = dr.replace_grad(ds.d, dr.normalize(ds.p - si.p))
                    em_val = scene.eval_emitter_direction(si, ds, active_em)
                    em_weight = dr.replace_grad(em_weight, dr.select((ds.pdf != 0), em_val / ds.pdf, 0))
                    dr.disable_grad(ds.d)

                # Evaluate BSDF * cos(theta) differentiably
                wo = si.to_local(ds.d)
                bsdf_value_em, bsdf_pdf_em = bsdf.eval_pdf(bsdf_ctx, si, wo, active_em)
                mis_em = dr.select(ds.delta, 1, mis_weight(ds.pdf, bsdf_pdf_em))
                Lr_dir = β * mis_em * bsdf_value_em * em_weight

            # ------------------ Detached BSDF sampling -------------------

            bsdf_sample, bsdf_weight = bsdf.sample(bsdf_ctx, si,
                                                   sampler.next_1d(),
                                                   sampler.next_2d(),
                                                   active_next)

            # ---- Update loop variables based on current interaction -----

            L = (L + Le + Lr_dir) if primal else (L - Le - Lr_dir)
            ray = si.spawn_ray(si.to_world(bsdf_sample.wo))
            η *= bsdf_sample.eta
            β *= bsdf_weight

            # Information about the current vertex needed by the next iteration

            prev_si = dr.detach(si, True)
            prev_bsdf_pdf = bsdf_sample.pdf
            prev_bsdf_delta = mi.has_flag(bsdf_sample.sampled_type, mi.BSDFFlags.Delta)

            # ------------------------ Seed rays --------------------------

            # Note: even when the current vertex has a delta BSDF, which implies
            # that any perturbation of the direction will be invalid, we still
            # allow projection of such a ray segment. If this is not desired,
            # mask ``active_seed_cand &= ~prev_bsdf_delta``.

            if dr.hint(project, mode='scalar'):
                    # Given the detached emitter sample, *recompute* its
                # Is it possible to project a seed ray from the current vertex?
                active_seed_cand = mi.Mask(active_next)

                # If so, pick a seed ray (if applicable)
                if dr.hint(self.project_seed == "bsdf", mode='scalar'):
                    # We assume that `ray` intersects the scene
                    ray_seed_cand = ray
                elif dr.hint(self.project_seed == "emitter", mode='scalar'):
                    ray_seed_cand = si.spawn_ray_to(ds.p)
                    ray_seed_cand.maxt = dr.largest(mi.Float)
                    # Directions towards the interior have no contribution
                    # unless we hit a transmissive BSDF
                    active_seed_cand &= (dr.dot(si.n, ray_seed_cand.d) > 0) | \
                                         mi.has_flag(bsdf.flags(), mi.BSDFFlags.Transmission)
                elif dr.hint(self.project_seed == "both", mode='scalar'):
                    # By default we use the BSDF sample as the seed ray
                    ray_seed_cand = ray

                    # Flip a coin only when the emitter sample is valid
                    mask_replace = (dr.dot(si.n, ray_seed_cand.d) > 0) | \
                                    mi.has_flag(bsdf.flags(), mi.BSDFFlags.Transmission)
                    mask_replace &= sampler.next_1d(active_seed_cand) > 0.5
                    ray_seed_cand[mask_replace] = si.spawn_ray_to(ds.p)
                    ray_seed_cand.maxt = dr.largest(mi.Float)

                # Resovoir sampling: do we use this candidate seed ray?
                cnt_seed[active_seed_cand] += 1
                replace = active_seed_cand & \
                          (sampler.next_1d(active_seed_cand) * cnt_seed <= 1)

                # Update the seed ray if necessary
                ray_seed[replace] = ray_seed_cand
                active_seed |= replace

            # -------------------- Stopping criterion ---------------------

            # Don't run another iteration if the throughput has reached zero
            β_max = dr.max(β)
            active_next &= (β_max != 0)

            # Russian roulette stopping probability (must cancel out ior^2
            # to obtain unitless throughput, enforces a minimum probability)
            rr_prob = dr.minimum(β_max * η**2, .95)

            # Apply only further along the path since, this introduces variance
            rr_active = depth >= self.rr_depth
            β[rr_active] *= dr.rcp(rr_prob)
            rr_continue = sampler.next_1d() < rr_prob
            active_next &= ~rr_active | rr_continue

            # ------------------ Differential phase only ------------------

            if dr.hint(not primal, mode='scalar'):
                with dr.resume_grad():
                    # 'L' stores the indirectly reflected radiance at the
                    # current vertex but does not track parameter derivatives.
                    # The following addresses this by canceling the detached
                    # BSDF value and replacing it with an equivalent term that
                    # has derivative tracking enabled. (nit picking: the
                    # direct/indirect terminology isn't 100% accurate here,
                    # since there may be a direct component that is weighted
                    # via multiple importance sampling)

                    # Recompute 'wo' to propagate derivatives to cosine term
                    wo = si.to_local(ray.d)

                    # Re-evaluate BSDF * cos(theta) differentiably
                    bsdf_val = bsdf.eval(bsdf_ctx, si, wo, active_next)

                    # Detached version of the above term and inverse
                    bsdf_val_det = bsdf_weight * bsdf_sample.pdf
                    inv_bsdf_val_det = dr.select(bsdf_val_det != 0,
                                                 dr.rcp(bsdf_val_det), 0)

                    # Differentiable version of the reflected indirect
                    # radiance. Minor optional tweak: indicate that the primal
                    # value of the second term is always 1.
                    tmp = inv_bsdf_val_det * bsdf_val
                    tmp_replaced = dr.replace_grad(dr.ones(mi.Float, dr.width(tmp)), tmp) #FIXME
                    Lr_ind = L * tmp_replaced

                    # Differentiable Monte Carlo estimate of all contributions
                    Lo = Le + Lr_dir + Lr_ind

                    attached_contrib = dr.flag(dr.JitFlag.VCallRecord) and not dr.grad_enabled(Lo)
                    if dr.hint(attached_contrib, mode='scalar'):
                        raise Exception(
                            "The contribution computed by the differential "
                            "rendering phase is not attached to the AD graph! "
                            "Raising an exception since this is usually "
                            "indicative of a bug (for example, you may have "
                            "forgotten to call dr.enable_grad(..) on one of "
                            "the scene parameters, or you may be trying to "
                            "optimize a parameter that does not generate "
                            "derivatives in detached PRB.)")

                    # Propagate derivatives from/to 'Lo' based on 'mode'
                    if dr.hint(mode == dr.ADMode.Backward, mode='scalar'):
                        dr.backward_from(δL * Lo)
                    else:
                        δL += dr.forward_to(Lo)

            depth[si.is_valid()] += 1
            active = active_next

        return (
            L if primal else δL, # Radiance/differential radiance
            depth != 0,          # Ray validity flag for alpha blending
            [],                  # Empty tuple of AOVs.
                                 # Seed rays, or the state for the differential phase
            [dr.detach(ray_seed), active_seed] if project else L
        )


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

        # ----------- Estimate the radiance of the foreground -----------

        # Create a preliminary intersection point
        pi_fg = dr.zeros(mi.PreliminaryIntersection3f)
        pi_fg.t = 1
        pi_fg.prim_index = ss.prim_index
        pi_fg.prim_uv = ss.uv
        pi_fg.shape = ss.shape

        # Create a dummy ray that we never perform ray-intersection with to
        # compute other fields in ``si``
        dummy_ray = mi.Ray3f(ss.p - ss.d, ss.d)

        # The ray origin is wrong, but this is fine if we only need the primal
        # radiance
        si_fg = pi_fg.compute_surface_interaction(
            dummy_ray, mi.RayFlags.All, active)

        # If smooth normals are used, it is possible that the computed
        # shading normal near visibility silhouette points to the wrong side
        # of the surface. We fix this by clamping the shading frame normal
        # to the visible side.
        alpha = dr.dot(si_fg.sh_frame.n, ss.d)
        eps = dr.epsilon(alpha) * 100
        wrong_side = active & (alpha > -eps)
        # Remove the component of the shading frame normal that points to
        # the wrong side
        new_sh_normal = dr.normalize(
            si_fg.sh_frame.n - (alpha + eps) * ss.d)
        # ``si_fg`` surgery
        si_fg.sh_frame[wrong_side] = mi.Frame3f(new_sh_normal)
        si_fg.wi[wrong_side] = si_fg.to_local(-ss.d)

        # Call `sample()` to estimate the radiance starting from the surface
        # interaction
        radiance_fg, _, _, _ = self.sample(
            dr.ADMode.Primal, scene, sampler, ray_bg, curr_depth, None, None, active, False, si_fg)

        # Compute the radiance difference
        radiance_diff = radiance_fg - radiance_bg
        active_diff = active & (dr.max(dr.abs(radiance_diff)) > 0)

        return radiance_diff, active_diff


    @dr.syntax
    def sample_importance(self, scene, sensor, ss, max_depth, sampler, preprocess, active):
        """
        See ``PSIntegrator.sample_importance()`` for a description of this
        interface and the role of the various parameters and return values.
        """

        # Trace a ray to the sensor end of the boundary segment
        ray_boundary = mi.Ray3f(
            ss.p + (1 + dr.max(dr.abs(ss.p))) * (-ss.d * ss.offset + ss.n * mi.math.ShapeEpsilon),
            -ss.d
        )
        if dr.hint(preprocess, mode='scalar'):
            si_boundary = scene.ray_intersect(ray_boundary, active=active)
        else:
            with dr.resume_grad():
                si_boundary = scene.ray_intersect(
                    ray_boundary,
                    ray_flags=mi.RayFlags.All | mi.RayFlags.FollowShape,
                    coherent=False,
                    active=active)
        active = active & si_boundary.is_valid()

        # Loop state variables
        β = mi.Spectrum(1)                          # Path importance weight
        W = mi.Spectrum(0)                          # Selected importance weight
        si_loop = dr.detach(si_boundary)            # Current surface interaction
        si_cam = dr.zeros(mi.SurfaceInteraction3f)  # Camera ray shape interaction
        depth = mi.UInt32(0)                        # Depth of the current path segment
        depth_cam = mi.UInt32(0)                    # Depth of the selected interaction
        cnt_valid = mi.UInt32(0)                    # Number of valid connections
        first_its = mi.Bool(active)                 # First iteration?
        first_wo = dr.zeros(mi.Vector3f)            # First BSDF sampled direction, or the direction to the sensor
        active_loop = mi.Mask(active)               # Active SIMD lanes

        bsdf_ctx = mi.BSDFContext(mi.TransportMode.Importance)

        while dr.hint(active_loop,
                      label="Estimate Importance"):
            # Is it possible to connect the current vertex to the sensor?
            bsdf = si_loop.bsdf()
            active_connect = active_loop & mi.has_flag(bsdf.flags(), mi.BSDFFlags.Smooth)

            # Sample a direction from the current vertex towards the sensor
            sensor_ds, sensor_weight = sensor.sample_direction(
                si_loop, sampler.next_2d(active_connect), active_connect)
            active_connect &= (sensor_ds.pdf > 0) & dr.any(sensor_weight > 0)

            # Check that the sensor is visible from the current vertex (shadow test)
            ray_test = si_loop.spawn_ray_to(sensor_ds.p)
            occluded = scene.ray_test(ray_test, active_connect)
            found = active_connect & ~occluded
            cnt_valid[found] += 1

            # Should we use this connection to replace the current one in the resovoir?
            replace = found & (sampler.next_1d(found) * cnt_valid <= 1)
            si_cam[replace]    = si_loop
            depth_cam[replace] = depth
            W[replace]         = β

            # Should we continue tracing to reach one more vertex? We need to
            # continue tracing even when we have found a valid connection.
            depth += 1
            active_loop &= depth + 1 < max_depth

            # -------------------- BSDF sampling --------------------
            # Sample a direction from the BSDF
            bs, bsdf_weight = bsdf.sample(
                bsdf_ctx, si_loop, sampler.next_1d(active_loop),
                sampler.next_2d(active_loop), active_loop)
            wo_bsdf_world = si_loop.to_world(bs.wo)

            # Store the direction if this is the first iteration
            first_wo[active_loop & first_its] = wo_bsdf_world
            first_its &= False

            # Update the throughput weight
            β[active_loop] *= bsdf_weight

            # Get the next surface interaction
            ray_next = si_loop.spawn_ray(wo_bsdf_world)
            si_loop[active_loop] = scene.ray_intersect(ray_next, active_loop)

            # Update the active lanes
            active_loop &= si_loop.is_valid()

        # Which lanes have a valid connection?
        active_found = active & (cnt_valid > 0)

        # Re-compute the importance weight
        sensor_ds, sensor_weight = sensor.sample_direction(
            si_cam, sampler.next_2d(active_found), active_found)
        W *= sensor_weight / sensor_ds.pdf

        # Include the camera ray intersection BSDF
        bsdf = si_cam.bsdf()
        bsdf_val = bsdf.eval(bsdf_ctx, si_cam, si_cam.to_local(sensor_ds.d), active_found)
        W *= bsdf_val

        # Compensate for the resovoir sampling
        W *= mi.Float(cnt_valid)

        # If we picked the first interaction point, we need to recompute the `first_wo`
        first_wo[depth_cam == 0] = sensor_ds.d

        # Return the number of segments to connect to the sensor. We first need
        # to add 1 since the counting starts from 0, and we need to add another
        # 1 because we have not counted the camera ray itself.
        depth_cam += 2

        # Recompute the correct motion of the first interaction point
        if dr.hint(not preprocess, mode='scalar'):
            d = -first_wo
            O = si_boundary.p - d
            with dr.resume_grad():
                t = dr.dot(si_boundary.p - O, si_boundary.n) / dr.dot(d, si_boundary.n)
                si_boundary.p = dr.replace_grad(si_boundary.p, O + t * d)

        return W, sensor_ds.uv, depth_cam, si_boundary.p, active_found

mi.register_integrator("prb_projective", lambda props: PathProjectiveIntegrator(props))

del PSIntegrator
