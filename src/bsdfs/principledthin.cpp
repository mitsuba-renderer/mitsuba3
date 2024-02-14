#include <mitsuba/core/fwd.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/string.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/fresnel.h>
#include <mitsuba/render/ior.h>
#include <mitsuba/render/microfacet.h>
#include <mitsuba/render/sampler.h>
#include <mitsuba/render/texture.h>
#include "principledhelpers.h"

NAMESPACE_BEGIN(mitsuba)

/**!
.. _bsdf-principledthin:

The Thin Principled BSDF (:monosp:`principledthin`)
-----------------------------------------------------
.. pluginparameters::

 * - base_color
   - |spectrum| or |texture|
   - The color of the material. (Default: 0.5)
   - |exposed|, |differentiable|

 * - roughness
   - |float| or |texture|
   - Controls the roughness parameter of the main specular lobes. (Default: 0.5)
   - |exposed|, |differentiable|, |discontinuous|

 * - anisotropic
   - |float| or |texture|
   - Controls the degree of anisotropy. (0.0: isotropic material) (Default: 0.0)
   - |exposed|, |differentiable|, |discontinuous|

 * - spec_trans
   - |texture| or |float|
   - Blends diffuse and specular responses. (1.0: only
     specular response, 0.0 : only diffuse response.)(Default: 0.0)
   - |exposed|, |differentiable|

 * - eta
   - |float| or |texture|
   - Interior IOR/Exterior IOR (Default: 1.5)
   - |exposed|, |differentiable|, |discontinuous|

 * - spec_tint
   - |texture| or |float|
   - The fraction of `base_color` tint applied onto the dielectric reflection
     lobe. (Default: 0.0)
   - |exposed|, |differentiable|

 * - sheen
   - |float| or |texture|
   - The rate of the sheen lobe. (Default: 0.0)
   - |exposed|, |differentiable|

 * - sheen_tint
   - |float| or |texture|
   - The fraction of `base_color` tint applied onto the sheen lobe. (Default: 0.0)
   - |exposed|, |differentiable|

 * - flatness
   - |float| or |texture|
   - Blends between the diffuse response and fake subsurface approximation based
     on Hanrahan-Krueger approximation. (0.0:only diffuse response, 1.0:only
     fake subsurface scattering.) (Default: 0.0)
   - |exposed|, |differentiable|

 * - diff_trans
   - |texture| or |float|
   - The fraction that the energy of diffuse reflection is given to the
     transmission. (0.0: only diffuse reflection, 2.0: only diffuse
     transmission) (Default:0.0)
   - |exposed|, |differentiable|

 * - diffuse_reflectance_sampling_rate
   - |float|
   - The rate of the cosine hemisphere reflection in sampling. (Default: 1.0)
   - |exposed|

 * - specular_reflectance_sampling_rate
   - |float|
   - The rate of the main specular reflection in sampling. (Default: 1.0)
   - |exposed|

 * - specular_transmittance_sampling_rate
   - |float|
   - The rate of the main specular transmission in sampling. (Default: 1.0)
   - |exposed|

 * - diffuse_transmittance_sampling_rate
   - |float|
   - The rate of the cosine hemisphere transmission in sampling. (Default: 1.0)
   - |exposed|

The thin principled BSDF is a complex BSDF which is designed by approximating
some features of thin, translucent materials. The implementation is based on
the papers *Physically Based Shading at Disney* :cite:`Disney2012` and
*Extending the Disney BRDF to a BSDF with Integrated Subsurface Scattering*
:cite:`Disney2015` by Brent Burley.

Images below show how the input parameters affect the appearance of the objects
while one of the parameters is changed for each row.

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/thinprincipled_blend.png
    :caption: Blending of parameters
.. subfigend::
    :label: fig-blend-principledthin

You can see the general structure of the BSDF below.

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/bsdf/principledthin.png
    :caption: The general structure of the thin principled BSDF
.. subfigend::
    :label: fig-structure-thin

The following XML snippet describes a material definition for
:monosp:`principledthin` material:

.. tabs::
    .. code-tab:: xml
        :name: principledthin

        <bsdf type="principledthin">
            <rgb name="base_color" value="0.7,0.1,0.1 "/>
            <float name="roughness" value="0.15" />
            <float name="spec_tint" value="0.1" />
            <float name="anisotropic" value="0.5" />
            <float name="spec_trans" value="0.8" />
            <float name="diff_trans" value="0.3" />
            <float name="eta" value="1.33" />
        </bsdf>

    .. code-tab:: python

        'type': 'principledthin',
        'base_color': {
            'type': 'rgb',
            'value': [0.7, 0.1, 0.1]
        },
        'roughness': 0.15,
        'spec_tint': 0.1,
        'anisotropic': 0.5,
        'spec_trans': 0.8,
        'diff_trans': 0.3,
        'eta': 1.33

All of the parameters, except sampling rates, `diff_trans` and
`eta`, should take values between 0.0 and 1.0. The range of
`diff_trans` is 0.0 to 2.0.
 */
