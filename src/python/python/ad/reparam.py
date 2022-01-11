from __future__ import annotations # Delayed parsing of type annotations

import enoki as ek
import mitsuba

from typing import Tuple

def _sample_warp_field(scene: mitsuba.render.Scene,
                       sample: mitsuba.core.Point2f,
                       ray: mitsuba.core.Ray3f,
                       ray_frame: mitsuba.core.Frame3f,
                       flip: mitsuba.core.Bool,
                       kappa: float,
                       exponent: float):
    """
    Helper function for re-parameterizing rays based on the paper

      "Unbiased Warped-Area Sampling for Differentiable Rendering"
      (Procedings of SIGGRAPH'20) by Sai Praveen Bangaru,
      Tzu-Mao Li, and Frédo Durand.

    The function is an implementation of the _ReparameterizeOp class below
    (which is in turn an implementation detail of the reparameterize_rays()
    function). It traces a single auxiliary ray and returns an attached 3D
    direction and sample weight. A number of calls to this function are
    generally necessary to reduce the bias of the parameterization to an
    acceptable level.

    The function assumes that all inputs are *detached* from the AD graph, with
    one exception: the ray origin (``ray.o``) may have derivative tracking
    enabled, which influences the returned directions.

    It has the following inputs:

    Parameter ``scene`` (``mitsuba.render.Scene``):
        The scene being rendered differentially.

    Parameter ``rng`` (``mitsuba.core.Point2f``):
        A uniformly distributed random variate on the domain [0, 1]^2.

    Parameter ``ray`` (``mitsuba.core.Ray3f``):
        The input ray that should be reparameterized.

    Parameter ``ray_frame`` (``mitsuba.core.Frame3f``):
        A precomputed orthonormal basis with `ray_frame.n = ray.d`.

    Parameter ``kappa`` (``float``):
        Kappa parameter of the von Mises Fisher distribution used to
        sample auxiliary rays.

    Parameter ``exponent`` (``float``):
        Exponent used in the computation of the harmonic weights.

    The function returns a tuple ``(Z, dZ, V, div)`` containig

    Return value ``Z`` (``mitsuba.core.Float``)
        The harmonic weight of the generated sample (detached).

    Return value ``dZ`` (``mitsuba.core.Vector3f``)
        The gradient of ``Z`` with respect to tangential changes
        in ``ray.d`` (detached).

    Return value ``V`` (``mitsuba.core.Vector3f``)
        The weighted warp field value (attached).

    Return value ``div`` (``mitsuba.core.Vector3f``)
        Dot product between dZ and the unweighted warp field, which
        is used to compute the divergence of the warp field/jacobian
        determinant of the parameterization.
    """

    from mitsuba.core import Ray3f, Vector3f, warp
    from mitsuba.render import RayFlags

    # Sample an auxiliary ray from a von Mises Fisher distribution
    omega_local = warp.square_to_von_mises_fisher(sample, kappa)

    # Antithetic sampling (optional)
    omega_local.x = ek.select(flip, -omega_local.x, omega_local.x)
    omega_local.y = ek.select(flip, -omega_local.y, omega_local.y)

    aux_ray = Ray3f(
        o = ray.o,
        d = ray_frame.to_world(omega_local),
        time = ray.time)

    # Compute an intersection that follows the intersected shape. For example,
    # a perturbation of the translation parameter will propagate to 'si.p'.
    si = scene.ray_intersect(aux_ray,
                             ray_flags=RayFlags.FollowShape,
                             coherent=False)

    # Convert into a direction at 'ray.o'. When no surface was intersected,
    # copy the original (static) direction
    hit = si.is_valid()
    V_direct = ek.select(hit, ek.normalize(si.p - ray.o), ray.d)

    with ek.suspend_grad():
        # Boundary term provided by the underlying shape (polymorphic)
        B = ek.select(hit, si.boundary_test(aux_ray), 1.0)

        # Inverse of vMF density without normalization constant
        # inv_vmf_density = ek.exp(ek.fnmadd(omega_local.z, kappa, kappa))

        # Better version (here, ek.exp() is constant). However, assumes a
        # specific implementation in warp.square_to_von_mises_fisher() (TODO)
        inv_vmf_density = ek.rcp(ek.fmadd(sample.y, ek.exp(-2 * kappa), 1 - sample.y))

        # Compute harmonic weight, being wary of division by near-zero values
        w_denom = inv_vmf_density + B - 1
        w_denom_rcp = ek.select(w_denom > 1e-4, ek.rcp(w_denom), 0.0)
        w = ek.pow(w_denom_rcp, exponent) * inv_vmf_density

        # Analytic weight gradient w.r.t. `ray.d`
        tmp1 = inv_vmf_density * w * w_denom_rcp * kappa * exponent
        tmp2 = ray_frame.to_world(Vector3f(omega_local.x, omega_local.y, 0))
        d_w_omega = ek.clamp(tmp1, -1e10, 1e10) * tmp2

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
    def eval(self, scene, rng, params, ray, num_rays, kappa, exponent, 
             antithetic, active):
        # Stash all of this information for the forward/backward passes
        self.scene = scene
        self.rng = rng
        self.params = params
        self.ray = ek.detach(ray)
        self.num_rays = num_rays
        self.kappa = kappa
        self.exponent = exponent
        self.active = active
        self.antithetic = antithetic

        # The reparameterization is simply the identity in primal mode
        return self.ray.d, ek.full(mitsuba.core.Float, 1, ek.width(ray))


    def forward(self):
        from mitsuba.core import (Bool, Float, UInt32, Loop, Point2f,
                                  Vector3f, Ray3f, Frame3f)

        # Initialize some accumulators
        Z = Float(0.0)
        dZ = Vector3f(0.0)
        grad_V = Vector3f(0.0)
        grad_div_lhs = Float(0.0)
        it = UInt32(0)
        rng = self.rng
        ray_grad = self.grad_in('ray')
        ray_frame = Frame3f(self.ray.d)

        loop = Loop(name="reparameterize_ray(): forward propagation",
                    state=lambda: (it, Z, dZ, grad_V, grad_div_lhs, rng.state))

        # Unroll the entire loop in wavefront mode
        loop.set_uniform(True)
        loop.set_max_iterations(self.num_rays)
        loop.set_eval_stride(self.num_rays)

        while loop(self.active & (it < self.num_rays)):
            ray = Ray3f(self.ray)
            ek.enable_grad(ray.o)
            ek.set_grad(ray.o, ray_grad.o)

            rng_state_backup = rng.state
            sample = Point2f(rng.next_float32(),
                             rng.next_float32())

            if self.antithetic:
                repeat = ek.eq(it & 1, 0)
                rng.state[repeat] = rng_state_backup
            else:
                repeat = Bool(False)

            Z_i, dZ_i, V_i, div_lhs_i = _sample_warp_field(self.scene, sample,
                                                           ray, ray_frame,
                                                           repeat, self.kappa,
                                                           self.exponent)

            ek.forward_to(V_i, div_lhs_i,
                          flags=ek.ADFlag.ClearEdges | ek.ADFlag.ClearInterior)

            Z += Z_i
            dZ += dZ_i
            grad_V += ek.grad(V_i)
            grad_div_lhs += ek.grad(div_lhs_i)
            it += 1

        inv_Z = ek.rcp(ek.max(Z, 1e-8))
        V_theta  = grad_V * inv_Z
        div_V_theta = (grad_div_lhs - ek.dot(V_theta, dZ)) * inv_Z

        # Ignore inactive lanes
        V_theta = ek.select(self.active, V_theta, 0.0)
        div_V_theta = ek.select(self.active, div_V_theta, 0.0)

        self.set_grad_out((V_theta, div_V_theta))


    def backward(self):
        from mitsuba.core import Bool, Float, UInt32, Point2f, \
            Point3f, Vector3f, Ray3f, Frame3f, Loop, PCG32 

        grad_direction, grad_divergence = self.grad_out()

        # Ignore inactive lanes
        grad_direction  = ek.select(self.active, grad_direction, 0.0)
        grad_divergence = ek.select(self.active, grad_divergence, 0.0)
        ray_frame = Frame3f(self.ray.d)
        rng = self.rng
        rng_clone = PCG32(rng)

        with ek.suspend_grad():
            # We need to trace the auxiliary rays a first time to compute the
            # constants Z and dZ in order to properly weight the incoming gradients
            Z = Float(0.0)
            dZ = Vector3f(0.0)
            it = UInt32(0)

            loop = Loop(name="reparameterize_ray(): weight normalization",
                        state=lambda: (it, Z, dZ, rng_clone.state))

            # Unroll the entire loop in wavefront mode
            loop.set_uniform(True)
            loop.set_max_iterations(self.num_rays)
            loop.set_eval_stride(self.num_rays)

            while loop(self.active & (it < self.num_rays)):
                rng_state_backup = rng_clone.state

                sample = Point2f(rng_clone.next_float32(),
                                 rng_clone.next_float32())

                if self.antithetic:
                    repeat = ek.eq(it & 1, 0)
                    rng_clone.state[repeat] = rng_state_backup
                else:
                    repeat = Bool(False)

                Z_i, dZ_i, _, _ = _sample_warp_field(self.scene, sample,
                                                     self.ray, ray_frame,
                                                     repeat, self.kappa,
                                                     self.exponent)
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
        ray_grad_o = Point3f(0)

        loop = Loop(name="reparameterize_ray(): backpropagation",
                    state=lambda: (it, rng.state, ray_grad_o))

        # Unroll the entire loop in wavefront mode
        loop.set_uniform(True)
        loop.set_max_iterations(self.num_rays)
        loop.set_eval_stride(self.num_rays)

        while loop(self.active & (it < self.num_rays)):
            ray = Ray3f(self.ray)
            ek.enable_grad(ray.o)

            rng_state_backup = rng.state

            sample = Point2f(rng.next_float32(),
                             rng.next_float32())

            if self.antithetic:
                repeat = ek.eq(it & 1, 0)
                rng.state[repeat] = rng_state_backup
            else:
                repeat = Bool(False)

            _, _, V_i, div_V_1_i = _sample_warp_field(self.scene, sample,
                                                      self.ray, ray_frame,
                                                      repeat, self.kappa,
                                                      self.exponent)

            ek.set_grad(V_i, grad_V)
            ek.set_grad(div_V_1_i, grad_div_V_1)
            ek.enqueue(ek.ADMode.Backward, V_i, div_V_1_i)
            ek.traverse(Float, ek.ADMode.Backward, ek.ADFlag.ClearVertices)
            ray_grad_o += ek.grad(ray.o)
            it += 1

        ray_grad = ek.zero(type(self.ray))
        ray_grad.o = ray_grad_o
        self.set_grad_in('ray', ray_grad)

    def name(self):
        return "reparameterize_ray()"


