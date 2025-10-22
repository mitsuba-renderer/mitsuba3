from __future__ import annotations  # Delayed parsing of type annotations

import drjit as dr
import mitsuba as mi

from .common import RBIntegrator

class BasicVolumetricPrimitiveRadianceFieldIntegrator(RBIntegrator):
    r"""
    .. _integrator-volprim_rf_basic:

    Basic Volumetric Primitive Radiance Field Integrator (:monosp:`volprim_rf_basic`)
    ------------------------------------------------------------------------------

    .. pluginparameters::

     * - max_depth
       - |int|
       - Specifies the longest path depth in the generated output image (where -1
         corresponds to :math:`\infty`). (Default: 64)

     * - srgb_primitives
       - |bool|
       - Specifies whether the SH coefficients of the primitives are defined in
         sRGB color space. (Default: True)

    This plugin implements a simple radiance field integrator for ellipsoids shapes.

    .. tabs::

        .. code-tab:: python

            'type': 'volprim_rf_basic',
            'max_depth': 8
    """

    def __init__(self, props):
        super().__init__(props)

        max_depth = int(props.get("max_depth", 64))
        if max_depth < 0 and max_depth != -1:
            raise Exception('"max_depth" must be set to -1 (infinite) or a value >= 0')
        self.max_depth = mi.UInt32(max_depth if max_depth != -1 else 0xFFFFFFFF) # Map -1 (infinity) to 2^32-1 bounces

        self.srgb_primitives = props.get('srgb_primitives', True)

    def eval_transmission(self, si, ray, active):
        """
        Evaluate the transmission model on intersected volumetric primitives
        """
        def gather_ellipsoids_props(self, prim_index, active):
            if self is not None and self.is_ellipsoids():
                si = dr.zeros(mi.SurfaceInteraction3f)
                si.prim_index = prim_index
                data = self.eval_attribute_x("ellipsoid", si, active)
                center  = mi.Point3f([data[i] for i in range(3)])
                scale   = mi.Vector3f([data[i + 3] for i in range(3)])
                quat    = mi.Quaternion4f([data[i + 6] for i in range(4)])
                rot     = dr.quat_to_matrix(mi.Matrix3f, quat)
                return center, scale, rot
            else:
                return mi.Point3f(0), mi.Vector3f(0), mi.Matrix3f(0)

        center, scale, rot = dr.dispatch(si.shape, gather_ellipsoids_props, si.prim_index, active)

        opacity = si.shape.eval_attribute_1("opacities", si, active)

        # Gaussian splatting transmittance model
        # Find the peak location along the ray, from "3D Gaussian Ray Tracing"
        o = rot.T * (ray.o - center) / scale
        d = rot.T * ray.d / scale
        t_peak = -dr.dot(o, d) / dr.dot(d, d)
        p_peak = ray(t_peak)

        # Gaussian kernel evaluation
        p = rot.T * (p_peak - center)
        density = dr.exp(-0.5 * (p.x**2 / scale.x**2 + p.y**2 / scale.y**2 + p.z**2 / scale.z**2))

        return 1.0 - dr.minimum(opacity * density, 0.9999)

    def eval_sh_emission(self, si, ray, active):
        """
        Evaluate the SH directionally emission on intersected volumetric primitives
        """
        def eval(shape, si, ray, active):
            if shape is not None and shape.is_ellipsoids():
                sh_coeffs = shape.eval_attribute_x("sh_coeffs", si, active)
                sh_degree = int(dr.sqrt((sh_coeffs.shape[0] // 3) - 1))
                sh_dir_coef = dr.sh_eval(ray.d, sh_degree)
                emission = mi.Color3f(0.0)
                for i, sh in enumerate(sh_dir_coef):
                    emission += sh * mi.Color3f(
                        [sh_coeffs[i * 3 + j] for j in range(3)]
                    )
                return dr.maximum(emission + 0.5, 0.0)
            else:
                return mi.Color3f(0.0)

        return dr.dispatch(si.shape, eval, si, ray, active)

    @dr.syntax
    def sample(self, mode, scene, sampler, ray, δL, state_in, active, **kwargs):
        # --------------------- Configure integrator state ---------------------

        primal = mode == dr.ADMode.Primal
        ray = mi.Ray3f(dr.detach(ray))
        active = mi.Bool(active)
        depth = mi.UInt32(0)

        if not primal:  # If the gradient is zero, stop early
            active &= dr.any((δL != 0))

        L  = mi.Spectrum(0.0 if primal else state_in)  # Radiance accumulator
        δL = mi.Spectrum(δL if δL is not None else 0)  # Differential radiance
        β  = mi.Spectrum(1.0) # Path throughput weight

        # ----------------------------- Main loop ------------------------------

        while dr.hint(active, label=f"Primitive splatting ({mode.name})"):

            # --------------------- Find next intersection ---------------------

            si = scene.ray_intersect(
                ray,
                coherent=(depth == 0),
                ray_flags=mi.RayFlags.All,
                active=active,
            )

            active &= si.is_valid() & si.shape.is_ellipsoids()

            # ----------------- Primitive emission evaluation ------------------

            Le = mi.Spectrum(0.0)

            with dr.resume_grad(when=not primal):
                emission     = self.eval_sh_emission(si, ray, active)
                transmission = self.eval_transmission(si, ray, active)
                Le[active] = β * (1.0 - transmission) * emission
                Le[~dr.isfinite(Le)] = 0.0

            # ------- Update loop variables based on current interaction -------

            L[active] = (L + Le) if primal else (L - Le)
            β[active] *= transmission

            # Spawn new ray (don't use si.spawn_ray to avoid self intersections)
            ray.o[active] = si.p + ray.d * 1e-4

            # -------------- Differential phase only (PRB logic) ---------------

            with dr.resume_grad(when=not primal):
                if not primal:
                    # Differentiable reflected indirect radiance for primitives
                    Lr_ind = L * transmission / dr.detach(transmission)

                    # Differentiable Monte Carlo estimate of all contributions
                    Lo = Le + Lr_ind
                    Lo = dr.select(active & dr.isfinite(Lo), Lo, 0.0)

                    if mode == dr.ADMode.Backward:
                        dr.backward_from(δL * Lo)
                    else:
                        δL += dr.forward_to(Lo)

            # ----------------------- Stopping criterion -----------------------

            active &= si.is_valid()
            depth[active] += 1

            # Kill path if has insignificant contribution
            active &= dr.max(β) > 0.01

            # Don't estimate next recursion if we exceeded number of bounces
            active &= depth < self.max_depth

        # Convert sRGB light transport to linear color space
        if self.srgb_primitives:
            L = mi.math.srgb_to_linear(L)

        return L if primal else δL, True, [], L

    def to_string(self):
        return f"BasicVolumetricPrimitiveRadianceFieldIntegrator[]"

mi.register_integrator("volprim_rf_basic", lambda props: BasicVolumetricPrimitiveRadianceFieldIntegrator(props))

del RBIntegrator
