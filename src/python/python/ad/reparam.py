from __future__ import annotations as __annotations__ # Delayed parsing of type annotations

import drjit as dr
import mitsuba as mi

import typing
if typing.TYPE_CHECKING:
    from typing import Tuple

def _sample_warp_field(scene: mi.Scene,
                       sample: mi.Point2f,
                       ray: mi.Ray3f,
                       ray_frame: mi.Frame3f,
                       flip: mi.Bool,
                       kappa: float,
                       exponent: float):
    """
    Helper function for reparameterizing rays based on the paper

      "Unbiased Warped-Area Sampling for Differentiable Rendering"
      (Proceedings of SIGGRAPH'20) by Sai Praveen Bangaru,
      Tzu-Mao Li, and Frédo Durand.

    The function is an implementation of the _ReparameterizeOp class below
    (which is in turn an implementation detail of the reparameterize_rays()
    function). It traces a single auxiliary ray and returns an attached 3D
    direction and sample weight. A number of calls to this function are
    generally necessary to reduce the bias of the parameterization to an
    acceptable level.

    The function assumes that all inputs are *detached* from the AD graph, with
    one exception: the ray origin (``ray.o``) may have derivative tracking
    enabled, which influences the returned directions. The scene parameter π
    as an implicit input can also have derivative tracking enabled.

    It has the following inputs:

    Parameter ``scene`` (``mitsuba.Scene``):
        The scene being rendered differentially.

    Parameter ``sample`` (``mitsuba.Point2f``):
        A uniformly distributed random variate on the domain [0, 1]^2.

    Parameter ``ray`` (``mitsuba.Ray3f``):
        The input ray that should be reparameterized.

    Parameter ``ray_frame`` (``mitsuba.Frame3f``):
        A precomputed orthonormal basis with `ray_frame.n = ray.d`.

    Parameter ``kappa`` (``float``):
        Kappa parameter of the von Mises Fisher distribution used to
        sample auxiliary rays.

    Parameter ``exponent`` (``float``):
        Power exponent applied on the computed harmonic weights.

    The function returns a tuple ``(Z, dZ, V, div)`` containing

    Return value ``Z`` (``mitsuba.Float``)
        The harmonic weight of the generated sample. (detached)

    Return value ``dZ`` (``mitsuba.Vector3f``)
        The gradient of ``Z`` with respect to tangential changes
        in ``ray.d``. (detached)

    Return value ``V`` (``mitsuba.Vector3f``)
        The weighted mi.warp field value. Taking the derivative w.r.t.
        scene parameter π gives the vector field that follows an object's
        motion due to π. (attached)

    Return value ``div_lhs`` (``mitsuba.Vector3f``)
        Dot product between dZ and the unweighted mi.warp field, which is
        an intermediate value used to compute the divergence of the
        mi.warp field/jacobian determinant of the reparameterization.
        (attached)
    """

    # Sample an auxiliary ray from a von Mises Fisher distribution
    omega_local = mi.warp.square_to_von_mises_fisher(sample, kappa)

    # Antithetic sampling (optional)
    omega_local.x[flip] = -omega_local.x
    omega_local.y[flip] = -omega_local.y

    aux_ray = mi.Ray3f(
        o = ray.o,
        d = ray_frame.to_world(omega_local),
        time = ray.time)

    # Compute an intersection that follows the intersected shape. For example,
    # a perturbation of the translation parameter will propagate to 'si.p'.
    # Propagation of such gradients guarantees the correct evaluation of 'V_direct'.
    si = scene.ray_intersect(aux_ray,
                             ray_flags=mi.RayFlags.All | mi.RayFlags.FollowShape | mi.RayFlags.BoundaryTest,
                             coherent=False)

    # Convert into a direction at 'ray.o'. When no surface was intersected,
    # copy the original direction
    hit = si.is_valid()
    V_direct = dr.select(hit, dr.normalize(si.p - ray.o), ray.d)

    with dr.suspend_grad():
        # Boundary term provided by the underlying shape
        B = dr.select(hit, si.boundary_test, 1.0)

        # Inverse of vMF density without normalization constant
        # inv_vmf_density = dr.exp(dr.fma(-omega_local.z, kappa, kappa))

        # Better version (here, dr.exp() is constant). However, assumes a
        # specific implementation in mi.warp.square_to_von_mises_fisher() (TODO)
        inv_vmf_density = dr.rcp(dr.fma(sample.y, dr.exp(-2 * kappa), 1 - sample.y))

        # Compute harmonic weight, being wary of division by near-zero values
        w_denom = inv_vmf_density - 1 + B
        w_denom_rcp = dr.select(w_denom > 1e-4, dr.rcp(w_denom), 0.0)  # 1 / (D + B)
        w = dr.power(w_denom_rcp, exponent) * inv_vmf_density

        # Analytic weight gradient w.r.t. `ray.d` (detaching inv_vmf_density gradient)
        tmp1 = inv_vmf_density * w * w_denom_rcp * kappa * exponent
        tmp2 = ray_frame.to_world(mi.Vector3f(omega_local.x, omega_local.y, 0))
        d_w_omega = dr.clamp(tmp1, -1e10, 1e10) * tmp2

    return w, d_w_omega, w * V_direct, dr.dot(d_w_omega, V_direct)


