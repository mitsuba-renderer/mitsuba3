# TODO WIP implementation of discontinuities reparameterization helper routines

import enoki as ek
import mitsuba


def sample_warp_field(scene: mitsuba.render.Scene,
                      sampler: mitsuba.render.Sampler,
                      ray: mitsuba.core.RayDifferential3f,
                      kappa: float,
                      power: float,
                      active: mitsuba.core.Mask):
    from mitsuba.core import Frame3f, Ray3f
    from mitsuba.core.warp import square_to_von_mises_fisher, square_to_von_mises_fisher_pdf
    from mitsuba.render import HitComputeFlags

    # Sample auxiliary direction from vMF
    offset = square_to_von_mises_fisher(sampler.next_2d(active), kappa)
    omega = Frame3f(ek.detach(ray.d)).to_world(offset)
    pdf_omega = square_to_von_mises_fisher_pdf(offset, kappa)

    aux_ray = Ray3f(ray)
    aux_ray.d = omega

    si = scene.ray_intersect(aux_ray, HitComputeFlags.Sticky, active)
    # shading_normal = ek.normalize(si.p) # TODO
    # hit = active & ek.detach(si.is_valid()) & (ek.dot(omega, shading_normal) > 1e-3)
    hit = active & ek.detach(si.is_valid())

    # Convolve directions attached to the "sticky" position derivative: the
    # actual computation is a bit weird here as it needs to involve both the
    # sticky intersection, and potentially an (attached) ray origin in order
    # to support camera translations.
    V_direct = ek.normalize(si.p - aux_ray.o)
    V_direct = ek.select(hit, V_direct, ek.detach(aux_ray.d)) # Background doesn't move w.r.t. theta

    # Compute harmonic weight while being careful of division by almost zero
    B = ek.detach(ek.select(hit, si.boundary_test(aux_ray), 1.0))
    D = ek.exp(kappa - kappa * ek.dot(ray.d, aux_ray.d)) - 1.0
    w_denom = D + B
    w_denom_p = ek.pow(w_denom, power)
    w = ek.select(w_denom > 1e-4, ek.rcp(w_denom_p), 0.0)
    w /= pdf_omega
    w = ek.detach(w)

    # Analytic weight gradients w.r.t. `ray.d`
    tmp0 = ek.pow(w_denom, power + 1.0)
    tmp1 = (D + 1.0) * ek.rcp(tmp0) * kappa * power
    tmp2 = omega - ray.d * ek.dot(ray.d, omega)
    d_w_omega = ek.sign(tmp1) * ek.min(ek.abs(tmp1), 1e10) * tmp2
    d_w_omega /= pdf_omega
    d_w_omega = ek.detach(d_w_omega)

    return w, d_w_omega, w * V_direct, ek.dot(d_w_omega, V_direct)


def reparameterize_ray(scene: mitsuba.render.Scene,
                       sampler: mitsuba.render.Sampler,
                       ray_: mitsuba.core.RayDifferential3f,
                       active_: mitsuba.core.Mask,
                       params: mitsuba.python.util.SceneParameters={},
                       num_auxiliary_rays: int=4,
                       kappa: float=1e5,
                       power: float=3.0):
    """
    Reparameterize given ray by "attaching" the derivatives of its direction to
    moving geometry in the scene.

    TODO
    """

    class Reparameterizer(ek.CustomOp):
        """
        Custom Enoki operator to reparameterize rays in order to account for
        gradient discontinuities due to moving geometry in the scene.
        """
        def eval(self, scene, params, ray, active):
            from mitsuba.core import Float
            self.scene = scene
            self.params = params
            self.add_input(self.params)
            self.ray = ray
            self.active = active
            return self.ray.d, ek.zero(Float, ek.width(self.ray.d))

        def forward(self):
            from mitsuba.core import Float, UInt32, Vector3f, Loop

            # Initialize some accumulators
            Z  = Float(0.0)
            dZ = Vector3f(0.0)
            grad_V = Vector3f(0.0)
            grad_divergence_lhs = Float(0.0)

            it = UInt32(0)
            loop = Loop("Auxiliary rays")
            loop.put(lambda:(it, Z, dZ, grad_V, grad_divergence_lhs))
            sampler.loop_register(loop)
            loop.init()

            while loop(it < num_auxiliary_rays):
                Z_i, dZ_i, V_i, divergence_lhs_i = sample_warp_field(self.scene, sampler, self.ray, kappa, power, self.active)

                ek.enqueue(ek.ADMode.Forward, self.params)
                ek.traverse(Float, ek.ADFlag.ClearEdges | ek.ADFlag.ClearInterior)

                Z  += Z_i
                dZ += dZ_i
                grad_V += ek.grad(V_i)
                grad_divergence_lhs += ek.grad(divergence_lhs_i)

                it = it + 1

            Z = ek.max(Z, 1e-8)
            V_theta  = grad_V / Z
            div_V_theta = (grad_divergence_lhs - ek.dot(V_theta, dZ)) / Z

            self.set_grad_out((V_theta, div_V_theta))

        def backward(self):
            from mitsuba.core import Float, UInt32, Vector3f, Loop

            grad_direction, grad_divergence = self.grad_out()

            with ek.suspend_grad():
                # We need to trace the auxiliary rays a first time to compute the
                # constants Z and dZ in order to properly weight the incoming gradients
                Z       = Float(0.0)
                dZ      = Vector3f(0.0)
                sampler_clone = sampler.clone()

                it = UInt32(0)
                loop = Loop("Auxiliary rays - compute normalization terms")
                loop.put(lambda:(it, Z, dZ))
                sampler_clone.loop_register(loop)
                loop.init()
                while loop(it < num_auxiliary_rays):
                    Z_i, dZ_i, _, _ = sample_warp_field(self.scene, sampler_clone, self.ray, kappa, power, self.active)
                    Z  += Z_i
                    dZ += dZ_i
                    it = it + 1

            # Un-normalized values
            V       = ek.zero(Vector3f, ek.width(Z))
            div_V_1 = ek.zero(Float, ek.width(Z))
            ek.enable_grad(V, div_V_1)

            # Compute normalized values
            V_theta = V / Z
            divergence = (div_V_1 - ek.dot(V_theta, dZ)) / Z
            direction = ek.normalize(self.ray.d + V_theta)

            ek.set_grad(direction, grad_direction)
            ek.set_grad(divergence, grad_divergence)
            ek.enqueue(ek.ADMode.Reverse, direction, divergence)
            ek.traverse(Float)

            grad_V       = ek.grad(V)
            grad_div_V_1 = ek.grad(div_V_1)

            it = UInt32(0)
            loop = Loop("Auxiliary rays - backpropagation")
            loop.put(lambda:(it))
            sampler.loop_register(loop)
            loop.init()
            while loop(it < num_auxiliary_rays):
                _, _, V_i, div_V_1_i = sample_warp_field(self.scene, sampler, self.ray, kappa, power, self.active)
                # print(ek.graphviz_str(Float(1)))
                ek.set_grad(V_i, grad_V)
                ek.set_grad(div_V_1_i, grad_div_V_1)
                ek.enqueue(ek.ADMode.Reverse, V_i, div_V_1_i)
                ek.traverse(Float, ek.ADFlag.ClearVertices)
                it = it + 1

        def name(self):
            return "ReparameterizeRay"

    return ek.custom(Reparameterizer, scene, params, ray_, active_)
