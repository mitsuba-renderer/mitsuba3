import enoki as ek
import mitsuba

from typing import Tuple

def _sample_warp_field(scene: mitsuba.render.Scene,
                       sampler: mitsuba.render.Sampler,
                       ray: mitsuba.core.RayDifferential3f,
                       kappa: float,
                       power: float,
                       active: mitsuba.core.Mask):
    """
    Sample the warp field of moving geometries by tracing an auxiliary ray and
    computing the appropriate convolution weight using the shape's boundary-test.
    """
    from mitsuba.core import Frame3f, Ray3f
    from mitsuba.core.warp import square_to_von_mises_fisher, square_to_von_mises_fisher_pdf
    from mitsuba.render import RayFlags

    # Sample auxiliary direction from vMF
    offset = square_to_von_mises_fisher(sampler.next_2d(active), kappa)
    omega = Frame3f(ek.detach(ray.d)).to_world(offset)
    pdf_omega = square_to_von_mises_fisher_pdf(offset, kappa)

    aux_ray = Ray3f(ray)
    aux_ray.d = omega

    si = scene.ray_intersect(aux_ray, RayFlags.FollowShape, active)
    # shading_normal = ek.normalize(si.p) # TODO
    # hit = active & ek.detach(si.is_valid()) & (ek.dot(omega, shading_normal) > 1e-3)
    hit = active & ek.detach(si.is_valid())

    # Compute warp field direction such that it follows the intersected shape
    # and moves along with the ray origin.
    V_direct = ek.normalize(si.p - aux_ray.o)

    # Background doesn't move w.r.t. scene parameters
    V_direct = ek.select(hit, V_direct, ek.detach(aux_ray.d))

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
    tmp1 = (D + 1.0) * ek.select(w_denom > 1e-4, ek.rcp(tmp0), 0.0) * kappa * power
    tmp2 = omega - ray.d * ek.dot(ray.d, omega)
    d_w_omega = ek.sign(tmp1) * ek.min(ek.abs(tmp1), 1e10) * tmp2
    d_w_omega /= pdf_omega
    d_w_omega = ek.detach(d_w_omega)

    return w, d_w_omega, w * V_direct, ek.dot(d_w_omega, V_direct)


