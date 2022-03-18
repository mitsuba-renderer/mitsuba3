from __future__ import annotations # Delayed parsing of type annotations

import drjit as dr
import mitsuba as mi

from .common import ADIntegrator, mis_weight

class DirectReparamIntegrator(ADIntegrator):
    """
    This class implements a reparameterized direct illumination integrator.
    
    It is functionally equivalent with 'prb_reparam' when 'max_depth' and
    'reparam_max_depth' are both set to be 2. But since direct illumination
    tasks only have two ray segments, the overhead of relying on radiative
    backpropagation is non-neglegible. This implementation builds on the
    traditional ADIntegrator that does not require two passes during
    gradient traversal.
    """

    def __init__(self, props):
        super().__init__(props)

        self.reparam_max_depth = props.get('reparam_max_depth', 2)

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
    ) -> Tuple[mi.Spectrum, mi.Bool]:
        """
        See ``ADIntegrator.sample()`` for a description of this interface and
        the role of the various parameters and return values.
        """

        primal = mode == dr.ADMode.Primal

        bsdf_ctx = mi.BSDFContext()
        L = mi.Spectrum(0)

        ray_reparam = mi.Ray3f(ray)
        if not primal:
            # Camera ray reparameterization determinant already considered in ADIntegrator.sample_rays()
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

        # Detached sample
        active_em = active_next & mi.has_flag(bsdf.flags(), mi.BSDFFlags.Smooth)
        with dr.suspend_grad():
            ds, em_weight = scene.sample_emitter_direction(
                si, sampler.next_2d(), True, active_em)  # is it correct to attached sample?
        active_em &= dr.neq(ds.pdf, 0.0)
        # Re-compute attached `em_weight` to enable emitter optimization
        if not primal:
            ds.d = dr.normalize(ds.p - si.p)
            em_val = scene.eval_emitter_direction(si, ds, active_em)
            em_weight = dr.select(dr.neq(ds.pdf, 0), em_val / ds.pdf, 0)
            dr.disable_grad(ds.d)

        # Reparametrize the ray
        em_ray_det = 1
        if not primal:
            # Create a surface interaction that follows the shape's
            # motion while ignoring any reparameterization from the
            # previous ray. In other words, 'si_follow.o' along can
            # have gradients attached for subsequent reparametrization.
            si_follow = pi.compute_surface_interaction(
                ray_reparam, mi.RayFlags.All | mi.RayFlags.FollowShape)
            # Reparameterize the ray towards the emitter
            em_ray = si_follow.spawn_ray_to(ds.p)
            em_ray.d, em_ray_det = reparam(em_ray, depth=1, active=active_em)
            ds.d = em_ray.d

        # Compute MIS
        wo = si.to_local(ds.d)
        bsdf_value_em, bsdf_pdf_em = bsdf.eval_pdf(bsdf_ctx, si, wo, active_em)
        mis_em = dr.select(ds.delta, 1, mis_weight(ds.pdf, bsdf_pdf_em))

        L += mis_em * bsdf_value_em * em_weight * em_ray_det

        # ------------------ BSDF sampling -------------------

        # Detached sample
        with dr.suspend_grad():
            bsdf_sample, bsdf_weight = bsdf.sample(bsdf_ctx, si, sampler.next_1d(active_next),
                                                sampler.next_2d(active_next), active_next)
            ray_next = si.spawn_ray(si.to_world(bsdf_sample.wo))
        active_bsdf = active_next & dr.neq(bsdf_sample, 0.0)

        # Reparametrize the ray
        ray_reparam_next = mi.Ray3f(ray_next)
        ray_reparam_det_next = 1
        if not primal:
            ray_reparam_next.d, ray_reparam_det_next = reparam(ray_next, depth=1, active=active_bsdf)

        # Illumination
        si_next = scene.ray_intersect(ray_reparam_next, active_bsdf)
        L_bsdf = si_next.emitter(scene).eval(si_next, active_bsdf)

        # Compute MIS
        with dr.suspend_grad():
            ds = mi.DirectionSample3f(scene, si_next, si)
            delta = mi.has_flag(bsdf_sample.sampled_type, mi.BSDFFlags.Delta)
            emitter_pdf = scene.pdf_emitter_direction(si, ds, active_bsdf & ~delta)
            mis_bsdf = mis_weight(bsdf_sample.pdf, emitter_pdf)

        L += L_bsdf * bsdf_weight * mis_bsdf * ray_reparam_det_next

        return (L, active)

mi.register_integrator("direct_reparam", lambda props: DirectReparamIntegrator(props))
