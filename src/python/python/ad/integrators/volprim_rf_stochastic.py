# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

from __future__ import annotations  # Delayed parsing of type annotations

import drjit as dr
import mitsuba as mi
from mitsuba.ad.integrators.common import ADIntegrator

class StochasticRadianceFieldIntegrator(ADIntegrator):
    """
    Stochastic radiance field integrator for ellipsoids shapes.
    """

    def __init__(self, props=mi.Properties()):
        super().__init__(props)

        # Maximum degree of spherical harmonic
        self.max_sh_degree = int(props.get("max_sh_degree", 0xFFFFFFFF))

        # Color space in which the Radiance Field is stored
        self.color_space = props.get("color_space", "srgb")
        if self.color_space not in ["srgb", "linear"]:
            raise Exception(
                f"Invalid color space: {self.color_space}, should be either 'srgb', 'linear'"
            )

    def traverse(self, cb):
        cb.put("color_space", self.color_space, mi.ParamFlags.NonDifferentiable)

    def parameters_changed(self, keys):
        pass

    def eval_sh_emission(self, si, ray, active):
        """
        Evaluate the SH directionally emission on intersected volumetric primitives
        """

        def eval(shape, si, ray, active):
            if shape is not None and shape.is_ellipsoids():
                sh_coeffs = shape.eval_attribute_x("sh_coeffs", si, active)
                sh_degree = int(dr.sqrt((sh_coeffs.shape[0] // 3) - 1))
                sh_degree = min(sh_degree, self.max_sh_degree)
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

    def sample(self, mode, scene, sampler, ray, δL, state_in, active, **kwargs):
        # --------------------- Configure integrator state ---------------------

        ray = mi.Ray3f(dr.detach(ray))
        active = mi.Bool(active)
        env_emitter = scene.environment()

        # -------------------- Find stochastic intersection --------------------

        ray_flags = (mi.RayFlags.StochasticEllipsoids | mi.RayFlags.GaussianEllipsoids)

        si = scene.ray_intersect(ray, coherent=True, ray_flags=ray_flags, active=active)

        Le_srgb = mi.Spectrum(0.0)
        Le_linear = mi.Spectrum(0.0)

        # ------------------- Direct emission evaluation -------------------

        if env_emitter is not None:
            env_value = env_emitter.eval(si, active & ~si.is_valid())
            Le_linear[active & ~si.is_valid()] = env_value

        # -------------- Volumetric primitive emission evaluation --------------

        active_p = active & si.is_valid() & (si.shape.is_ellipsoids())

        emission = self.eval_sh_emission(si, ray, active_p)

        if dr.hint(self.color_space == "srgb", mode="scalar"):
            Le_srgb[active_p] = emission
        else:
            Le_linear[active_p] = emission

        Le_srgb[~dr.isfinite(Le_srgb)] = 0.0
        Le_linear[~dr.isfinite(Le_linear)] = 0.0

        return Le_linear, active_p, [*Le_srgb], None

    def render(
        self: mi.SamplingIntegrator,
        scene: mi.Scene,
        sensor: Union[int, mi.Sensor] = 0,
        seed: mi.UInt32 = 0,
        spp: int = 0,
        develop: bool = True,
        evaluate: bool = True,
    ) -> mi.TensorXf:
        if not develop:
            raise Exception(
                "develop=True must be specified when invoking AD integrators"
            )

        if isinstance(sensor, int):
            sensor = scene.sensors()[sensor]

        film = sensor.film()

        # Disable derivatives in all of the following
        with dr.suspend_grad():
            # Prepare the film and sample generator for rendering
            sampler, spp = self.prepare(
                sensor=sensor, seed=seed, spp=spp, aovs=self.aov_names(), active=True
            )

            # Generate a set of rays starting at the sensor
            ray, weight, pos = self.sample_rays(scene, sensor, sampler)

            # This integrator returns separate sRGB and linear pixel values
            L_linear, valid, aovs, _ = self.sample(
                mode=dr.ADMode.Primal,
                scene=scene,
                sampler=sampler,
                ray=ray,
                depth=mi.UInt32(0),
                δL=None,
                δaovs=None,
                state_in=None,
                active=mi.Bool(True),
            )
            alpha = dr.select(valid, 1.0, 0.0)

            L_srgb = None
            if self.color_space == "srgb":
                L_srgb = mi.Spectrum(aovs)

            dr.eval(L_linear, L_srgb, alpha)

            def develop_image(L, alpha):
                film.clear()
                block = film.create_block()
                block.set_coalesce(block.coalesce() and spp >= 4)
                ADIntegrator._splat_to_block(
                    block,
                    film,
                    pos,
                    value=L * weight,
                    weight=1.0,
                    alpha=alpha,
                    aovs=[],
                    wavelengths=ray.wavelengths,
                )

                film.put_block(block)

                return film.develop()

            if self.color_space == "linear":
                return develop_image(L_linear, alpha)

            img_srgb = develop_image(L_srgb, alpha)
            img_linear = develop_image(L_linear, 0.0)

            def tensor_srgb_to_linear(img):
                w, h, c = img.shape
                if c == 3:
                    fgd_color = mi.math.srgb_to_linear(
                        dr.unravel(mi.Color3f, img.array)
                    )
                    return mi.TensorXf(dr.ravel(fgd_color), shape=(w, h, c))
                elif c == 4:
                    fgd_color = mi.math.srgb_to_linear(
                        dr.unravel(mi.Color3f, img[:, :, :3].array)
                    )
                    img[:, :, :3] = dr.ravel(fgd_color)
                    return img
                else:
                    raise Exception(f"Channel count not supported: {c}")

            return img_linear + tensor_srgb_to_linear(img_srgb)

    def to_string(self):
        return f"StochasticRadianceFieldIntegrator[]"

mi.register_integrator("volprim_rf_stochastic", lambda props: StochasticRadianceFieldIntegrator(props))