template <typename Float, typename Spectrum>
class PrincipledThin final : public BSDF<Float, Spectrum> {
public:
    MI_IMPORT_BASE(BSDF, m_flags, m_components)
    MI_IMPORT_TYPES(Texture, MicrofacetDistribution)

    PrincipledThin(const Properties &props) : Base(props) {

        m_base_color = props.texture<Texture>("base_color", 0.5f);
        m_roughness = props.texture<Texture>("roughness", 0.5f);
        m_has_anisotropic = get_flag("anisotropic", props);
        m_anisotropic = props.texture<Texture>("anisotropic", 0.0f);
        m_has_spec_trans = get_flag("spec_trans", props);
        m_spec_trans = props.texture<Texture>("spec_trans", 0.0f);
        m_has_sheen = get_flag("sheen", props);
        m_sheen = props.texture<Texture>("sheen", 0.0f);
        m_has_sheen_tint = get_flag("sheen_tint", props);
        m_sheen_tint = props.texture<Texture>("sheen_tint", 0.0f);
        m_has_flatness = get_flag("flatness", props);
        m_flatness = props.texture<Texture>("flatness", 0.0f);
        m_has_spec_tint = get_flag("spec_tint", props);
        m_spec_tint = props.texture<Texture>("spec_tint", 0.0f);
        m_eta_thin = props.texture<Texture>("eta", 1.5f);
        m_has_diff_trans = get_flag("diff_trans", props);
        m_diff_trans = props.texture<Texture>("diff_trans", 0.0f);
        m_spec_refl_srate =
                props.get("specular_reflectance_sampling_rate", 1.0f);
        m_spec_trans_srate =
                props.get("specular_transmittance_sampling_rate", 1.0f);
        m_diff_trans_srate =
                props.get("diffuse_transmittance_sampling_rate", 1.0f);
        m_diff_refl_srate =
                props.get("diffuse_reflectance_sampling_rate", 1.0f);

        initialize_lobes();
    }

    void initialize_lobes() {
        // Diffuse reflection lobe
        m_components.push_back(BSDFFlags::DiffuseReflection |
                               BSDFFlags::FrontSide | BSDFFlags::BackSide);
        // Specular diffuse lobe
        m_components.push_back(BSDFFlags::DiffuseTransmission |
                               BSDFFlags::FrontSide | BSDFFlags::BackSide);

        // Specular transmission lobe
        if (m_has_spec_trans) {
            uint32_t f = BSDFFlags::GlossyTransmission | BSDFFlags::FrontSide |
                         BSDFFlags::BackSide;
            if (m_has_anisotropic)
                f = f | BSDFFlags::Anisotropic;
            m_components.push_back(f);
        }

        // Main specular reflection lobe
        uint32_t f = BSDFFlags::GlossyReflection | BSDFFlags::FrontSide |
                     BSDFFlags::BackSide;
        if (m_has_anisotropic)
            f = f | BSDFFlags::Anisotropic;
        m_components.push_back(f);

        for (auto c : m_components)
            m_flags |= c;
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_object("eta",                                     m_eta_thin.get() , ParamFlags::Differentiable | ParamFlags::Discontinuous);
        callback->put_object("roughness",                               m_roughness.get(), ParamFlags::Differentiable | ParamFlags::Discontinuous);
        callback->put_object("diff_trans",                              m_diff_trans.get(),  +ParamFlags::Differentiable);
        callback->put_parameter("specular_reflectance_sampling_rate",   m_spec_refl_srate,   +ParamFlags::NonDifferentiable);
        callback->put_parameter("diffuse_reflectance_sampling_rate",    m_diff_refl_srate,   +ParamFlags::NonDifferentiable);
        callback->put_parameter("diffuse_transmittance_sampling_rate",  m_diff_trans_srate,  +ParamFlags::NonDifferentiable);
        callback->put_parameter("specular_transmittance_sampling_rate", m_spec_trans_srate,  +ParamFlags::NonDifferentiable);
        callback->put_object("base_color",                              m_base_color.get(),  +ParamFlags::Differentiable);
        callback->put_object("anisotropic",                             m_anisotropic.get(), +ParamFlags::Differentiable);
        callback->put_object("spec_tint",                               m_spec_tint.get(),   +ParamFlags::Differentiable);
        callback->put_object("sheen",                                   m_sheen.get(),       +ParamFlags::Differentiable);
        callback->put_object("sheen_tint",                              m_sheen_tint.get(),  +ParamFlags::Differentiable);
        callback->put_object("spec_trans",                              m_spec_trans.get(),  +ParamFlags::Differentiable);
        callback->put_object("flatness",                                m_flatness.get(),    +ParamFlags::Differentiable);
    }

