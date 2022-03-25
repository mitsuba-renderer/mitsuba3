from __future__ import annotations # Delayed parsing of type annotations

import drjit as dr
import mitsuba as mi

from .common import ADIntegrator

class EmissionReparamIntegrator(ADIntegrator):
    """
    This class implements a reparameterized emission integrator.

    It reparametrizes the camera ray to handle discontinuity issues
    caused by moving emitters. This is mainly used for learning and
    debugging purpose.
    """

    def __init__(self, props):
        super().__init__(props)

        # Specifies the max depth that reparamtrization is applied
        self.reparam_max_depth = props.get('reparam_max_depth', 1)
        assert(self.reparam_max_depth <= 1)

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

        ray_reparam = mi.Ray3f(ray)
        if mode != dr.ADMode.Primal:
            # Camera ray reparameterization determinant multiplied in ADIntegrator.sample_rays()
            ray_reparam.d, _ = reparam(ray, depth=0, active=active)

        # ---------------------- Direct emission ----------------------

        si = scene.ray_intersect(ray_reparam, active)
        L = si.emitter(scene).eval(si)

        return L, active, None

mi.register_integrator("emission_reparam", lambda props: EmissionReparamIntegrator(props))