class _ReparameterizeOp(dr.CustomOp):
    """
    Dr.Jit custom operation that reparameterizes rays based on the paper

      "Unbiased Warped-Area Sampling for Differentiable Rendering"
      (Proceedings of SIGGRAPH'20) by Sai Praveen Bangaru,
      Tzu-Mao Li, and Frédo Durand.

    This is needed to to avoid bias caused by the discontinuous visibility
    function in gradient-based geometric optimization.
    """
    def eval(self, scene, rng, params, ray, num_rays, kappa, exponent,
             antithetic, unroll, active):
        # Stash all of this information for the forward/backward passes
        self.scene = scene
        self.rng = rng
        self.params = params
        self.ray = dr.detach(ray)
        self.num_rays = num_rays
        self.kappa = kappa
        self.exponent = exponent
        self.active = active
        self.antithetic = antithetic
        self.unroll = unroll

        # The reparameterization is simply the identity in primal mode
        return self.ray.d, dr.full(mi.Float, 1, dr.width(ray))


    def forward(self):
        """
        Propagate the gradients in the forward direction to 'ray.d' and the
        jacobian determinant 'det'. From a warp field point of view, the
        derivative of 'ray.d' is the warp field direction at 'ray', and
        the derivative of 'det' is the divergence of the warp field at 'ray'.
        """

        # Initialize some accumulators
        Z = mi.Float(0.0)
        dZ = mi.Vector3f(0.0)
        grad_V = mi.Vector3f(0.0)
        grad_div_lhs = mi.Float(0.0)
        it = mi.UInt32(0)
        rng = self.rng
        ray_grad_o = self.grad_in('ray').o
        ray_grad_d = self.grad_in('ray').d

        loop = mi.Loop(name="reparameterize_ray(): forward propagation",
                       state=lambda: (it, Z, dZ, grad_V, grad_div_lhs, rng.state))

        # Unroll the entire loop in wavefront mode
        # loop.set_uniform(True) # TODO can we turn this back on? (see self.active in loop condition)
        loop.set_max_iterations(self.num_rays)
        loop.set_eval_stride(self.num_rays)

        while loop(self.active & (it < self.num_rays)):
            ray = mi.Ray3f(self.ray)
            dr.enable_grad(ray.o)
            dr.set_grad(ray.o, ray_grad_o)
            dr.enable_grad(ray.d)
            dr.set_grad(ray.d, ray_grad_d)
            ray_frame = mi.Frame3f(ray.d)

            rng_state_backup = rng.state
            sample = mi.Point2f(rng.next_float32(), rng.next_float32())

            if self.antithetic:
                repeat = dr.eq(it & 1, 0)
                rng.state[repeat] = rng_state_backup
            else:
                repeat = mi.Bool(False)

            Z_i, dZ_i, V_i, div_lhs_i = _sample_warp_field(self.scene, sample,
                                                           ray, ray_frame,
                                                           repeat, self.kappa,
                                                           self.exponent)

            # Do not clear input vertex gradient
            dr.forward_to(V_i, div_lhs_i,
                          flags=dr.ADFlag.ClearEdges | dr.ADFlag.ClearInterior)

            Z += Z_i
            dZ += dZ_i
            grad_V += dr.grad(V_i)
            grad_div_lhs += dr.grad(div_lhs_i)
            it += 1

        inv_Z = dr.rcp(dr.maximum(Z, 1e-8))
        V_theta  = grad_V * inv_Z
        div_V_theta = (grad_div_lhs - dr.dot(V_theta, dZ)) * inv_Z

        # Ignore inactive lanes
        V_theta = dr.select(self.active, V_theta, 0.0)
        div_V_theta = dr.select(self.active, div_V_theta, 0.0)

        self.set_grad_out((V_theta, div_V_theta))


    def backward_symbolic(self):
        grad_direction, grad_divergence = self.grad_out()

        # Ignore inactive lanes
        grad_direction  = dr.select(self.active, grad_direction, 0.0)
        grad_divergence = dr.select(self.active, grad_divergence, 0.0)
        ray_frame = mi.Frame3f(self.ray.d)
        rng = self.rng
        rng_clone = mi.PCG32(rng)

        with dr.suspend_grad():
            # We need to trace the auxiliary rays a first time to compute the
            # constants Z and dZ in order to properly weight the incoming gradients
            Z = mi.Float(0.0)
            dZ = mi.Vector3f(0.0)
            it = mi.UInt32(0)

            loop = mi.Loop(name="reparameterize_ray(): weight normalization",
                           state=lambda: (it, Z, dZ, rng_clone.state))

            # Unroll the entire loop in wavefront mode
            # loop.set_uniform(True) # TODO can we turn this back on? (see self.active in loop condition)
            loop.set_max_iterations(self.num_rays)
            loop.set_eval_stride(self.num_rays)

            while loop(self.active & (it < self.num_rays)):
                rng_state_backup = rng_clone.state

                sample = mi.Point2f(rng_clone.next_float32(),
                                    rng_clone.next_float32())

                if self.antithetic:
                    repeat = dr.eq(it & 1, 0)
                    rng_clone.state[repeat] = rng_state_backup
                else:
                    repeat = mi.Bool(False)

                Z_i, dZ_i, _, _ = _sample_warp_field(self.scene, sample,
                                                     self.ray, ray_frame,
                                                     repeat, self.kappa,
                                                     self.exponent)
                Z += Z_i
                dZ += dZ_i
                it += 1

        # Un-normalized values
        V = dr.zeros(mi.Vector3f, dr.width(Z))
        div_V_1 = dr.zeros(mi.Float, dr.width(Z))
        dr.enable_grad(V, div_V_1)

        # Compute normalized values
        Z = dr.maximum(Z, 1e-8)
        V_theta = V / Z
        divergence = (div_V_1 - dr.dot(V_theta, dZ)) / Z
        direction = dr.normalize(self.ray.d + V_theta)

        dr.set_grad(direction, grad_direction)
        dr.set_grad(divergence, grad_divergence)
        dr.enqueue(dr.ADMode.Backward, direction, divergence)
        dr.traverse(mi.Float, dr.ADMode.Backward)

        grad_V = dr.grad(V)
        grad_div_V_1 = dr.grad(div_V_1)

        it = mi.UInt32(0)
        ray_grad_o = mi.Point3f(0)
        ray_grad_d = mi.Vector3f(0)

        loop = mi.Loop(name="reparameterize_ray(): backpropagation",
                       state=lambda: (it, rng.state, ray_grad_o, ray_grad_d))

        # Unroll the entire loop in wavefront mode
        # loop.set_uniform(True) # TODO can we turn this back on? (see self.active in loop condition)
        loop.set_max_iterations(self.num_rays)
        loop.set_eval_stride(self.num_rays)

        while loop(self.active & (it < self.num_rays)):
            ray = mi.Ray3f(self.ray)
            dr.enable_grad(ray.o)
            dr.enable_grad(ray.d)
            ray_frame = mi.Frame3f(ray.d)

            rng_state_backup = rng.state

            sample = mi.Point2f(rng.next_float32(),
                                rng.next_float32())

            if self.antithetic:
                repeat = dr.eq(it & 1, 0)
                rng.state[repeat] = rng_state_backup
            else:
                repeat = mi.Bool(False)

            _, _, V_i, div_V_1_i = _sample_warp_field(self.scene, sample,
                                                      ray, ray_frame,
                                                      repeat, self.kappa,
                                                      self.exponent)

            dr.set_grad(V_i, grad_V)
            dr.set_grad(div_V_1_i, grad_div_V_1)
            dr.enqueue(dr.ADMode.Backward, V_i, div_V_1_i)
            dr.traverse(mi.Float, dr.ADMode.Backward, dr.ADFlag.ClearVertices)
            ray_grad_o += dr.grad(ray.o)
            ray_grad_d += dr.grad(ray.d)
            it += 1

        ray_grad = dr.detach(dr.zeros(type(self.ray)))
        ray_grad.o = ray_grad_o
        ray_grad.d = ray_grad_d
        self.set_grad_in('ray', ray_grad)


    def backward_unroll(self):
        assert not self.antithetic

        grad_direction, grad_divergence = self.grad_out()

        # Ignore inactive lanes
        grad_direction  = dr.select(self.active, grad_direction, 0.0)
        grad_divergence = dr.select(self.active, grad_divergence, 0.0)
        rng = self.rng
        rng_clone = mi.PCG32(rng)

        ray = mi.Ray3f(self.ray)
        dr.enable_grad(ray.o)
        dr.enable_grad(ray.d)
        ray_frame = mi.Frame3f(ray.d)

        warp_fields = []
        for i in range(self.num_rays):
            sample = mi.Point2f(rng_clone.next_float32(),
                                rng_clone.next_float32())
            warp_fields.append(_sample_warp_field(self.scene, sample,
                                                  ray, ray_frame,
                                                  mi.Bool(False), self.kappa,
                                                  self.exponent))

        Z = mi.Float(0.0)
        dZ = mi.Vector3f(0.0)
        for Z_i, dZ_i, _, _ in warp_fields:
            Z += Z_i
            dZ += dZ_i

        # Un-normalized values
        V = dr.zeros(mi.Vector3f, dr.width(Z))
        div_V_1 = dr.zeros(mi.Float, dr.width(Z))
        dr.enable_grad(V, div_V_1)

        # Compute normalized values
        Z = dr.maximum(Z, 1e-8)
        V_theta = V / Z
        divergence = (div_V_1 - dr.dot(V_theta, dZ)) / Z
        direction = dr.normalize(self.ray.d + V_theta)

        dr.set_grad(direction, grad_direction)
        dr.set_grad(divergence, grad_divergence)
        dr.enqueue(dr.ADMode.Backward, direction, divergence)
        dr.traverse(mi.Float, dr.ADMode.Backward)

        grad_V = dr.grad(V)
        grad_div_V_1 = dr.grad(div_V_1)

        for _, _, V_i, div_V_1_i in warp_fields:
            dr.set_grad(V_i, grad_V)
            dr.set_grad(div_V_1_i, grad_div_V_1)
            dr.enqueue(dr.ADMode.Backward, V_i, div_V_1_i)

        dr.traverse(mi.Float, dr.ADMode.Backward, dr.ADFlag.ClearVertices)

        ray_grad = dr.detach(dr.zeros(type(self.ray)))
        ray_grad.o = dr.grad(ray.o)
        ray_grad.d = dr.grad(ray.d)
        self.set_grad_in('ray', ray_grad)


    def backward(self):
        if self.unroll:
            self.backward_unroll()
        else:
            self.backward_symbolic()


    def name(self):
        return "reparameterize_ray()"