    void
    parameters_changed(const std::vector <std::string> &keys = {}) override {
        // In case the parameters are changed from zero to something else
        // boolean flags need to be changed also.
        if (string::contains(keys, "spec_trans"))
            m_has_spec_trans = true;
        if (string::contains(keys, "diff_trans"))
            m_has_diff_trans = true;
        if (string::contains(keys, "sheen"))
            m_has_sheen = true;
        if (string::contains(keys, "sheen_tint"))
            m_has_sheen_tint = true;
        if (string::contains(keys, "anisotropic"))
            m_has_anisotropic = true;
        if (string::contains(keys, "flatness"))
            m_has_flatness = true;
        if (string::contains(keys, "spec_tint"))
            m_has_spec_tint = true;

        initialize_lobes();
    }

    std::pair<BSDFSample3f, Spectrum>
    sample(const BSDFContext &ctx, const SurfaceInteraction3f &si,
           Float sample1, const Point2f &sample2, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFSample, active);

        Float cos_theta_i = Frame3f::cos_theta(si.wi);
        BSDFSample3f bs   = dr::zeros<BSDFSample3f>();

        // Ignoring perfectly grazing incoming rays
        active &= cos_theta_i != 0.0f;

        if (unlikely(dr::none_or<false>(active)))
            return { bs, 0.0f };

        // Store the weights.
        Float anisotropic =
                m_has_anisotropic ? m_anisotropic->eval_1(si, active) : 0.0f,
                roughness = m_roughness->eval_1(si, active),
                spec_trans =
                        m_has_spec_trans ? m_spec_trans->eval_1(si, active) : 0.0f;
        /* Diffuse transmission weight. Normally, its range is 0-2, we
               make it 0-1 here. */
        Float diff_trans =
                m_has_diff_trans ? m_diff_trans->eval_1(si, active) / 2.0f : 0.0f;

        // There is no negative incoming angle for a thin surface, so we
        // change the direction for back_side case. The direction change is
        // taken into account after sampling the outgoing direction.
        Vector3f wi = dr::mulsign(si.wi, cos_theta_i);

        // Probability for each minor lobe
        Float prob_spec_reflect =
                m_has_spec_trans ? spec_trans * m_spec_refl_srate/2.0f : 0.0f;
        Float prob_spec_trans =
                m_has_spec_trans ? spec_trans * m_spec_trans_srate/2.0f : 0.0f;
        Float prob_coshemi_reflect =
                m_diff_refl_srate * (1.0f - spec_trans) * (1.0f - diff_trans);
        Float prob_coshemi_trans =
                m_has_diff_trans
                ? m_diff_trans_srate * (1.0f - spec_trans) * (diff_trans)
                : 0.0f;

        // Normalizing the probabilities for the specular minor lobes
        Float rcp_total_prob =
                dr::rcp(prob_spec_reflect + prob_spec_trans + prob_coshemi_reflect +
                prob_coshemi_trans);