def reparameterize_ray(scene: mitsuba.render.Scene,
                       sampler: mitsuba.render.Sampler,
                       ray: mitsuba.core.RayDifferential3f,
                       params: mitsuba.python.util.SceneParameters={},
                       active: mitsuba.core.Mask=True,
                       num_auxiliary_rays: int=4,
                       kappa: float=1e5,
                       power: float=3.0) -> Tuple[mitsuba.core.Vector3f, mitsuba.core.Float]:
    """
    Reparameterize given ray by "attaching" the derivatives of its direction to
    moving geometry in the scene.

    Parameter ``scene`` (``mitsuba.render.Scene``):
        Scene containing all shapes.

    Parameter ``sampler`` (``mitsuba.render.Sampler``):
        Sampler object used to sample auxiliary rays direction.

    Parameter ``ray`` (``mitsuba.core.RayDifferential3f``):
        Ray to be reparameterized

    Parameter ``params`` (``mitsuba.python.util.SceneParameters``):
        Scene parameters

    Parameter ``active`` (``mitsuba.core.Mask``):
        Mask specifying the active lanes

    Parameter ``num_auxiliary_rays`` (``int``):
        Number of auxiliary rays to trace when performing the convolution.

    Parameter ``kappa`` (``float``):
        Kappa parameter of the von Mises Fisher distribution used to sample the
        auxiliary rays.

    Parameter ``power`` (``float``):
        Power value used to control the harmonic weights.

    Returns → (mitsuba.core.Vector3f, mitsuba.core.Float):
        Reparameterized ray direction and divergence value of the warp field.
    """

    num_auxiliary_rays = ek.opaque(mitsuba.core.UInt32, num_auxiliary_rays)
    kappa = ek.opaque(mitsuba.core.Float, kappa)
    power = ek.opaque(mitsuba.core.Float, power)

    class ReparameterizeOp(ek.CustomOp):
        """
        Custom Enoki operator to reparameterize rays in order to account for
        gradient discontinuities due to moving geometry in the scene.
        """
        def eval(self, scene, params, ray, active):
            self.scene = scene
            self.params = params
            self.ray = ray
            self.active = active

            return self.ray.d, ek.zero(mitsuba.core.Float,
                                       ek.width(self.ray.d))


        def forward(self):
            from mitsuba.core import Float, UInt32, Vector3f, Loop, Ray3f

            # Initialize some accumulators
            Z = Float(0.0)
            dZ = Vector3f(0.0)
            grad_V = Vector3f(0.0)
            grad_div_lhs = Float(0.0)
            ray = Ray3f(ek.detach(self.ray))

            it = UInt32(0)
            loop = Loop(name="reparameterize_ray(): forward propagation",
                        state=lambda: (it, Z, dZ, grad_V, grad_div_lhs, sampler))

            while loop(active & (it < num_auxiliary_rays)):
                ek.enable_grad(ray.o)
                ek.set_grad(ray.o, self.grad_in('ray').o)
                Z_i, dZ_i, V_i, div_lhs_i = _sample_warp_field(self.scene, sampler, ray,
                                                               kappa, power, self.active)

                ek.enqueue(ek.ADMode.Forward, self.params, ray.o)
                ek.traverse(Float, ek.ADMode.Forward, ek.ADFlag.ClearEdges | ek.ADFlag.ClearInterior)

                Z += Z_i
                dZ += dZ_i
                grad_V += ek.grad(V_i)
                grad_div_lhs += ek.grad(div_lhs_i)

                it = it + 1

            Z = ek.max(Z, 1e-8)
            V_theta  = grad_V / Z
            div_V_theta = (grad_div_lhs - ek.dot(V_theta, dZ)) / Z

            # Ignore inactive lanes
            V_theta = ek.select(self.active, V_theta, 0.0)
            div_V_theta = ek.select(self.active, div_V_theta, 0.0)

            self.set_grad_out((V_theta, div_V_theta))


        def backward(self):
            from mitsuba.core import Float, UInt32, Vector3f, Loop

            grad_direction, grad_divergence = self.grad_out()

            # Ignore inactive lanes
            grad_direction  = ek.select(self.active, grad_direction, 0.0)
            grad_divergence = ek.select(self.active, grad_divergence, 0.0)

            with ek.suspend_grad():
                # We need to trace the auxiliary rays a first time to compute the
                # constants Z and dZ in order to properly weight the incoming gradients
                Z = Float(0.0)
                dZ = Vector3f(0.0)
                sampler_clone = sampler.clone()

                it = UInt32(0)
                loop = Loop(name="reparameterize_ray(): normalization",
                            state=lambda: (it, Z, dZ, sampler_clone))

                while loop(active & (it < num_auxiliary_rays)):
                    Z_i, dZ_i, _, _ = _sample_warp_field(self.scene, sampler_clone, self.ray,
                                                         kappa, power, self.active)
                    Z += Z_i
                    dZ += dZ_i
                    it += 1

            # Un-normalized values
            V = ek.zero(Vector3f, ek.width(Z))
            div_V_1 = ek.zero(Float, ek.width(Z))
            ek.enable_grad(V, div_V_1)

            # Compute normalized values
            Z = ek.max(Z, 1e-8)
            V_theta = V / Z
            divergence = (div_V_1 - ek.dot(V_theta, dZ)) / Z
            direction = ek.normalize(self.ray.d + V_theta)

            ek.set_grad(direction, grad_direction)
            ek.set_grad(divergence, grad_divergence)
            ek.enqueue(ek.ADMode.Backward, direction, divergence)
            ek.traverse(Float, ek.ADMode.Backward)

            grad_V = ek.grad(V)
            grad_div_V_1 = ek.grad(div_V_1)

            it = UInt32(0)
            loop = Loop(name="reparameterize_ray(): backpropagation",
                        state=lambda: (it, sampler))

            while loop(active & (it < num_auxiliary_rays)):
                _, _, V_i, div_V_1_i = _sample_warp_field(self.scene, sampler, self.ray,
                                                          kappa, power, self.active)

                ek.set_grad(V_i, grad_V)
                ek.set_grad(div_V_1_i, grad_div_V_1)
                ek.enqueue(ek.ADMode.Backward, V_i, div_V_1_i)
                ek.traverse(Float, ek.ADMode.Backward, ek.ADFlag.ClearVertices)
                it += 1

        def name(self):
            return "reparameterize_ray()"

    return ek.custom(ReparameterizeOp, scene, params, ray, active)
