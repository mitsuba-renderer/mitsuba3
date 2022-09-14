from __future__ import annotations # Delayed parsing of type annotations

import drjit as dr
import mitsuba as mi

from .common import RBIntegrator, mis_weight

class PRBReparamIntegrator(RBIntegrator):
    r"""
    .. _integrator-prb_reparam:

    Reparameterized Path Replay Backpropagation Integrator (:monosp:`prb_reparam`)
    ------------------------------------------------------------------------------

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

     * - reparam_max_depth
       - |int|
       - Specifies the longest path depth for which the reparameterization
         should be enabled. For instance, a value of 1 will only produce
         visibility gradients for directly visible shapes. (Default: 2)

     * - reparam_rays
       - |int|
       - Specifies the number of auxiliary rays to be traced when performing the
         reparameterization. (Default: 16)

     * - reparam_kappa
       - |float|
       - Specifies the kappa parameter of the von Mises Fisher distribution used
         to sample auxiliary rays.. (Default: 1e5)

     * - reparam_exp
       - |float|
       - Power exponent applied on the computed harmonic weights in the
         reparameterization. (Default: 3.0)

     * - reparam_antithetic
       - |bool|
       - Should antithetic sampling be enabled to improve convergence when
         sampling the auxiliary rays? (Default: False)

     * - reparam_unroll
       - |bool|
       - Unroll the loop tracing auxiliary rays in the reparameterization?
         (Default: False)

    This class implements a reparameterized Path Replay Backpropagation (PRB)
    integrator with the following properties:

    - Emitter sampling (a.k.a. next event estimation).

    - Russian Roulette stopping criterion.

    - The integrator reparameterizes the incident hemisphere to handle
      visibility-induced discontinuities. This makes it possible to optimize
      geometric parameters like vertex positions. Discontinuities observed
      through ideal specular reflection/refraction are not supported and produce
      biased gradients (see also the next point).

    - Detached sampling. This means that the properties of ideal specular
      objects (e.g., the IOR of a glass vase) cannot be optimized.

    See ``prb.py`` and ``prb_basic.py`` for simplified implementations that
    remove some of these features.

    See the papers :cite:`Vicini2021` and :cite:`Zeltner2021MonteCarlo` for
    details on PRB, attached/detached sampling. Reparameterizations for
    differentiable rendering were proposed in
    :cite:`Loubet2019Reparameterizing`. The specific change of variables used in
    Mitsuba is described in :cite:`Bangaru2020`.

    A few more technical details regarding the implementation: this integrator
    uses detached sampling, hence the sampling process that generates the path
    vertices (e.g., ``v₀``, ``v₁``, ..) and the computation of Monte Carlo
    sampling densities is excluded from the derivative computation. However, the
    path throughput that is then evaluated on top of these vertices does track
    derivatives, and it also uses reparameterizations.

    Consider the contribution ``L`` of a path ``(v₀, v₁, v₂, v₃, v₄)`` where
    ``v₀`` is the sensor position, the ``f`` capture BSDFs and cosine factors,
    and the ``E`` represent emission.

    .. code-block:: none

        L(v₀, v₁, v₂, v₃) = E₁(v₀, v₁) + f₁(v₀, v₁, v₂) *
                                (E₂(v₁, v₂) + f₂(v₁, v₂, v₃) *
                                    (E₃(v₂, v₃) + f₃(v₂, v₃, v₄) * E₄(v₃, v₄)))


    The derivative of this function with respect to a scene parameter ``π``
    expands into a long sequence of terms via the product and chain rules.

    .. code-block:: none

        ∂L =                       ______ Derivative of emission terms ______
                                  (∂E₁/∂π + ∂E₁/∂v₀ ∂v₀/∂π + ∂E₁/∂v₁ ∂v₁/∂π)
        + (f₁   )                 (∂E₂/∂π + ∂E₂/∂v₁ ∂v₁/∂π + ∂E₂/∂v₂ ∂v₂/∂π)
        + (f₁ f₂)                 (∂E₃/∂π + ∂E₃/∂v₂ ∂v₂/∂π + ∂E₃/∂v₃ ∂v₃/∂π)
        + (f₁ f₂ f₃)              (∂E₄/∂π + ∂E₄/∂v₃ ∂v₃/∂π + ∂E₄/∂v₄ ∂v₄/∂π)

                                ______________ Derivative of reflection terms _____________
        + (E₂ + f₂ E₃ + f₂ f₃ E₄) (∂f₁/∂π + ∂f₁/∂v₀ ∂v₀/∂π + ∂f₁/∂v₁ ∂v₁/∂π + ∂f₁/∂v₂ ∂v₂/∂π)
        + (f₁ E₃ + f₁ f₃ E₄     ) (∂f₂/∂π + ∂f₂/∂v₁ ∂v₁/∂π + ∂f₂/∂v₂ ∂v₂/∂π + ∂f₂/∂v₃ ∂v₃/∂π)
        + (f₁ f₂ E₄             ) (∂f₃/∂π + ∂f₃/∂v₂ ∂v₂/∂π + ∂f₃/∂v₃ ∂v₃/∂π + ∂f₃/∂v₄ ∂v₄/∂π)

    This expression sums over essentially the same terms, but it must account
    for how each one could change as a consequence of internal dependencies
    (e.g., ``∂f₁/∂π``) or due to the reparameterization (e.g., ``∂f₁/∂v₁
    ∂v₁/∂π``). It's tedious to do this derivative calculation manually,
    especially once additional complications like direct illumination sampling
    and MIS are taken into account. We prefer using automatic differentiation
    for this, which will evaluate the chain/product rule automatically.

    However, a nontrivial technical challenge here is that path tracing-style
    integrators perform a loop over path vertices, while DrJit's loop recording
    facilities do not permit the use of AD across loop iterations. The loop must
    thus be designed so that use of AD is self-contained in each iteration of
    the loop, while generating all the terms of ``∂T`` iteratively without
    omission or duplication.

    We have chosen to implement this loop so that iteration ``i`` computes all
    derivative terms associated with the current vertex, meaning: ``Eᵢ/∂π``,
    ``fᵢ/∂π``, as well as the parameterization-induced terms involving
    ``∂vᵢ/∂π``. To give a more concrete example, filtering the previous example
    derivative ``∂L`` to only include terms for the interior vertex ``v₂``
    leaves:

    .. code-block:: none

        ∂L₂ =                      __ Derivative of emission terms __
            + (f₁   )                 (∂E₂/∂π + ∂E₂/∂v₂ ∂v₂/∂π)
            + (f₁ f₂)                 (∂E₃/∂v₂ ∂v₂/∂π)

                                   __ Derivative of reflection terms __
            + (E₂ + f₂ E₃ + f₂ f₃ E₄) (∂f₁/∂v₂ ∂v₂/∂π)
            + (f₁ E₃ + f₁ f₃ E₄     ) (∂f₂/∂π + ∂f₂/∂v₂ ∂v₂/∂π)
            + (f₁ f₂ E₄             ) (∂f₃/∂v₂ ∂v₂/∂π)

    Let's go through these one by one, starting with the easier ones:

    .. code-block:: none

        ∂L₂ = (f₁              ) (∂E₂/∂π + ∂E₂/∂v₂ ∂v₂/∂π)
            + (f₁ E₃ + f₁ f₃ E₄) (∂f₂/∂π + ∂f₂/∂v₂ ∂v₂/∂π)
            + ...

    These are the derivatives of local emission and reflection terms. They both
    account for changes in the parameterization (``∂v₂/∂π``) and a potential
    dependence of the reflection/emission model on the scene parameter being
    differentiated (``∂E₂/∂π``, ``∂f₂/∂π``).

    In the first line, the f₁ term corresponds to the path throughput (labeled
    ``β`` in this class) in the general case, and the (``f₁ E₃ + f₁ f₃ E₄``)
    term is the incident illumination (``Lᵢ``) at the current vertex.

    Evaluating these terms using AD will look something like this:

    .. code-block:: none

        with dr.resume_grad():
            v₂' = reparameterize(v₂)
            L₂ = β * E₂(v₁, v₂') + Lᵢ * f₂(v₁, v₂', v₃) + ...

    with a later call to ``dr.backward(L₂)``. In practice, ``E`` and ``f`` are
    directional functions, which means that these directions need to be
    recomputed from positions using AD.

    However, that leaves a few more terms in ``∂L₂`` that unfortunately add some
    complications.

    .. code-block:: none

        ∂L₂ = ...  + (f₁ f₂)                 (∂E₃/∂v₂ ∂v₂/∂π)
                   + (E₂ + f₂ E₃ + f₂ f₃ E₄) (∂f₁/∂v₂ ∂v₂/∂π)
                   + (f₁ f₂ E₄             ) (∂f₃/∂v₂ ∂v₂/∂π)

    These are changes in emission or reflection terms at neighboring vertices
    (``v₁`` and ``v₃``) that arise due to the reparameterization at ``v₂``. It's
    important that the influence of the scene parameters on the emission or
    reflection terms at these vertices is excluded: we are only interested in
    directional derivatives that arise due to the reparametrization, which can
    be accomplished using a more targeted version of ``dr.resume_grad()``.

    .. code-block:: none

        with dr.resume_grad(v₂'):
            L₂ += β * f₂ * E₃(v₂', v₃) # Emission at next vertex
            L₂ += Lᵢ_prev * f₁(v₀, v₁, v₂') # BSDF at previous vertex
            L₂ += Lᵢ_next * f₃(v₂', v₃, v₄) # BSDF at next vertex

    To get the quantities for the next vertex, the path tracer must "run ahead"
    by one bounce.

    The loop begins each iteration being already provided with the previous and
    current intersection in the form of a ``PreliminaryIntersection3f``. It must
    still be turned into a full ``SurfaceInteraction3f`` which, however, only
    involves a virtual function call and no ray tracing. It computes the next
    intersection and passes that along to the subsequent iteration. Each
    iteration reconstructs three surface interaction records (``prev``, ``cur``,
    ``next``), of which only ``cur`` tracks positional derivatives.

    .. tabs::

        .. code-tab:: python

            'type': 'prb_reparam',
            'max_depth': 8,
            'reparam_rays': 8
    """

    def __init__(self, props):
        super().__init__(props)

        # The reparameterization is computed stochastically and removes
        # gradient bias at the cost of additional variance. Use this parameter
        # to disable the reparameterization after a certain path depth to
        # control this tradeoff. A value of zero disables it completely.
        self.reparam_max_depth = props.get('reparam_max_depth', self.max_depth)

        # Specifies the number of auxiliary rays used to evaluate the
        # reparameterization
        self.reparam_rays = props.get('reparam_rays', 16)

        # Specifies the von Mises Fisher distribution parameter for sampling
        # auxiliary rays in Bangaru et al.'s [2020] parameterization
        self.reparam_kappa = props.get('reparam_kappa', 1e5)

        # Harmonic weight exponent in Bangaru et al.'s [2020] parameterization
        self.reparam_exp = props.get('reparam_exp', 3.0)

        # Enable antithetic sampling in the reparameterization?
        self.reparam_antithetic = props.get('reparam_antithetic', False)

        # Unroll the loop tracing auxiliary rays in the reparameterization?
        self.reparam_unroll = props.get('reparam_unroll', False)

    def reparam(self,
                scene: mi.Scene,
                rng: mi.PCG32,
                params: Any,
                ray: mi.Ray3f,
                depth: mi.UInt32,
                active: mi.Bool):
        """
        Helper function to reparameterize rays internally and within ADIntegrator
        """

        # Potentially disable the reparameterization completely
        if self.reparam_max_depth == 0:
            return dr.detach(ray.d, True), mi.Float(1)

        active = active & (depth < self.reparam_max_depth)

        return mi.ad.reparameterize_ray(scene, rng, params, ray,
                                        num_rays=self.reparam_rays,
                                        kappa=self.reparam_kappa,
                                        exponent=self.reparam_exp,
                                        antithetic=self.reparam_antithetic,
                                        unroll=self.reparam_unroll,
                                        active=active)

    def sample(self,
               mode: dr.ADMode,
               scene: mi.Scene,
               sampler: mi.Sampler,
               ray: mi.Ray3f,
               δL: Optional[mi.Spectrum],
               state_in: Optional[mi.Spectrum],
               reparam: Optional[
                   Callable[[mi.Ray3f, mi.Bool],
                            Tuple[mi.Ray3f, mi.Float]]],
               active: mi.Bool,
               **kwargs # Absorbs unused arguments
    ) -> Tuple[mi.Spectrum, mi.Bool, mi.Spectrum]:
        """
        See ``ADIntegrator.sample()`` for a description of this interface and
        the role of the various parameters and return values.
        """

        # Rendering a primal image? (vs performing forward/reverse-mode AD)
        primal = mode == dr.ADMode.Primal

        # Standard BSDF evaluation context for path tracing
        bsdf_ctx = mi.BSDFContext()

        # --------------------- Configure loop state ----------------------

        # Copy input arguments to avoid mutating the caller's state
        depth = mi.UInt32(0)                          # Depth of current vertex
        L = mi.Spectrum(0 if primal else state_in)    # Radiance accumulator
        δL = mi.Spectrum(δL if δL is not None else 0) # Differential/adjoint radiance
        β = mi.Spectrum(1)                            # Path throughput weight
        η = mi.Float(1)                               # Index of refraction
        mis_em = mi.Float(1)                          # Emitter MIS weight
        active = mi.Bool(active)                      # Active SIMD lanes

        # Initialize loop state variables caching the rays and preliminary
        # intersections of the previous (zero-initialized) and current vertex
        ray_prev = dr.zeros(mi.Ray3f)
        ray_cur  = mi.Ray3f(dr.detach(ray))
        pi_prev  = dr.zeros(mi.PreliminaryIntersection3f)
        pi_cur   = scene.ray_intersect_preliminary(ray_cur, coherent=True,
                                                   active=active)

        # Record the following loop in its entirety
        loop = mi.Loop(name="Path Replay Backpropagation (%s)" % mode.name,
                       state=lambda: (sampler, depth, L, δL, β, η, mis_em, active,
                                      ray_prev, ray_cur, pi_prev, pi_cur, reparam))

        # Specify the max. number of loop iterations (this can help avoid
        # costly synchronization when wavefront-style loops are generated)
        loop.set_max_iterations(self.max_depth)

        while loop(active):
            # The first path vertex requires some special handling (see below)
            first_vertex = dr.eq(depth, 0)

            # Reparameterized ray (a copy of 'ray_cur' in primal mode)
            ray_reparam = mi.Ray3f(ray_cur)

            # Jacobian determinant of the parameterization (1 in primal mode)
            ray_reparam_det = 1

            # ----------- Reparameterize (differential phase only) -----------

            if not primal:
                with dr.resume_grad():
                    # Compute a surface interaction of the previous vertex with
                    # derivative tracking (no-op if there is no prev. vertex)
                    si_prev = pi_prev.compute_surface_interaction(
                        ray_prev, mi.RayFlags.All | mi.RayFlags.FollowShape)

                    # Adjust the ray origin of 'ray_cur' so that it follows the
                    # previous shape, then pass this information to 'reparam'
                    ray_reparam.d, ray_reparam_det = reparam(
                        dr.select(first_vertex, ray_cur,
                                  si_prev.spawn_ray(ray_cur.d)), depth)
                    ray_reparam_det[first_vertex] = 1

                    # Finally, disable all derivatives in 'si_prev', as we are
                    # only interested in tracking derivatives related to the
                    # current interaction in the remainder of this function
                    dr.disable_grad(si_prev)

            # ------ Compute detailed record of the current interaction ------

            # Compute a surface interaction that potentially tracks derivatives
            # due to differentiable shape parameters (position, normals, etc.)

            with dr.resume_grad(when=not primal):
                si_cur = pi_cur.compute_surface_interaction(ray_reparam)

            # ---------------------- Direct emission ----------------------

            # Evaluate the emitter (with derivative tracking if requested)
            with dr.resume_grad(when=not primal):
                emitter = si_cur.emitter(scene)
                Le = β * mis_em * emitter.eval(si_cur)

            # ----------------------- Emitter sampling -----------------------

            # Should we continue tracing to reach one more vertex?
            active_next = (depth + 1 < self.max_depth) & si_cur.is_valid()

            # Get the BSDF, potentially computes texture-space differentials.
            bsdf_cur = si_cur.bsdf(ray_cur)

            # Is emitter sampling even possible on the current vertex?
            active_em = active_next & mi.has_flag(bsdf_cur.flags(), mi.BSDFFlags.Smooth)

            # If so, randomly sample an emitter without derivative tracking.
            ds, em_weight = scene.sample_emitter_direction(
                si_cur, sampler.next_2d(), True, active_em)
            active_em &= dr.neq(ds.pdf, 0.0)

            with dr.resume_grad(when=not primal):
                em_ray_det = 1

                if not primal:
                    # Create a surface interaction that follows the shape's
                    # motion while ignoring any reparameterization from the
                    # previous ray. This ensures the subsequent reparameterization
                    # accounts for occluder's motion relatively to the current shape.
                    si_cur_follow = pi_cur.compute_surface_interaction(
                        ray_cur, mi.RayFlags.All | mi.RayFlags.FollowShape)

                    # Reparameterize the ray towards the emitter
                    em_ray = si_cur_follow.spawn_ray_to(ds.p)
                    em_ray.d, em_ray_det = reparam(em_ray, depth + 1, active_em)
                    ds.d = em_ray.d

                    # Given the detached emitter sample, *recompute* its
                    # contribution with AD to enable light source optimization
                    em_val = scene.eval_emitter_direction(si_cur, ds, active_em)
                    em_weight = dr.select(dr.neq(ds.pdf, 0), em_val / ds.pdf, 0)
                    dr.disable_grad(ds.d)

                # Evaluate BSDF * cos(theta) differentiably
                wo = si_cur.to_local(ds.d)
                bsdf_value_em, bsdf_pdf_em = bsdf_cur.eval_pdf(bsdf_ctx, si_cur,
                                                               wo, active_em)
                mis_direct = dr.select(ds.delta, 1, mis_weight(ds.pdf, bsdf_pdf_em))
                Lr_dir = β * mis_direct * bsdf_value_em * em_weight * em_ray_det

            # ------------------ Detached BSDF sampling -------------------

            # Perform detached BSDF sampling.
            bsdf_sample, bsdf_weight = bsdf_cur.sample(bsdf_ctx, si_cur,
                                                       sampler.next_1d(),
                                                       sampler.next_2d(),
                                                       active_next)

            bsdf_sample_delta = mi.has_flag(bsdf_sample.sampled_type,
                                            mi.BSDFFlags.Delta)

            # ---- Update loop variables based on current interaction -----

            η     *= bsdf_sample.eta
            β     *= bsdf_weight
            L_prev = L  # Value of 'L' at previous vertex
            L      = (L + Le + Lr_dir) if primal else (L - Le - Lr_dir)

            # -------------------- Stopping criterion ---------------------

            # Don't run another iteration if the throughput has reached zero
            β_max = dr.max(β)
            active_next &= dr.neq(β_max, 0)

            # Russian roulette stopping probability (must cancel out ior^2
            # to obtain unitless throughput, enforces a minimum probability)
            rr_prob = dr.minimum(β_max * η**2, .95)

            # Apply only further along the path since, this introduces variance
            rr_active = depth >= self.rr_depth
            β[rr_active] *= dr.rcp(rr_prob)
            rr_continue = sampler.next_1d() < rr_prob
            active_next &= ~rr_active | rr_continue

            # ------------------ Intersect next surface -------------------

            ray_next = si_cur.spawn_ray(si_cur.to_world(bsdf_sample.wo))
            pi_next = scene.ray_intersect_preliminary(ray_next,
                                                      active=active_next)

            # Compute a detached intersection record for the next vertex
            si_next = pi_next.compute_surface_interaction(ray_next)

            # ---------- Compute MIS weight for the next vertex -----------

            ds = mi.DirectionSample3f(scene, si=si_next, ref=si_cur)

            # Probability of sampling 'si_next' using emitter sampling
            # (set to zero if the BSDF doesn't have any smooth components)
            pdf_em = scene.pdf_emitter_direction(
                ref=si_cur, ds=ds, active=~bsdf_sample_delta
            )

            mis_em = mis_weight(bsdf_sample.pdf, pdf_em)

            # ------------------ Differential phase only ------------------

            if not primal:
                # Clone the sampler to run ahead in the random number sequence
                # without affecting the PRB random walk
                sampler_clone = sampler.clone()

                # 'active_next' value at the next vertex
                active_next_next = active_next & si_next.is_valid() & \
                    (depth + 2 < self.max_depth)

                # Retrieve the BSDFs of the two adjacent vertices
                bsdf_next = si_next.bsdf(ray_next)
                bsdf_prev = si_prev.bsdf(ray_prev)

                # Check if emitter sampling is possible at the next vertex
                active_em_next = active_next_next & mi.has_flag(bsdf_next.flags(),
                                                                mi.BSDFFlags.Smooth)

                # If so, randomly sample an emitter without derivative tracking.
                ds_next, em_weight_next = scene.sample_emitter_direction(
                    si_next, sampler_clone.next_2d(), True, active_em_next)
                active_em_next &= dr.neq(ds_next.pdf, 0.0)

                # Compute the emission sampling contribution at the next vertex
                bsdf_value_em_next, bsdf_pdf_em_next = bsdf_next.eval_pdf(
                    bsdf_ctx, si_next, si_next.to_local(ds_next.d), active_em_next)

                mis_direct_next = dr.select(ds_next.delta, 1,
                                            mis_weight(ds_next.pdf, bsdf_pdf_em_next))
                Lr_dir_next = β * mis_direct_next * bsdf_value_em_next * em_weight_next

                # Generate a detached BSDF sample at the next vertex
                bsdf_sample_next, bsdf_weight_next = bsdf_next.sample(
                    bsdf_ctx, si_next, sampler_clone.next_1d(),
                    sampler_clone.next_2d(), active_next_next
                )

                # Account for adjacent vertices, but only consider derivatives
                # that arise from the reparameterization at 'si_cur.p'
                with dr.resume_grad(ray_reparam):
                    # Compute a surface interaction that only tracks derivatives
                    # that arise from the reparameterization.
                    si_cur_reparam_only = pi_cur.compute_surface_interaction(
                        ray_reparam, mi.RayFlags.All | mi.RayFlags.DetachShape)

                    # Differentiably recompute the outgoing direction at 'prev'
                    # and the incident direction at 'next'
                    wo_prev = dr.normalize(si_cur_reparam_only.p - si_prev.p)
                    wi_next = dr.normalize(si_cur_reparam_only.p - si_next.p)

                    # Compute the emission at the next vertex
                    si_next.wi = si_next.to_local(wi_next)
                    Le_next = β * mis_em * \
                        si_next.emitter(scene).eval(si_next, active_next)

                    # Value of 'L' at the next vertex
                    L_next = L - dr.detach(Le_next) - dr.detach(Lr_dir_next)

                    # Account for the BSDF of the previous and next vertices
                    bsdf_val_prev = bsdf_prev.eval(bsdf_ctx, si_prev,
                                                   si_prev.to_local(wo_prev))
                    bsdf_val_next = bsdf_next.eval(bsdf_ctx, si_next,
                                                   bsdf_sample_next.wo)

                    extra = mi.Spectrum(Le_next)
                    extra[~first_vertex]      += L_prev * bsdf_val_prev / dr.maximum(1e-8, dr.detach(bsdf_val_prev))
                    extra[si_next.is_valid()] += L_next * bsdf_val_next / dr.maximum(1e-8, dr.detach(bsdf_val_next))

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
                    wo = si_cur.to_local(ray_next.d)

                    # Re-evaluate BSDF * cos(theta) differentiably
                    bsdf_val = bsdf_cur.eval(bsdf_ctx, si_cur, wo, active_next)

                    # Detached version of the above term and inverse
                    bsdf_val_detach = bsdf_weight * bsdf_sample.pdf
                    inv_bsdf_val_detach = dr.select(dr.neq(bsdf_val_detach, 0),
                                                    dr.rcp(bsdf_val_detach), 0)

                    # Differentiable version of the reflected indirect
                    # radiance. Minor optional tweak: indicate that the primal
                    # value of the second term is always 1.
                    Lr_ind = L * dr.replace_grad(1, inv_bsdf_val_detach * bsdf_val)

                with dr.resume_grad():
                    # Differentiable Monte Carlo estimate of all contributions
                    Lo = (Le + Lr_dir + Lr_ind) * ray_reparam_det + extra

                    if dr.flag(dr.JitFlag.VCallRecord) and not dr.grad_enabled(Lo):
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
                    if mode == dr.ADMode.Backward:
                        dr.backward_from(δL * Lo)
                    else:
                        δL += dr.forward_to(Lo)

            # Differential phases need access to the previous interaction, too
            if not primal:
                pi_prev  = pi_cur
                ray_prev = ray_cur

            # Provide ray/interaction to the next iteration
            pi_cur   = pi_next
            ray_cur  = ray_next

            depth[si_cur.is_valid()] += 1
            active = active_next

        return (
            L if primal else δL, # Radiance/differential radiance
            dr.neq(depth, 0),    # Ray validity flag for alpha blending
            L                    # State for the differential phase
        )

mi.register_integrator("prb_reparam", lambda props: PRBReparamIntegrator(props))