def reparameterize_ray(scene: mitsuba.Scene,
                       rng: mitsuba.PCG32,
                       params: mitsuba.SceneParameters,
                       ray: mitsuba.RayDifferential3f,
                       num_rays: int=4,
                       kappa: float=1e5,
                       exponent: float=3.0,
                       antithetic: bool=False,
                       unroll: bool=False,
                       active: mitsuba.Bool = True
) -> Tuple[mitsuba.Vector3f, mitsuba.Float]:
    """
    Reparameterize given ray by "attaching" the derivatives of its direction to
    moving geometry in the scene.

    Parameter ``scene`` (``mitsuba.Scene``):
        Scene containing all shapes.

    Parameter ``rng`` (``mitsuba.PCG32``):
        Random number generator used to sample auxiliary ray directions.

    Parameter ``params`` (``mitsuba.SceneParameters``):
        Scene parameters

    Parameter ``ray`` (``mitsuba.RayDifferential3f``):
        Ray to be reparameterized

    Parameter ``num_rays`` (``int``):
        Number of auxiliary rays to trace when performing the convolution.

    Parameter ``kappa`` (``float``):
        Kappa parameter of the von Mises Fisher distribution used to sample the
        auxiliary rays.

    Parameter ``exponent`` (``float``):
        Exponent used in the computation of the harmonic weights

    Parameter ``antithetic`` (``bool``):
        Should antithetic sampling be enabled to improve convergence?
        (Default: False)

    Parameter ``unroll`` (``bool``):
        Should the loop tracing auxiliary rays be unrolled? (Default: False)

    Parameter ``active`` (``mitsuba.Bool``):
        Boolean array specifying the active lanes

    Returns → (mitsuba.Vector3f, mitsuba.Float):
        Returns the reparameterized ray direction and the Jacobian
        determinant of the change of variables.
    """

    return dr.custom(_ReparameterizeOp, scene, rng, params, ray, num_rays,
                     kappa, exponent, antithetic, unroll, active)
