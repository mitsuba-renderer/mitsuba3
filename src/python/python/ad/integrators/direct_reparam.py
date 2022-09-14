from __future__ import annotations # Delayed parsing of type annotations

import drjit as dr
import mitsuba as mi

from .common import ADIntegrator, mis_weight

class DirectReparamIntegrator(ADIntegrator):
    r"""
    .. _integrator-direct_reparam:

    Reparameterized Direct Integrator (:monosp:`direct_reparam`)
    -------------------------------------------------------------

    .. pluginparameters::

     * - reparam_max_depth
       - |int|
       - Specifies the longest path depth for which the reparameterization
         should be enabled (maximum 2 for this integrator). A value of 1
         will only produce visibility gradients for directly visible shapes
         while a value of 2 will also account for shadows. (Default: 2)

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
         sampling the auxiliary rays. (Default: False)

    This plugin implements a reparameterized direct illumination integrator.

    It is functionally equivalent with `prb_reparam` when `max_depth` and
    `reparam_max_depth` are both set to be 2. But since direct illumination
    tasks only have two ray segments, the overhead of relying on radiative
    backpropagation is non-negligible. This implementation builds on the
    traditional ADIntegrator that does not require two passes during
    gradient traversal.

    .. tabs::

        .. code-tab:: python

            'type': 'direct_reparam',
            'reparam_rays': 8
    """

    def __init__(self, props):
        super().__init__(props)

        # Specifies the max depth up to which reparameterization is applied
        self.reparam_max_depth = props.get('reparam_max_depth', 2)
        assert(self.reparam_max_depth <= 2)

        # Specifies the number of auxiliary rays used to evaluate the
        # reparameterization
        self.reparam_rays = props.get('reparam_rays', 16)

        # Specifies the von Mises Fisher distribution parameter for sampling
        # auxiliary rays in Bangaru et al.'s [2000] parameterization
        self.reparam_kappa = props.get('reparam_kappa', 1e5)

        # Harmonic weight exponent in Bangaru et al.'s [2000] parameterization
        self.reparam_exp = props.get('reparam_exp', 3.0)

        # Enable antithetic sampling in the reparameterization?
        self.reparam_antithetic = props.get('reparam_antithetic', False)

        self.params = None

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
                                        unroll=False,
                                        active=active)

    def sample(self,
               mode: dr.ADMode,
               scene: mi.Scene,
               sampler: mi.Sampler,
               ray: mi.Ray3f,
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

        primal = mode == dr.ADMode.Primal

        bsdf_ctx = mi.BSDFContext()
        L = mi.Spectrum(0)

        ray_reparam = mi.Ray3f(dr.detach(ray))
        if not primal:
            # Camera ray reparameterization determinant multiplied in ADIntegrator.sample_rays()
            ray_reparam.d, _ = reparam(ray, depth=0, active=active)

        # ---------------------- Direct emission ----------------------

        pi = scene.ray_intersect_preliminary(ray_reparam, active)
        si = pi.compute_surface_interaction(ray_reparam)

        # Differentiable evaluation of intersected emitter / envmap
        L += si.emitter(scene).eval(si)

        # ------------------ Emitter sampling -------------------

        # Should we continue tracing to reach one more vertex?
        active_next = si.is_valid()
        # Get the BSDF. Potentially computes texture-space differentials.
        bsdf = si.bsdf(ray_reparam)

        # Detached emitter sample
        active_em = active_next & mi.has_flag(bsdf.flags(), mi.BSDFFlags.Smooth)
        with dr.suspend_grad():
            ds, weight_em = scene.sample_emitter_direction(
                si, sampler.next_2d(), True, active_em)
        active_em &= dr.neq(ds.pdf, 0.0)
        # Re-compute attached `weight_em` to enable emitter optimization
        if not primal:
            ds.d = dr.normalize(ds.p - si.p)
            val_em = scene.eval_emitter_direction(si, ds, active_em)
            weight_em = dr.select(dr.neq(ds.pdf, 0), val_em / ds.pdf, 0)
            dr.disable_grad(ds.d)

        # Reparameterize the ray
        ray_em_det = 1.0
        if not primal:
            # Create a surface interaction that follows the shape's
            # motion while ignoring any reparameterization from the
            # previous ray. In other words, 'si_follow.o' along can
            # have gradients attached for subsequent reparametrization.
            si_follow = pi.compute_surface_interaction(
                ray_reparam, mi.RayFlags.All | mi.RayFlags.FollowShape)
            # Reparameterize the ray towards the emitter
            ds.d, ray_em_det = reparam(si_follow.spawn_ray_to(ds.p), depth=1, active=active_em)

        # Compute MIS
        wo = si.to_local(ds.d)
        bsdf_value_em, bsdf_pdf_em = bsdf.eval_pdf(bsdf_ctx, si, wo, active_em)
        mis_em = dr.select(ds.delta, 1.0, mis_weight(ds.pdf, bsdf_pdf_em))

        L += bsdf_value_em * weight_em * ray_em_det * mis_em

        # ------------------ BSDF sampling -------------------

        # Detached BSDF sample
        with dr.suspend_grad():
            bsdf_sample, bsdf_weight = bsdf.sample(bsdf_ctx, si, sampler.next_1d(active_next),
                                                   sampler.next_2d(active_next), active_next)
            ray_bsdf = si.spawn_ray(si.to_world(bsdf_sample.wo))
        active_bsdf = active_next & dr.any(dr.neq(bsdf_weight, 0.0))
        # Re-compute attached `bsdf_weight`
        if not primal:
            wo = si.to_local(ray_bsdf.d)
            bsdf_val, bsdf_pdf = bsdf.eval_pdf(bsdf_ctx, si, wo, active_bsdf)
            bsdf_weight = dr.select(dr.neq(bsdf_pdf, 0.0), bsdf_val / dr.detach(bsdf_pdf), 0.0)

        # Reparameterize the ray
        ray_bsdf_det = 1.0
        if not primal:
            ray_bsdf.d, ray_bsdf_det = reparam(
                si_follow.spawn_ray(ray_bsdf.d), depth=1, active=active_bsdf)

        # Illumination
        si_bsdf = scene.ray_intersect(ray_bsdf, active_bsdf)
        L_bsdf = si_bsdf.emitter(scene).eval(si_bsdf, active_bsdf)

        # Compute MIS
        with dr.suspend_grad():
            ds = mi.DirectionSample3f(scene, si_bsdf, si)
            delta = mi.has_flag(bsdf_sample.sampled_type, mi.BSDFFlags.Delta)
            emitter_pdf = scene.pdf_emitter_direction(si, ds, active_bsdf & ~delta)
            mis_bsdf = mis_weight(bsdf_sample.pdf, emitter_pdf)

        L += L_bsdf * bsdf_weight * ray_bsdf_det * mis_bsdf

        return L, active, None

mi.register_integrator("direct_reparam", lambda props: DirectReparamIntegrator(props))