def reparameterize_ray(scene: mitsuba.render.Scene,
                       rng: mitsuba.core.PCG32,
                       params: Any,
                       ray: mitsuba.core.Ray3f,
                       num_rays: int=4,
                       kappa: float=1e5,
                       exponent: float=3.0,
                       antithetic: bool=True,
                       active: Any[bool, mitsuba.core.Bool] = True
) -> Tuple[mitsuba.core.Vector3f, mitsuba.core.Float]:
    """
    Reparameterize given ray by "attaching" the derivatives of its direction to
    moving geometry in the scene.

    Parameter ``scene`` (``mitsuba.render.Scene``):
        Scene containing all shapes.

    Parameter ``rng`` (``mitsuba.core.PCG32``):
        Random number generator used to sample auxiliary ray directions.

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

    Parameter ``antithetic`` (``bool``):
        Should antithetic sampling be enabled to improve convergence?

    Parameter ``active`` (``mitsuba.core.Bool``):
        Boolean array specifying the active lanes

    Returns → (mitsuba.core.Vector3f, mitsuba.core.Float):
        Returns the reparameterized ray direction and the Jacobian 
        determinant of the change of variables.
    """

    return ek.custom(_ReparameterizeOp, scene, rng, params, ray, 
                     num_rays, kappa, exponent, antithetic, active)