        prob_spec_reflect *= rcp_total_prob;
        prob_spec_trans *= rcp_total_prob;
        prob_coshemi_reflect *= rcp_total_prob;

        // Sampling masks
        Float curr_prob(0.0f);
        Mask sample_spec_reflect =
                m_has_spec_trans && active && (sample1 < prob_spec_reflect);
        curr_prob += prob_spec_reflect;
        Mask sample_spec_trans = m_has_spec_trans && active &&
                (sample1 >= curr_prob) &&
                (sample1 < curr_prob + prob_spec_trans);
        curr_prob += prob_spec_trans;
        Mask sample_coshemi_reflect =
                active && (sample1 >= curr_prob) &&
                (sample1 < curr_prob + prob_coshemi_reflect);
        curr_prob += prob_coshemi_reflect;
        Mask sample_coshemi_trans =
                m_has_diff_trans && active && (sample1 >= curr_prob);

        // Thin model is just a  2D surface, both mediums have the same index of
        // refraction
        bs.eta = 1.0f;

        // Microfacet reflection lobe
        if (m_has_spec_trans && dr::any_or<true>(sample_spec_reflect)) {
            // Defining the Microfacet Distribution.
            auto [ax, ay] = calc_dist_params(anisotropic, roughness,m_has_anisotropic);
            MicrofacetDistribution spec_reflect_distr(MicrofacetType::GGX, ax,
                                                      ay);
            Normal3f m_spec_reflect =
                    std::get<0>(spec_reflect_distr.sample(wi, sample2));

            // Sampling
            Vector3f wo = reflect(wi, m_spec_reflect);
            dr::masked(bs.wo, sample_spec_reflect)                = wo;
            dr::masked(bs.sampled_component, sample_spec_reflect) = 3;
            dr::masked(bs.sampled_type, sample_spec_reflect) =
                    +BSDFFlags::GlossyReflection;

            // Filter the cases where macro and micro SURFACES do not agree
            // on the same side and the ray is not reflected.
            Mask reflect = Frame3f::cos_theta(wo) > 0.0f;
            active &=
                    (!sample_spec_reflect ||
                    (mac_mic_compatibility(Vector3f(m_spec_reflect),
                                           wi, wo, wi.z(), true) &&
                    reflect));
        }
        // Specular transmission lobe
        if (m_has_spec_trans && dr::any_or<true>(sample_spec_trans)) {
            // Relative index of refraction.
            Float eta_t = m_eta_thin->eval_1(si, active);

            // Defining the scaled distribution for thin specular
            // transmission Scale roughness based on IOR (Burley 2015,
            // Figure 15).
            Float roughness_scaled = (0.65f * eta_t - 0.35f) * roughness;
            auto [ax_scaled, ay_scaled] =
                    calc_dist_params(anisotropic, roughness_scaled,m_has_anisotropic);
            MicrofacetDistribution spec_trans_distr(MicrofacetType::GGX,
                                                    ax_scaled, ay_scaled);
            Normal3f m_spec_trans =
                    std::get<0>(spec_trans_distr.sample(wi, sample2));

            // Here, we are reflecting and turning the ray to the other side
            // since there is no bending on thin surfaces.
            Vector3f wo                          = reflect(wi, m_spec_trans);
            wo.z()                               = -wo.z();
            dr::masked(bs.wo, sample_spec_trans) = wo;
            dr::masked(bs.sampled_component, sample_spec_trans) = 2;
            dr::masked(bs.sampled_type, sample_spec_trans) =
                    +BSDFFlags::GlossyTransmission;

            // Filter the cases where macro and micro SURFACES do not agree
            // on the same side and the ray is not refracted.
            Mask transmission = Frame3f::cos_theta(wo) < 0.0f;
            active &=
                    (!sample_spec_trans ||
                    (mac_mic_compatibility(Vector3f(m_spec_trans), wi, wo, wi.z(), false) &&
                    transmission));
        }
        // Cosine hemisphere reflection for  reflection lobes (diffuse,
        //  retro reflection)
        if (dr::any_or<true>(sample_coshemi_reflect)) {
            dr::masked(bs.wo, sample_coshemi_reflect) =
                    warp::square_to_cosine_hemisphere(sample2);
            dr::masked(bs.sampled_component, sample_coshemi_reflect) = 0;
            dr::masked(bs.sampled_type, sample_coshemi_reflect) =
                    +BSDFFlags::DiffuseReflection;
        }
        // Diffuse transmission lobe
        if (m_has_diff_trans && dr::any_or<true>(sample_coshemi_trans)) {
            dr::masked(bs.wo, sample_coshemi_trans) =
                    -1.0f * warp::square_to_cosine_hemisphere(sample2);
            dr::masked(bs.sampled_component, sample_coshemi_trans) = 1;
            dr::masked(bs.sampled_type, sample_coshemi_trans) =
                    +BSDFFlags::DiffuseTransmission;
        }

