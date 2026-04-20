# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# This code is derived from Blender Cycles and was developed at Meta.

from __future__ import annotations  # Delayed parsing of type annotations

from typing import Tuple

import drjit as dr
import mitsuba as mi

from .common import *
from .lobes import *


class dotdict(dict):
    """
    dot.notation access to dictionary attributes
    """

    __getattr__ = dict.get
    __setattr__ = dict.__setitem__
    __delattr__ = dict.__delitem__


class BlenderPrincipledBSDF(mi.BSDF):
    """
    Principled BSDF that matches Blender Cycles 4.5.

    Parameters:
        - base_color (Texture): The base color texture
        - roughness (Texture): The roughness texture
        - anisotropic (Texture): The anisotropic texture
        - anisotropic_rot (Texture): The anisotropic rotation texture in [0, 1]
        - eta (float): Main index of refraction
        - transmission (Texture): The specular transmission texture
        - sheen (Texture): The sheen texture
        - sheen_tint (Texture): The sheen tint texture
        - sheen_roughness (Texture): The sheen roughness texture
        - spec_ior_level (Texture): The specular IOR level texture to adjust specularity. 0.0 removes all reflections, 1.0 doubles the reflections.
        - spec_tint (Texture): The specular tint texture
        - metallic (Texture): The metallic texture
        - clearcoat (Texture): The clearcoat texture
        - clearcoat_roughness (Texture): The clearcoat gloss texture
        - clearcoat_ior (Texture): The clearcoat IOR texture
        - clearcoat_tint (Texture): The clearcoat tint texture
        - clearcoat_normalmap (Texture): Clearcoat normalmap texture (default: null)
        - normalmap (Texture): Base layer normal map (default: null)
        - alpha (Texture): Opacity texture (default: 1.0)
    """

    def __init__(self, props):
        mi.BSDF.__init__(self, props)

        # Parameter definitions
        self.two_sided = props.get("two_sided", False)
        self.base_color = props.get_texture("base_color", 0.5)
        self.diffuse_roughness = props.get_texture("diffuse_roughness", 0.0)
        self.roughness = props.get_texture("roughness", 0.5)
        self.has_specular = True
        self.spec_ior_level = props.get_texture("spec_ior_level", 0.5)
        self.has_anisotropic = is_active(props, "anisotropic")
        self.anisotropic = props.get_texture("anisotropic", 0.0)
        self.anisotropic_rot = props.get_texture("anisotropic_rot", 0.0)
        self.has_transmission = is_active(props, "transmission")
        self.transmission = props.get_texture("transmission", 0.0)
        self.has_sheen = is_active(props, "sheen")
        self.sheen = props.get_texture("sheen", 0.0)
        self.has_sheen_tint = is_active(props, "sheen_tint")
        self.sheen_tint = props.get_texture("sheen_tint", 0.0)
        self.sheen_roughness = props.get_texture("sheen_roughness", 0.5)
        self.has_spec_tint = is_active(props, "spec_tint")
        self.spec_tint = props.get_texture("spec_tint", 0.0)
        self.has_metallic = is_active(props, "metallic")
        self.metallic = props.get_texture("metallic", 0.0)

        self.has_clearcoat = is_active(props, "clearcoat")
        self.clearcoat = props.get_texture("clearcoat", 0.0)
        self.clearcoat_roughness = props.get_texture("clearcoat_roughness", 0.0)
        self.clearcoat_ior = props.get_texture("clearcoat_ior", 1.5)
        self.clearcoat_tint = props.get_texture("clearcoat_tint", 1.0)
        self.has_clearcoat_normalmap = is_active(props, "clearcoat_normalmap")
        self.clearcoat_normalmap = props.get_texture("clearcoat_normalmap", 0.0)

        assert not (
            self.has_transmission and self.two_sided
        ), "Only materials without a specular transmission lobe can be two sided."

        self.has_normalmap = is_active(props, "normalmap")
        self.normalmap = props.get_texture("normalmap", 0.0)

        self.has_alpha = is_active(props, "alpha")
        self.alpha = props.get_texture("alpha", 1.0)

        # Eta and specular has one to one correspondence, both of them can not be specified.
        assert not ("eta" in props.keys() and "specular" in props.keys())

        eta = props.get("eta", 1.5)
        # self.eta = 1 is not plausible for transmission
        if self.has_transmission and (eta == 1.0):
            eta = 1.001
        self.eta = mi.Float(eta)

        self._initialize_lobes()

        # Ensure pre-computed tables are fetched outside of rendering kernel
        if self.has_clearcoat or self.has_specular or self.has_sheen:
            fetch_table("ggx_gen_schlick_ior_s")

    def traverse(self, cb):
        F = mi.ParamFlags
        cb.put("base_color", self.base_color, F.Differentiable)
        cb.put("metallic", self.metallic, F.Differentiable)
        cb.put("clearcoat", self.clearcoat, F.Differentiable)
        cb.put("diffuse_roughness", self.diffuse_roughness, F.Differentiable)
        cb.put("clearcoat_roughness", self.clearcoat_roughness, F.Differentiable)
        cb.put("clearcoat_ior", self.clearcoat_ior, F.Differentiable)
        cb.put("clearcoat_tint", self.clearcoat_tint, F.Differentiable)
        cb.put("roughness", self.roughness, F.Differentiable | F.Discontinuous)
        cb.put("anisotropic", self.anisotropic, F.Differentiable)
        cb.put("anisotropic_rot", self.anisotropic_rot, F.Differentiable)
        cb.put("spec_tint", self.spec_tint, F.Differentiable)
        cb.put("spec_ior_level", self.spec_ior_level, F.Differentiable)
        cb.put("sheen", self.sheen, F.Differentiable)
        cb.put("sheen_tint", self.sheen_tint, F.Differentiable)
        cb.put("sheen_roughness", self.sheen_roughness, F.Differentiable)
        cb.put("transmission", self.transmission, F.Differentiable)
        cb.put("normalmap", self.normalmap, F.Differentiable)
        cb.put("eta", self.eta, F.Differentiable | F.Discontinuous)
        cb.put("alpha", self.alpha, F.Differentiable)

    def parameters_changed(self, keys):
        # Update the self.has_* attributes based on keys
        for k in keys:
            name = f"has_{k}"
            if hasattr(self, name):
                setattr(self, name, True)

        if "eta" in keys:
            # Eta = 1 is not plausible for transmission.
            self.eta[self.eta == 1.0] = 1.001

        self._initialize_lobes()

    def _initialize_lobes(self):
        F = mi.BSDFFlags

        self.components_mapping = dotdict()
        components = []

        # Diffuse reflection lobe
        components.append(F.DiffuseReflection | F.FrontSide)
        self.components_mapping.diffuse = len(components) - 1

        # Clearcoat lobe
        if self.has_clearcoat:
            components.append(F.GlossyReflection | F.FrontSide)
            self.components_mapping.clearcoat = len(components) - 1

        # Specular transmission lobe
        if self.has_transmission:
            f = (
                F.GlossyReflection
                | F.GlossyTransmission
                | F.FrontSide
                | F.BackSide
                | F.NonSymmetric
            )
            if self.has_anisotropic:
                f = f | F.Anisotropic
            components.append(f)
            self.components_mapping.trans_reflect = len(components) - 1
            self.components_mapping.trans_refract = len(components) - 1

        # Main specular reflection lobe
        if self.has_specular:
            f = F.GlossyReflection | F.FrontSide | F.BackSide
            if self.has_anisotropic:
                f = f | F.Anisotropic
            components.append(f)
            self.components_mapping.specular = len(components) - 1

        if self.two_sided:
            for i in range(len(components)):
                components[i] |= F.FrontSide | F.BackSide

        if self.has_alpha:
            f = F.Null | F.FrontSide | F.BackSide
            components.append(f)
            self.components_mapping.null = len(components) - 1

        flags = 0
        for c in components:
            flags |= c

        self.m_components = components
        self.m_flags = flags

        dr.make_opaque(self.eta)

    def fetch_attributes(
        self, si: mi.SurfaceInteraction3f, active: mi.Bool
    ) -> dotdict:
        """
        Fetch BSDF attributes for the current surface interaction
        """
        attr = dotdict()
        attr.diffuse_roughness = self.diffuse_roughness.eval_1(si, active)
        attr.diffuse = 1.0
        attr.base_color = self.base_color.eval(si, active)
        attr.sheen = self.sheen.eval_1(si, active) if self.has_sheen else mi.Float(0.0)
        attr.sheen_tint = (
            self.sheen_tint.eval(si, active)
            if self.has_sheen_tint
            else mi.UnpolarizedSpectrum(0.0)
        )
        attr.sheen_roughness = (
            self.sheen_roughness.eval_1(si, active) if self.has_sheen else mi.Float(0.0)
        )
        attr.anisotropic = (
            self.anisotropic.eval_1(si, active)
            if self.has_anisotropic
            else mi.Float(0.0)
        )
        attr.anisotropic_rot = (
            self.anisotropic_rot.eval_1(si, active)
            if self.has_anisotropic
            else mi.Float(0.0)
        )
        attr.roughness = self.roughness.eval_1(si, active)
        attr.transmission = (
            self.transmission.eval_1(si, active)
            if self.has_transmission
            else mi.Float(0.0)
        )
        attr.spec_tint = (
            self.spec_tint.eval(si, active)
            if self.has_spec_tint
            else mi.UnpolarizedSpectrum(0.0)
        )
        attr.spec_ior_level = self.spec_ior_level.eval_1(si, active)
        attr.metallic = (
            self.metallic.eval_1(si, active) if self.has_metallic else mi.Float(0.0)
        )
        attr.clearcoat = (
            self.clearcoat.eval_1(si, active) if self.has_clearcoat else mi.Float(0.0)
        )
        attr.clearcoat_roughness = self.clearcoat_roughness.eval_1(si, active)
        attr.clearcoat_ior = self.clearcoat_ior.eval_1(si, active)
        attr.clearcoat_tint = self.clearcoat_tint.eval(si, active)
        attr.clearcoat_normal = (
            self.clearcoat_normalmap.eval_3(si, active)
            if self.has_clearcoat_normalmap
            else mi.Normal3f(0.5, 0.5, 1.0)
        )
        attr.normal = (
            mi.Normal3f(self.normalmap.eval_3(si, active))
            if self.has_normalmap
            else mi.Normal3f(0.5, 0.5, 1.0)
        )
        attr.eta = sanitize_eta(self.eta)
        attr.two_sided = mi.Bool(self.two_sided)
        attr.alpha = (
            dr.clip(self.alpha.eval_1(si, active), 0.0, 1.0)
            if self.has_alpha
            else mi.Float(1.0)
        )
        return attr

    def _lobe_weights(
        self,
        attr: dotdict,
        ctx: mi.BSDFContext,
        geo_wi: mi.Vector3f,
        sh_wi: mi.Vector3f,
        wh: mi.Vector3f,
        cc_wi: mi.Vector3f,
    ) -> Tuple[dotdict, dotdict, dotdict]:
        """
        Calculate lobe weights, sampling weights and masks
        """
        weights = dotdict()
        weights.diffuse = mi.UnpolarizedSpectrum(attr.diffuse)
        weights.sheen = mi.UnpolarizedSpectrum(attr.sheen)
        weights.clearcoat = mi.UnpolarizedSpectrum(attr.clearcoat)
        weights.metallic = mi.UnpolarizedSpectrum(attr.metallic)
        weights.specular = mi.UnpolarizedSpectrum(1.0)
        weights.trans_reflect = mi.UnpolarizedSpectrum(attr.transmission)
        weights.trans_refract = mi.UnpolarizedSpectrum(attr.transmission)

        # Only lobes with non-zero weights are considered as active
        masks = dotdict({k: (dr.mean(v) > 0.0) for k, v in weights.items()})

        # Masks lobes that only affects rays on the front side
        front_side = is_front_side(sh_wi)
        masks.diffuse &= front_side
        masks.sheen &= front_side
        masks.clearcoat &= front_side
        masks.metallic &= front_side
        masks.specular &= front_side

        # Fresnel coefficient for the main specular.
        F_spec_dielectric = mi.fresnel(dr.dot(sh_wi, wh), attr.eta)[0]
        masks.specular &= F_spec_dielectric > 0.0

        # Masks lobes based on flags enabled in the BSDF context
        masks.diffuse &= ctx.is_enabled(+mi.BSDFFlags.DiffuseReflection)
        masks.sheen &= ctx.is_enabled(+mi.BSDFFlags.GlossyReflection)
        masks.clearcoat &= ctx.is_enabled(+mi.BSDFFlags.GlossyReflection)
        masks.metallic &= ctx.is_enabled(+mi.BSDFFlags.GlossyReflection)
        masks.specular &= ctx.is_enabled(+mi.BSDFFlags.GlossyReflection)
        masks.trans_reflect &= ctx.is_enabled(+mi.BSDFFlags.GlossyReflection)
        masks.trans_refract &= ctx.is_enabled(+mi.BSDFFlags.GlossyTransmission)

        if self.has_alpha:
            for k, v in masks.items():
                masks[k] &= ctx.component != mi.UInt32(self.components_mapping.null)
        masks.null = mi.Bool(ctx.is_enabled(+mi.BSDFFlags.Null))

        # Zero-out weights for lobes that are not active
        for k, v in weights.items():
            weights[k] = dr.select(masks[k], weights[k], 0.0)

        # Lobe albedos
        albedos = dotdict()

        # Compute lobe attenuation coefficients
        attenuation = mi.Float(1.0)

        # Ensure attenuation factor is in [0, 1]
        sanitize = lambda x: dr.select(dr.isfinite(x), dr.clip(x, 0, 1), 0)
        layering = lambda albedo, weight, attenuation: sanitize(
            attenuation * (1.0 - dr.max(albedo * weight / attenuation))
        )

        # Sheen lobe
        if self.has_sheen:
            albedos.sheen = (
                microfacet_estimate_albedo(
                    MicrofacetFresnel.NONE,
                    sh_wi,
                    roughness=attr.sheen_roughness,
                    r0=0.0,
                    eta=attr.eta,
                    reflection=True,
                    transmission=False,
                )
                * attr.sheen_tint
            )
            attenuation = layering(albedos.sheen, weights.sheen, attenuation)

        # Clearcoat lobe
        if self.has_clearcoat:
            weights.clearcoat *= attenuation
            albedos.clearcoat = microfacet_estimate_albedo(
                MicrofacetFresnel.DIELECTRIC,
                cc_wi,
                attr.clearcoat_roughness,
                r0=None,
                eta=attr.clearcoat_ior,
                reflection=True,
                transmission=False,
            )
            attenuation = layering(albedos.clearcoat, weights.clearcoat, attenuation)

            # Clearcoat tint models absorption in the layer
            cc_cos_theta_i = mi.Frame3f.cos_theta(cc_wi)
            optical_depth = 1.0 / dr.sqrt(
                1.0
                - dr.square(1.0 / attr.clearcoat_ior) * (1 - dr.square(cc_cos_theta_i))
            )
            cc_tint = dr.power(attr.clearcoat_tint, optical_depth)
            attenuation *= dr.lerp(1.0, cc_tint, attr.clearcoat)

        if self.has_metallic:
            weights.metallic *= attenuation
            attenuation = sanitize(attenuation * (1.0 - attr.metallic))

        # Transmission lobe
        if self.has_transmission:
            weights.trans_reflect *= attenuation
            weights.trans_refract *= attenuation
            albedos.trans_reflect = mi.UnpolarizedSpectrum(F_spec_dielectric)
            albedos.trans_refract = mi.UnpolarizedSpectrum(
                (1.0 - F_spec_dielectric) * dr.sqrt(attr.base_color)
            )
            attenuation = sanitize(attenuation * (1.0 - attr.transmission))

        # Specular lobe
        if self.has_specular:
            spec_r0 = eta_to_r0(attr.eta) * 2.0 * attr.spec_ior_level
            spec_eta = r0_to_eta(spec_r0)
            spec_eta[attr.eta < 1.0] = 1.0 / spec_eta
            spec_r0 *= attr.spec_tint

            weights.specular *= attenuation
            weights.specular[spec_eta == 1.0] = mi.UnpolarizedSpectrum(0.0)

            albedos.specular = microfacet_estimate_albedo(
                MicrofacetFresnel.GENERALIZED_SCHLICK,
                sh_wi,
                attr.roughness,
                r0=spec_r0,
                eta=spec_eta,
                reflection=True,
                transmission=False,
            )
            albedos.specular[attr.spec_ior_level == 0.0] = 0.0
            attenuation = layering(albedos.specular, weights.specular, attenuation)

        # Diffuse lobe
        weights.diffuse *= attenuation
        albedos.diffuse = mi.UnpolarizedSpectrum(attr.base_color)

        # Negative weights not allowed
        for k, v in weights.items():
            weights[k] = dr.maximum(v, 0.0)

        # Compute lobe sampling weights
        sampling_weights = dotdict()
        for k, v in weights.items():
            sampling_weights[k] = dr.mean(weights[k])

        # Account for lobe albedos in sampling weights
        for k, v in albedos.items():
            sampling_weights[k] *= dr.mean(albedos[k])

        # Normalize and detach the sampling weights
        normalization = dr.rcp(sum(sampling_weights.values()))
        for k, v in sampling_weights.items():
            sampling_weights[k] = dr.detach(v * normalization)

        if self.has_alpha:
            for k, v in sampling_weights.items():
                sampling_weights[k] *= attr.alpha
            sampling_weights.null = 1.0 - attr.alpha

        return weights, sampling_weights, masks

    def _eval_pdf_impl(self, attr, ctx, si, wo_, active):
        # Two-sided
        wo_ = mi.Vector3f(wo_)
        si = mi.SurfaceInteraction3f(si)
        wo_.z[attr.two_sided] = dr.mulsign(wo_.z, si.wi.z)
        si.wi.z[attr.two_sided] = dr.abs(si.wi.z)

        # Apply normalmap
        frame = compute_normalmap_frame(si, attr.normal, rot=attr.anisotropic_rot)
        wi = frame.to_local(si.wi)
        wo = frame.to_local(wo_)

        # Compute half vector
        wh = half_vector(wi, wo, attr.eta)

        # Apply normalmap for clearcoat lobe
        cc_frame = compute_normalmap_frame(si, normal=attr.clearcoat_normal)
        cc_wi = cc_frame.to_local(si.wi)

        # Compute lobe weights and sample weights
        weights, sampling_weights, masks = self._lobe_weights(
            attr, ctx, si.wi, wi, wh, cc_wi
        )

        # Shading and geometric horizon validity flags
        reflect_geom = is_reflection(si.wi, wo_)
        refract_geom = is_refraction(si.wi, wo_)
        reflect_shading = is_reflection(wi, wo)
        refract_shading = is_refraction(wi, wo)

        masks.diffuse &= reflect_geom & reflect_shading
        masks.clearcoat &= reflect_geom & reflect_shading
        masks.sheen &= reflect_geom & reflect_shading
        masks.metallic &= reflect_geom & reflect_shading
        masks.specular &= reflect_geom & reflect_shading
        masks.trans_reflect &= reflect_geom & reflect_shading
        masks.trans_refract &= refract_geom & refract_shading

        # Initializing the output PDF and BSDF values
        pdf = mi.Float(0.0)
        value = mi.Spectrum(0.0)

        # Sheen evaluation
        if self.has_sheen:
            sheen_value = SheenLobe.eval(wi, wo, attr.sheen_roughness, attr.anisotropic)
            value[masks.sheen] += mi.Spectrum(weights.sheen) * mi.Spectrum(sheen_value)

        # Clearcoat lobe
        if self.has_clearcoat:
            cc_value, cc_pdf = MicrofacetLobe.eval_pdf(
                wi,
                wo,
                reflection=True,
                roughness=attr.clearcoat_roughness,
                fresnel_mode=MicrofacetFresnel.DIELECTRIC,
                eta=attr.clearcoat_ior,
                anisotropic=0.0,
                correlated_shadow_masking=True,
            )

            value[masks.clearcoat] += mi.Spectrum(weights.clearcoat) * mi.Spectrum(
                cc_value
            )
            pdf[masks.clearcoat] += sampling_weights.clearcoat * cc_pdf

        # Metallic component based on Schlick
        if self.has_metallic:
            metal_value, metal_pdf = MicrofacetLobe.eval_pdf(
                wi,
                wo,
                reflection=True,
                roughness=attr.roughness,
                anisotropic=attr.anisotropic,
                fresnel_mode=MicrofacetFresnel.APPROXIMATED_SCHLICK,
                r0=attr.base_color,
                eta=attr.eta,
                correlated_shadow_masking=False,
            )

            value[masks.metallic] += mi.Spectrum(weights.metallic) * mi.Spectrum(
                metal_value
            )
            pdf[masks.metallic] += sampling_weights.metallic * metal_pdf

        # Glass lobe
        if self.has_transmission:
            reflect_value, reflect_pdf = MicrofacetLobe.eval_pdf(
                wi,
                wo,
                reflection=True,
                roughness=attr.roughness,
                anisotropic=attr.anisotropic,
                fresnel_mode=MicrofacetFresnel.GENERALIZED_SCHLICK,
                eta=attr.eta,
                r0=eta_to_r0(attr.eta) * attr.spec_tint,
                correlated_shadow_masking=True,
            )
            value[masks.trans_reflect] += mi.Spectrum(
                weights.trans_reflect
            ) * mi.Spectrum(reflect_value)
            pdf[masks.trans_reflect] += sampling_weights.trans_reflect * reflect_pdf

            refract_value, refract_pdf = MicrofacetLobe.eval_pdf(
                wi,
                wo,
                reflection=False,
                roughness=attr.roughness,
                anisotropic=attr.anisotropic,
                fresnel_mode=MicrofacetFresnel.GENERALIZED_SCHLICK,
                eta=attr.eta,
                r0=eta_to_r0(attr.eta) * attr.spec_tint,
                correlated_shadow_masking=True,
            )
            value[masks.trans_refract] += (
                mi.Spectrum(weights.trans_refract)
                * mi.Spectrum(refract_value)
                * mi.Spectrum(dr.sqrt(attr.base_color))
            )
            pdf[masks.trans_refract] += sampling_weights.trans_refract * refract_pdf

        # Specular reflection lobe
        if self.has_specular:
            spec_r0 = eta_to_r0(attr.eta) * 2.0 * attr.spec_ior_level
            spec_eta = r0_to_eta(spec_r0)
            spec_eta[attr.eta < 1.0] = 1.0 / spec_eta
            spec_r0 *= attr.spec_tint

            spec_value, spec_pdf = MicrofacetLobe.eval_pdf(
                wi,
                wo,
                reflection=True,
                roughness=attr.roughness,
                anisotropic=attr.anisotropic,
                fresnel_mode=MicrofacetFresnel.GENERALIZED_SCHLICK,
                r0=spec_r0,
                eta=spec_eta,
            )

            value[masks.specular] += mi.Spectrum(weights.specular) * mi.Spectrum(
                spec_value
            )
            pdf[masks.specular] += sampling_weights.specular * spec_pdf

        if True:
            # Adding diffuse lobe
            diffuse_value, diffuse_pdf = OrenNayarLobe.eval_pdf(
                wi, wo, attr.base_color, attr.diffuse_roughness
            )
            value[masks.diffuse] += (
                mi.Spectrum(diffuse_value)
                * mi.Spectrum(weights.diffuse)
                * mi.Spectrum(attr.base_color)
            )
            pdf[masks.diffuse] += sampling_weights.diffuse * diffuse_pdf

        if self.has_alpha:
            value *= mi.Spectrum(attr.alpha)
            # Lobe's pdfs are already multiplied by alpha in the sampling weights

        return value, dr.detach(pdf)

    def _sample_impl(self, attr, ctx, si, sample1, sample2, active):
        # Avoid modifying caller's state
        active = mi.Bool(active)

        null_wi = mi.Vector3f(si.wi)

        # Two-sided
        si = mi.SurfaceInteraction3f(si)
        si.wi.z[attr.two_sided] = dr.abs(si.wi.z)

        # Apply normalmap
        frame = compute_normalmap_frame(si, attr.normal, rot=attr.anisotropic_rot)
        wi = frame.to_local(si.wi)

        # Sample main specular and transmission reflection distribution
        wh = MicrofacetLobe.sample_wh(wi, sample2, attr.roughness, attr.anisotropic)

        # Apply normalmap for clearcoat lobe
        cc_frame = compute_normalmap_frame(si, normal=attr.clearcoat_normal)
        cc_wi = cc_frame.to_local(si.wi)

        # Compute lobe weights and sample weights
        weights, sampling_weights, _ = self._lobe_weights(
            attr, ctx, si.wi, wi, wh, cc_wi
        )

        # Pick lobe based on weights
        masks = dotdict()
        masks.diffuse = mi.Bool(active)
        masks.clearcoat = mi.Bool(active) & self.has_clearcoat
        masks.metallic = mi.Bool(active)
        masks.specular = mi.Bool(active)
        masks.trans_reflect = mi.Bool(active) & self.has_transmission
        masks.trans_refract = mi.Bool(active) & self.has_transmission
        masks.sheen = mi.Bool(active) & self.has_sheen
        masks.null = mi.Bool(active) & self.has_alpha

        cum = mi.Float(0.0)
        for k, w in sampling_weights.items():
            masks[k] &= (sample1 >= cum) & (sample1 < cum + w)
            cum += w

        # BSDF sampling data-structure to fill in
        bs = dr.zeros(mi.BSDFSample3f)

        # Diffuse lobe sampling
        if True:
            b = dr.zeros(mi.BSDFSample3f)
            b.wo = frame.to_world(OrenNayarLobe.sample(sample2))
            b.sampled_component = self.components_mapping.diffuse
            b.sampled_type = +mi.BSDFFlags.DiffuseReflection
            b.eta = 1.0
            bs[masks.diffuse & is_reflection(si.wi, b.wo)] = b

        # Clearcoat reflection sampling
        if self.has_clearcoat:
            cc_wh = MicrofacetLobe.sample_wh(cc_wi, sample2, attr.clearcoat_roughness)
            b = dr.zeros(mi.BSDFSample3f)
            b.wo = frame.to_world(microfacet_reflect(cc_wi, cc_wh))
            b.sampled_component = self.components_mapping.clearcoat
            b.sampled_type = +mi.BSDFFlags.GlossyReflection
            b.eta = 1.0
            bs[masks.clearcoat & is_reflection(si.wi, b.wo)] = b

        # Reflection
        if self.has_specular:
            b = dr.zeros(mi.BSDFSample3f)
            b.wo = frame.to_world(microfacet_reflect(wi, wh))
            b.sampled_component = self.components_mapping.specular
            b.sampled_type = +mi.BSDFFlags.GlossyReflection
            b.eta = 1.0
            valid = is_reflection(si.wi, b.wo)
            bs[(masks.specular | masks.metallic | masks.sheen) & valid] = b

        # Transmission
        if self.has_transmission:
            # Reflection
            b = dr.zeros(mi.BSDFSample3f)
            b.wo = frame.to_world(microfacet_reflect(wi, wh))
            b.sampled_component = self.components_mapping.trans_reflect
            b.sampled_type = +mi.BSDFFlags.GlossyReflection
            b.eta = 1.0
            bs[masks.trans_reflect] = b

            # Refraction
            b = dr.zeros(mi.BSDFSample3f)
            b.wo = frame.to_world(microfacet_refract(wi, wh, attr.eta))
            b.sampled_component = self.components_mapping.trans_refract
            b.sampled_type = +mi.BSDFFlags.GlossyTransmission
            b.eta = dr.select(is_front_side(wi), attr.eta, dr.rcp(attr.eta))
            bs[masks.trans_refract & is_refraction(si.wi, b.wo)] = b

        if self.has_alpha:
            b = dr.zeros(mi.BSDFSample3f)
            b.wo = -null_wi
            b.sampled_component = self.components_mapping.null
            b.sampled_type = +mi.BSDFFlags.Null
            b.eta = 1.0
            bs[masks.null] = b

        # Compute corresponding PDF and BSDF value
        value, bs.pdf = self._eval_pdf_impl(attr, ctx, si, bs.wo, active)

        if self.has_alpha:
            value[masks.null] = 1.0 - attr.alpha
            bs.pdf[masks.null] = dr.detach(1.0 - attr.alpha)

        active &= bs.pdf > 0.0

        # Compute sampling weight
        weight = dr.select(active, value / bs.pdf, 0.0)

        # Two-sided
        bs.wo.z[attr.two_sided & ~masks.null] = dr.mulsign(bs.wo.z, si.wi.z)

        return bs, weight

    def eval(self, ctx, si, wo, active=True):
        attr = self.fetch_attributes(si, active)
        return self._eval_pdf_impl(attr, ctx, si, wo, active)[0]

    def pdf(self, ctx, si, wo, active=True):
        attr = self.fetch_attributes(si, active)
        return self._eval_pdf_impl(attr, ctx, si, wo, active)[1]

    def eval_pdf(self, ctx, si, wo, active=True):
        attr = self.fetch_attributes(si, active)
        return self._eval_pdf_impl(attr, ctx, si, wo, active)

    def sample(self, ctx, si, sample1, sample2, active):
        attr = self.fetch_attributes(si, active)
        return self._sample_impl(attr, ctx, si, sample1, sample2, active)

    def eval_null_transmission(self, si, active):
        if self.has_alpha:
            return 1.0 - dr.clip(self.alpha.eval_1(si, active), 0.0, 1.0)
        else:
            return 0.0

    def to_string(self):
        return (
            "Principled BSDF[\n"
            "    two_sided=%s,\n"
            "    base_color=%s,\n"
            "    transmission=%s,\n"
            "    roughness=%s,\n"
            "    anisotropic=%s,\n"
            "    eta=%s,\n"
            "    sheen=%s,\n"
            "    sheen_tint=%s,\n"
            "    sheen_roughness=%s,\n"
            "    clearcoat=%s,\n"
            "    clearcoat_roughness=%s,\n"
            "    clearcoat_ior=%s,\n"
            "    clearcoat_tint=%s,\n"
            "    clearcoat_normalmap=%s,\n"
            "    metallic=%s,\n"
            "    spec_tint=%s,\n"
            "    spec_ior_level=%s,\n"
            "    normalmap=%s,\n"
            "]"
            % (
                self.two_sided,
                self.base_color,
                self.transmission,
                self.roughness,
                self.anisotropic,
                self.eta,
                self.sheen,
                self.sheen_tint,
                self.sheen_roughness,
                self.clearcoat,
                self.clearcoat_roughness,
                self.clearcoat_ior,
                self.clearcoat_tint,
                self.clearcoat_normalmap,
                self.metallic,
                self.spec_tint,
                self.spec_ior_level,
                self.normalmap,
            )
        )


mi.register_bsdf("blender_principled", lambda props: BlenderPrincipledBSDF(props))
