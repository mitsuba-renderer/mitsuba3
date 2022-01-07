from __future__ import annotations # Delayed parsing of type annotations

import enoki as ek
import mitsuba

from typing import Tuple

def _sample_warp_field(scene: mitsuba.render.Scene,
                       sample: mitsuba.core.Point2f,
                       ray: mitsuba.core.Ray3f,
                       kappa: float,
                       exponent: float):
    """
    Sample the warp field of moving geometries by tracing an auxiliary ray and
    computing the appropriate convolution weight using the shape's boundary-test.
    """
    from mitsuba.core import Ray3f, Frame3f
    from mitsuba.core.warp import square_to_von_mises_fisher, square_to_von_mises_fisher_pdf
    from mitsuba.render import RayFlags

    # Sample an auxiliary ray from a von Mises Fisher distribution (+ antithetic sampling)
    omega_local = square_to_von_mises_fisher(sample, kappa)
    omega = Frame3f(ray.d).to_world(omega_local)
    pdf_omega = square_to_von_mises_fisher_pdf(omega_local, kappa)
    aux_ray = Ray3f(ray)
    aux_ray.d = omega

    # Compute an intersection that follows the intersected shape. For example,
    # a perturbation of the translation parameter will propagate to 'si.p'.
    si = scene.ray_intersect(aux_ray, ray_flags=RayFlags.FollowShape,
                             coherent=False)

    # Convert into a direction at 'ray.o'. When no surface was intersected,
    # copy the original (static) direction
    hit = si.is_valid()
    V_direct = ek.select(hit, ek.normalize(si.p - ray.o), ray.d)

    # Compute harmonic weight while being careful of division by almost zero
    B = ek.select(hit, ek.detach(si.boundary_test(aux_ray)), 1.0)
    D = ek.exp(kappa - kappa * ek.dot(ray.d, aux_ray.d)) - 1.0
    w_denom = D + B
    w_denom_p = ek.pow(w_denom, exponent)
    w = ek.select(w_denom > 1e-4, ek.rcp(w_denom_p), 0.0)
    w /= pdf_omega

    # Analytic weight gradients w.r.t. `ray.d`
    tmp0 = ek.pow(w_denom, exponent + 1.0)
    tmp1 = (D + 1.0) * ek.select(w_denom > 1e-4, ek.rcp(tmp0), 0.0) * kappa * exponent
    tmp2 = omega - ray.d * ek.dot(ray.d, omega)
    d_w_omega = ek.sign(tmp1) * ek.min(ek.abs(tmp1), 1e10) * tmp2
    d_w_omega /= pdf_omega

    ek.disable_grad(w, d_w_omega)

    return w, d_w_omega, w * V_direct, ek.dot(d_w_omega, V_direct)