        /* The direction is changed once more. (Because it was changed in
           the beginning.) */
        bs.wo = dr::mulsign(bs.wo, cos_theta_i);

        bs.pdf = pdf(ctx, si, bs.wo, active);
        active &= bs.pdf > 0.0f;
        Spectrum result = eval(ctx, si, bs.wo, active);
        return { bs, result / bs.pdf & active };
    }

    Spectrum eval(const BSDFContext &, const SurfaceInteraction3f &si,
                  const Vector3f &wo, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        Float cos_theta_i = Frame3f::cos_theta(si.wi);
        // Ignore perfectly grazing configurations
        active &= cos_theta_i != 0.0f;

        if (unlikely(dr::none_or<false>(active)))
            return 0.0f;

        // Store the weights.
        Float anisotropic =
                m_has_anisotropic ? m_anisotropic->eval_1(si, active) : 0.0f,
                roughness = m_roughness->eval_1(si, active),
                flatness = m_has_flatness ? m_flatness->eval_1(si, active) : 0.0f,
                spec_trans =
                        m_has_spec_trans ? m_spec_trans->eval_1(si, active) : 0.0f,
                        eta_t = m_eta_thin->eval_1(si, active),
                        // The range of diff_trans parameter is 0 to 2. It is made 0 to 1 here.
                        diff_trans =
                                m_has_diff_trans ? m_diff_trans->eval_1(si, active) / 2.0f : 0.0f;
        UnpolarizedSpectrum base_color = m_base_color->eval(si, active);

        // Changing the signs in a way that we are always at the front side.
        // Thin BSDF is symmetric!
        Vector3f wi       = dr::mulsign(si.wi, cos_theta_i);
        Vector3f wo_t     = dr::mulsign(wo, cos_theta_i);
        cos_theta_i       = dr::abs(cos_theta_i);
        Float cos_theta_o = Frame3f::cos_theta(wo_t);

        Mask reflect = cos_theta_o > 0.0f;
        Mask refract = cos_theta_o < 0.0f;

        // Halfway vector calculation
        Vector3f wo_r = wo_t;
        wo_r.z()      = dr::abs(wo_r.z());
        Vector3f wh   = dr::normalize(wi + wo_r);

        /* Masks for controlling the micro-macro surface incompatibilities
           and correct sides. */
        Mask spec_reflect_active =
                active && (spec_trans > 0.0f) && reflect &&
                mac_mic_compatibility(wh, wi, wo_t, wi.z(), true);
        Mask spec_trans_active =
                active && refract && (spec_trans > 0.0f) &&
                mac_mic_compatibility(wh, wi, wo_t, wi.z(), false);
        Mask diffuse_reflect_active =
                active && reflect && (spec_trans < 1.0f) && (diff_trans < 1.0f);
        Mask diffuse_trans_active =
                active && refract && (spec_trans < 1) && (diff_trans > 0.0f);

        // Calculation of eval function starts.
        UnpolarizedSpectrum value = 0.0f;

        // Specular lobes (transmission and reflection)
        if (m_has_spec_trans) {
            // Dielectric Fresnel
            Float F_dielectric = std::get<0>(fresnel(dr::dot(wi, wh), eta_t));

            // Specular reflection lobe
            if (dr::any_or<true>(spec_reflect_active)) {
                // Specular reflection distribution
                auto [ax, ay] = calc_dist_params(anisotropic, roughness,m_has_anisotropic);
                MicrofacetDistribution spec_reflect_distr(MicrofacetType::GGX,
                                                          ax, ay);

                // No need to calculate luminance if there is no color tint.
                Float lum = m_has_spec_tint
                        ? mitsuba::luminance(base_color, si.wavelengths)
                        : 1.0f;
                Float spec_tint =
                        m_has_spec_tint ? m_spec_tint->eval_1(si, active) : 0.0f;

                UnpolarizedSpectrum F_thin = thin_fresnel(F_dielectric,spec_tint,base_color,
                                           lum,dr::dot(wi,wh),eta_t,m_has_spec_tint);

                // Evaluate the microfacet normal distribution
                Float D = spec_reflect_distr.eval(wh);

                // Smith's shadow-masking function
                Float G = spec_reflect_distr.G(wi, wo_t, wh);

                // Calculate the specular reflection component.
                dr::masked(value, spec_reflect_active) +=
                        spec_trans * F_thin * D * G / (4.0f * cos_theta_i);
            }
            // Specular Transmission lobe
            if (dr::any_or<true>(spec_trans_active)) {
                // Defining the scaled distribution for thin specular
                // reflection Scale roughness based on IOR. (Burley 2015,
                // Figure 15).
                Float roughness_scaled = (0.65f * eta_t - 0.35f) * roughness;
                auto [ax_scaled, ay_scaled] =
                        calc_dist_params(anisotropic, roughness_scaled,
                                         m_has_anisotropic);
                MicrofacetDistribution spec_trans_distr(MicrofacetType::GGX,
                                                        ax_scaled, ay_scaled);

                // Evaluate the microfacet normal distribution
                Float D = spec_trans_distr.eval(wh);

                // Smith's shadow-masking function
                Float G = spec_trans_distr.G(wi, wo_t, wh);

                // Calculate the specular transmission component.
                dr::masked(value, spec_trans_active) +=
                        spec_trans * base_color * (1.0f - F_dielectric) * D * G /
                        (4.0f * cos_theta_i);
            }
        }
        // Diffuse, retro reflection, sheen and fake-subsurface evaluation
        if (dr::any_or<true>(diffuse_reflect_active)) {
            Float Fo = schlick_weight(dr::abs(cos_theta_o)),
            Fi = schlick_weight(cos_theta_i);

            // Diffuse response
            Float f_diff = (1.0f - 0.5f * Fi) * (1.0f - 0.5f * Fo);

            // Retro response
            Float cos_theta_d = dr::dot(wh, wo_t);
            Float Rr          = 2.0f * roughness * dr::square(cos_theta_d);
            Float f_retro     = Rr * (Fo + Fi + Fo * Fi * (Rr - 1.0f));

            /* Fake subsurface implementation based on Hanrahan-Krueger
               Fss90 used to "flatten" retro reflection based on
               roughness. */
            if (m_has_flatness) {
                Float Fss90 = Rr / 2.f;
                Float Fss = dr::lerp(1.f, Fss90, Fo) * dr::lerp(1.f, Fss90, Fi);
                Float f_ss = 1.25f * (Fss * (1.f / (dr::abs(cos_theta_o) +
                        dr::abs(cos_theta_i)) - 0.5f) + 0.5f);

                // Adding diffuse, retro and fake subsurface components.
                dr::masked(value, diffuse_reflect_active) +=
                        (1.0f - spec_trans) * cos_theta_o * base_color *
                        dr::InvPi<Float> * (1.0f - diff_trans) *
                        dr::lerp(f_diff + f_retro, f_ss, flatness);
            } else
                // Adding diffuse and retro components. (no fake subsurface)
                dr::masked(value, diffuse_reflect_active) +=
                        (1.0f - spec_trans) * cos_theta_o * base_color *
                        dr::InvPi<Float> * (1.0f - diff_trans) * (f_diff + f_retro);

            // Sheen evaluation
            Float sheen =
                    m_has_sheen ? m_sheen->eval_1(si, active) : Float(0.0f);
            if (m_has_sheen && dr::any_or<true>(sheen > 0.0f)) {

                Float Fd = schlick_weight(dr::abs(cos_theta_d));

                if (m_has_sheen_tint) { // Tints the sheen evaluation to the
                    // base_color.
                    Float sheen_tint = m_sheen_tint->eval_1(si, active);

                    // Calculation of luminance of base_color.
                    Float lum = mitsuba::luminance(base_color, si.wavelengths);

                    // Normalize color with luminance and apply tint.
                    UnpolarizedSpectrum c_tint =
                            dr::select(lum > 0.0f, base_color / lum, 1.0f);
                    UnpolarizedSpectrum c_sheen = dr::lerp(1.0f, c_tint, sheen_tint);

                    // Adding the sheen component with tint.
                    dr::masked(value, diffuse_reflect_active) +=
                            sheen * (1.0f - spec_trans) * Fd * c_sheen *
                            (1.0f - diff_trans) * dr::abs(cos_theta_o);
                } else
                    // Adding the sheen component without tint.
                    dr::masked(value, diffuse_reflect_active) +=
                            sheen * (1.0f - spec_trans) * Fd * (1.0f - diff_trans) *
                            dr::abs(cos_theta_o);
            }
        }
        // Adding diffuse Lambertian transmission component.
        if (m_has_diff_trans && dr::any_or<true>(diffuse_trans_active)) {
            dr::masked(value, diffuse_trans_active) +=
                    (1.0f - spec_trans) * diff_trans * base_color *
                    dr::InvPi<Float> * dr::abs(cos_theta_o);
        }
        return depolarizer<Spectrum>(value) & active;
    }

    Float pdf(const BSDFContext &, const SurfaceInteraction3f &si,
              const Vector3f &wo, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        Float cos_theta_i = Frame3f::cos_theta(si.wi);
        // Ignore perfectly grazing configurations.
        active &= cos_theta_i != 0.0f;

        if (unlikely(dr::none_or<false>(active)))
            return 0.0f;

        // Store the weights.
        Float anisotropic =
                m_has_anisotropic ? m_anisotropic->eval_1(si, active) : 0.0f,
                roughness = m_roughness->eval_1(si, active),
                spec_trans =
                        m_has_spec_trans ? m_spec_trans->eval_1(si, active) : 0.0f,
                        eta_t = m_eta_thin->eval_1(si, active),
                        // The range of diff_trans parameter is 0 to 2. It is made 0 to 1 here
                        diff_trans =
                                m_has_diff_trans ? m_diff_trans->eval_1(si, active) / 2.0f : 0.0f;

        // Changing the signs in a way that we are always at the front side.
        // Thin BSDF is symmetric !!
        Vector3f wi = dr::mulsign(si.wi, cos_theta_i);
        // wo_t stands for thin wo.
        Vector3f wo_t     = dr::mulsign(wo, cos_theta_i);
        Float cos_theta_o = Frame3f::cos_theta(wo_t);

        Mask reflect = cos_theta_o > 0.0f;
        Mask refract = cos_theta_o < 0.0f;

        // Probability definitions
        Float prob_spec_reflect =
                m_has_spec_trans ? spec_trans * m_spec_refl_srate/2.0f : 0.0f;
        Float prob_spec_trans =
                m_has_spec_trans ? spec_trans * m_spec_trans_srate/2.0f : 0.0f;
        Float prob_coshemi_reflect =
                m_diff_refl_srate * (1.0f - spec_trans) * (1.0f - diff_trans);
        Float prob_coshemi_trans =
                m_has_diff_trans
                ? m_diff_trans_srate * (1.0f - spec_trans) * (diff_trans)
                : 0.0f;

        // Normalizing the probabilities
        Float rcp_total_prob =
                dr::rcp(prob_spec_reflect + prob_spec_trans + prob_coshemi_reflect +
                prob_coshemi_trans);
        prob_spec_reflect *= rcp_total_prob;
        prob_spec_trans *= rcp_total_prob;
        prob_coshemi_reflect *= rcp_total_prob;
        prob_coshemi_trans *= rcp_total_prob;

        // Initializing the final pdf value.
        Float pdf(0.0f);

        // Specular lobes' pdf evaluations
        if (m_has_spec_trans) {
            /* Halfway vector calculation. Absolute value is taken since for
             * specular transmission, we first apply microfacet reflection
             * and invert to the other side. */
            Vector3f wo_r = wo_t;
            wo_r.z()      = dr::abs(wo_r.z());
            Vector3f wh   = dr::normalize(wi + wo_r);

            // Macro-micro surface compatibility masks
            Mask mfacet_reflect_macmic =
                    mac_mic_compatibility(wh, wi, wo_t, wi.z(), true) && reflect;
            Mask mfacet_trans_macmic =
                    mac_mic_compatibility(wh, wi, wo_t, wi.z(), false) && refract;

            // d(wh) / d(wo) calculation. Inverted wo is used (wo_r) !
            Float dot_wor_wh  = dr::dot(wo_r, wh);
            Float dwh_dwo_abs = dr::abs(dr::rcp(4.0f * dot_wor_wh));

            // Specular reflection distribution.
            auto [ax, ay] = calc_dist_params(anisotropic, roughness,
                                             m_has_anisotropic);
            MicrofacetDistribution spec_reflect_distr(MicrofacetType::GGX, ax, ay);
            // Defining the scaled distribution for thin specular reflection
            // Scale roughness based on IOR (Burley 2015, Figure 15).
            Float roughness_scaled = (0.65f * eta_t - 0.35f) * roughness;
            auto [ax_scaled, ay_scaled] =
                    calc_dist_params(anisotropic, roughness_scaled,
                                     m_has_anisotropic);
            MicrofacetDistribution spec_trans_distr(MicrofacetType::GGX,
                                                    ax_scaled, ay_scaled);
            // Adding specular lobes' pdfs
            dr::masked(pdf, mfacet_reflect_macmic) +=
                    prob_spec_reflect * spec_reflect_distr.pdf(wi, wh) *
                    dwh_dwo_abs;
            dr::masked(pdf, mfacet_trans_macmic) +=
                    prob_spec_trans * spec_trans_distr.pdf(wi, wh) * dwh_dwo_abs;
        }
        // Adding cosine hemisphere reflection pdf
        dr::masked(pdf, reflect) +=
                prob_coshemi_reflect * warp::square_to_cosine_hemisphere_pdf(wo_t);

        // Adding cosine hemisphere transmission pdf
        if (m_has_diff_trans) {
            dr::masked(pdf, refract) +=
                    prob_coshemi_trans *
                    warp::square_to_cosine_hemisphere_pdf(-wo_t);
        }
        return pdf;
    }

    Spectrum eval_diffuse_reflectance(const SurfaceInteraction3f &si,
                                      Mask active) const override {
        return m_base_color->eval(si, active);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "The Thin Principled BSDF :" << std::endl
            << "base_color: " << m_base_color << "," << std::endl
            << "spec_trans: " << m_spec_trans << "," << std::endl
            << "diff_trans: " << m_diff_trans << "," << std::endl
            << "anisotropic: " << m_anisotropic << "," << std::endl
            << "roughness: " << m_roughness << "," << std::endl
            << "sheen: " << m_sheen << "," << std::endl
            << "sheen_tint: " << m_sheen_tint << "," << std::endl
            << "flatness: " << m_flatness << "," << std::endl
            << "eta: " << m_eta_thin << "," << std::endl
            << "spec_tint: " << m_spec_tint << "," << std::endl;

        return oss.str();
    }
    MI_DECLARE_CLASS()
private:
    /// Parameters of the model
    ref<Texture> m_base_color;
    ref<Texture> m_roughness;
    ref<Texture> m_anisotropic;
    ref<Texture> m_sheen;
    ref<Texture> m_sheen_tint;
    ref<Texture> m_spec_trans;
    ref<Texture> m_flatness;
    ref<Texture> m_spec_tint;
    ref<Texture> m_diff_trans;
    ref<Texture> m_eta_thin;

    /// Sampling rates
    ScalarFloat m_spec_refl_srate;
    ScalarFloat m_spec_trans_srate;
    ScalarFloat m_diff_trans_srate;
    ScalarFloat m_diff_refl_srate;

    /** Whether the lobes are active or not.*/
    bool m_has_sheen;
    bool m_has_diff_trans;
    bool m_has_spec_trans;
    bool m_has_spec_tint;
    bool m_has_sheen_tint;
    bool m_has_anisotropic;
    bool m_has_flatness;
};

MI_IMPLEMENT_CLASS_VARIANT(PrincipledThin, BSDF)
MI_EXPORT_PLUGIN(PrincipledThin, "The Principled Thin Material")
NAMESPACE_END(mitsuba)