class _ReparameterizeOp(ek.CustomOp):
    """
    Enoki custom operation that reparameterizes rays based on the paper

      "Unbiased Warped-Area Sampling for Differentiable Rendering"
      (Procedings of SIGGRAPH'20) by Sai Praveen Bangaru,
      Tzu-Mao Li, and Frédo Durand.

    This is needed to to avoid bias caused by the discontinuous visibility
    function in gradient-based geometric optimization.
    """
    def eval(self, scene, rng, params, ray, num_rays, kappa, exponent, active):
        # Stash all of this information for the forward/backward passes
        self.scene = scene
        self.rng = rng
        self.params = params
        self.ray = ek.detach(ray)
        self.num_rays = num_rays
        self.kappa = kappa
        self.exponent = exponent
        self.active = active

        # The reparameterization is simply the identity in primal mode
        return self.ray.d, ek.full(mitsuba.core.Float, 1, ek.width(ray))


    def forward(self):
        from mitsuba.core import (Bool, Float, UInt32, Loop, Point2f,
                                  Vector3f, Ray3f)

        # Initialize some accumulators
        Z = Float(0.0)
        dZ = Vector3f(0.0)
        grad_V = Vector3f(0.0)
        grad_div_lhs = Float(0.0)
        it = UInt32(0)
        rng = self.rng

        loop = Loop(name="reparameterize_ray(): forward propagation",
                    state=lambda: (it, Z, dZ, grad_V, grad_div_lhs, rng.state))

        while loop(self.active & (it < self.num_rays)):
            ray = Ray3f(self.ray)
            ek.enable_grad(ray.o)
            ek.set_grad(ray.o, self.grad_in('ray').o)

            sample = Point2f(rng.next_float32(),
                             rng.next_float32())

            Z_i, dZ_i, V_i, div_lhs_i = _sample_warp_field(
                self.scene, sample, ray, self.kappa, self.exponent)

            ek.enqueue(ek.ADMode.Forward, self.params, ray.o)
            ek.traverse(Float, ek.ADMode.Forward, ek.ADFlag.ClearEdges | ek.ADFlag.ClearInterior)

            Z += Z_i
            dZ += dZ_i
            grad_V += ek.grad(V_i)
            grad_div_lhs += ek.grad(div_lhs_i)
            it += 1

        Z = ek.max(Z, 1e-8)
        V_theta  = grad_V / Z
        div_V_theta = (grad_div_lhs - ek.dot(V_theta, dZ)) / Z

        # Ignore inactive lanes
        V_theta = ek.select(self.active, V_theta, 0.0)
        div_V_theta = ek.select(self.active, div_V_theta, 0.0)

        self.set_grad_out((V_theta, div_V_theta))


    def backward(self):
        from mitsuba.core import Float, UInt32, Point2f, Vector3f, Loop, PCG32

        grad_direction, grad_divergence = self.grad_out()

        # Ignore inactive lanes
        grad_direction  = ek.select(self.active, grad_direction, 0.0)
        grad_divergence = ek.select(self.active, grad_divergence, 0.0)
        rng = self.rng
        rng_clone = PCG32(rng)

        with ek.suspend_grad():
            # We need to trace the auxiliary rays a first time to compute the
            # constants Z and dZ in order to properly weight the incoming gradients
            Z = Float(0.0)
            dZ = Vector3f(0.0)

            it = UInt32(0)
            loop = Loop(name="reparameterize_ray(): normalization",
                        state=lambda: (it, Z, dZ, rng_clone.state))

            while loop(self.active & (it < self.num_rays)):
                sample = Point2f(rng_clone.next_float32(),
                                 rng_clone.next_float32())

                Z_i, dZ_i, _, _ = _sample_warp_field(self.scene, sample, self.ray,
                                                     self.kappa, self.exponent)
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
                    state=lambda: (it, rng.state))

        while loop(self.active & (it < self.num_rays)):
            sample = Point2f(rng.next_float32(),
                             rng.next_float32())

            _, _, V_i, div_V_1_i = _sample_warp_field(self.scene, sample,
                                                      self.ray, self.kappa,
                                                      self.exponent)

            ek.set_grad(V_i, grad_V)
            ek.set_grad(div_V_1_i, grad_div_V_1)
            ek.enqueue(ek.ADMode.Backward, V_i, div_V_1_i)
            ek.traverse(Float, ek.ADMode.Backward, ek.ADFlag.ClearVertices)
            it += 1

    def name(self):
        return "reparameterize_ray()"


def reparameterize_ray(scene: mitsuba.render.Scene,
                       rng: mitsuba.core.PCG32,
                       params: Any,
                       ray: mitsuba.core.Ray3f,
                       num_rays: int=4,
                       kappa: float=1e5,
                       exponent: float=3.0,
                       active: Any[bool, mitsuba.core.Bool] = True
) -> Tuple[mitsuba.core.Vector3f, mitsuba.core.Float]:
    """
    Reparameterize given ray by "attaching" the derivatives of its direction to
    moving geometry in the scene.

    Parameter ``scene`` (``mitsuba.render.Scene``):
        Scene containing all shapes.

    Parameter ``rng`` (``mitsuba.core.PCG32``):
        Random number generator used to sample auxiliary ray direction.

    Parameter ``ray`` (``mitsuba.core.RayDifferential3f``):
        Ray to be reparameterized

    Parameter ``params`` (``mitsuba.python.util.SceneParameters``):
        Scene parameters

    Parameter ``num_rays`` (``int``):
        Number of auxiliary rays to trace when performing the convolution.

    Parameter ``kappa`` (``float``):
        Kappa parameter of the von Mises Fisher distribution used to sample the
        auxiliary rays.

    Parameter ``exponent`` (``float``):
        Exponent used in the computation of the harmonic weights

    Parameter ``active`` (``mitsuba.core.Bool``):
        Boolean array specifying the active lanes

    Returns → (mitsuba.core.Vector3f, mitsuba.core.Float):
        Returns the reparameterized ray direction and the Jacobian 
        determinant of the change of variables.
    """

    return ek.custom(_ReparameterizeOp, scene, rng, params, ray, 
                     num_rays, kappa, exponent, active)
