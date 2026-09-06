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

        m_base_color = props.get_texture<Texture>("base_color", 0.5f);
        m_roughness = props.get_texture<Texture>("roughness", 0.5f);
        m_has_anisotropic = get_flag("anisotropic", props);
        m_anisotropic = props.get_texture<Texture>("anisotropic", 0.0f);
        m_has_spec_trans = get_flag("spec_trans", props);
        m_spec_trans = props.get_texture<Texture>("spec_trans", 0.0f);
        m_has_sheen = get_flag("sheen", props);
        m_sheen = props.get_texture<Texture>("sheen", 0.0f);
        m_has_sheen_tint = get_flag("sheen_tint", props);
        m_sheen_tint = props.get_texture<Texture>("sheen_tint", 0.0f);
        m_has_flatness = get_flag("flatness", props);
        m_flatness = props.get_texture<Texture>("flatness", 0.0f);
        m_has_spec_tint = get_flag("spec_tint", props);
        m_spec_tint = props.get_texture<Texture>("spec_tint", 0.0f);
        m_eta_thin = props.get_unbounded_texture<Texture>("eta", 1.5f);
        m_has_diff_trans = get_flag("diff_trans", props);
        m_diff_trans = props.get_texture<Texture>("diff_trans", 0.0f);
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
        m_components.clear();
        m_flags = +BSDFFlags::Empty;

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

    void traverse(TraversalCallback *cb) override {
        cb->put("eta",                                   m_eta_thin,         ParamFlags::Differentiable | ParamFlags::Discontinuous);
        cb->put("roughness",                             m_roughness,        ParamFlags::Differentiable | ParamFlags::Discontinuous);
        cb->put("diff_trans",                            m_diff_trans,       ParamFlags::Differentiable);
        cb->put("specular_reflectance_sampling_rate",    m_spec_refl_srate,  ParamFlags::NonDifferentiable);
        cb->put("diffuse_reflectance_sampling_rate",     m_diff_refl_srate,  ParamFlags::NonDifferentiable);
        cb->put("diffuse_transmittance_sampling_rate",   m_diff_trans_srate, ParamFlags::NonDifferentiable);
        cb->put("specular_transmittance_sampling_rate",  m_spec_trans_srate, ParamFlags::NonDifferentiable);
        cb->put("base_color",                            m_base_color,       ParamFlags::Differentiable);
        cb->put("anisotropic",                           m_anisotropic,      ParamFlags::Differentiable);
        cb->put("spec_tint",                             m_spec_tint,        ParamFlags::Differentiable);
        cb->put("sheen",                                 m_sheen,            ParamFlags::Differentiable);
        cb->put("sheen_tint",                            m_sheen_tint,       ParamFlags::Differentiable);
        cb->put("spec_trans",                            m_spec_trans,       ParamFlags::Differentiable);
        cb->put("flatness",                              m_flatness,         ParamFlags::Differentiable);
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
    sample(const BSDFContext &, const SurfaceInteraction3f &si,
           Float sample1, const Point2f &sample2, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFSample, active);

        // Ignore perfectly grazing configurations
        active &= Frame3f::cos_theta(si.wi) != 0.0f;

        if (unlikely(dr::none_or<false>(active)))
            return { dr::zeros<BSDFSample3f>(), 0.0f };

        return sample_impl(si, eval_params(si, active), sample1, sample2,
                           active);
    }

    Spectrum eval(const BSDFContext &, const SurfaceInteraction3f &si,
                  const Vector3f &wo, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        active &= Frame3f::cos_theta(si.wi) != 0.0f;

        if (unlikely(dr::none_or<false>(active)))
            return 0.0f;

        auto [value, pdf] =
            eval_pdf_impl(si, eval_params(si, active), wo, active);

        return depolarizer<Spectrum>(value) & active;
    }

    Float pdf(const BSDFContext &, const SurfaceInteraction3f &si,
              const Vector3f &wo, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        active &= Frame3f::cos_theta(si.wi) != 0.0f;

        if (unlikely(dr::none_or<false>(active)))
            return 0.0f;

        auto [value, pdf] =
            eval_pdf_impl(si, eval_params(si, active), wo, active);

        return pdf;
    }

    std::pair<Spectrum, Float> eval_pdf(const BSDFContext &,
                                        const SurfaceInteraction3f &si,
                                        const Vector3f &wo,
                                        Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        active &= Frame3f::cos_theta(si.wi) != 0.0f;

        if (unlikely(dr::none_or<false>(active)))
            return { 0.0f, 0.0f };

        auto [value, pdf] =
            eval_pdf_impl(si, eval_params(si, active), wo, active);

        return { depolarizer<Spectrum>(value) & active, pdf };
    }

    std::tuple<Spectrum, Float, BSDFSample3f, Spectrum>
    eval_pdf_sample(const BSDFContext &, const SurfaceInteraction3f &si,
                    const Vector3f &wo, Float sample1, const Point2f &sample2,
                    Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        active &= Frame3f::cos_theta(si.wi) != 0.0f;

        if (unlikely(dr::none_or<false>(active)))
            return { 0.0f, 0.0f, dr::zeros<BSDFSample3f>(), 0.0f };

        // The material parameters are shared by both queries
        Params p = eval_params(si, active);

        auto [value, pdf] = eval_pdf_impl(si, p, wo, active);
        auto [bs, weight] = sample_impl(si, p, sample1, sample2, active);

        return { depolarizer<Spectrum>(value) & active, pdf, bs, weight };
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
    MI_DECLARE_CLASS(PrincipledThin)
private:
    /// Material parameters at a surface position, fetched once per query
    struct Params {
        Float roughness, anisotropic, spec_trans, diff_trans, eta, spec_tint,
              sheen, sheen_tint, flatness;
        UnpolarizedSpectrum base_color;

        /// Base color normalized by its luminance
        UnpolarizedSpectrum c_tint;

        /// Roughness of the specular reflection and transmission lobes
        Float alpha_x, alpha_y, alpha_x_trans, alpha_y_trans;

        /// Discrete probabilities of the four sampling techniques
        Float prob_spec_reflect, prob_spec_trans, prob_diff_reflect,
              prob_diff_trans;
    };

    Params eval_params(const SurfaceInteraction3f &si, Mask active) const {
        Params p;

        p.roughness   = m_roughness->eval_1(si, active);
        p.anisotropic = m_has_anisotropic ? m_anisotropic->eval_1(si, active) : 0.0f;
        p.spec_trans  = m_has_spec_trans ? m_spec_trans->eval_1(si, active) : 0.0f;
        p.eta         = m_eta_thin->eval_1(si, active);
        p.spec_tint   = m_has_spec_tint ? m_spec_tint->eval_1(si, active) : 0.0f;
        p.sheen       = m_has_sheen ? m_sheen->eval_1(si, active) : 0.0f;
        p.sheen_tint  = m_has_sheen_tint ? m_sheen_tint->eval_1(si, active) : 0.0f;
        p.flatness    = m_has_flatness ? m_flatness->eval_1(si, active) : 0.0f;
        p.base_color  = m_base_color->eval(si, active);

        // The diff_trans parameter ranges from 0 to 2 and is mapped to 0 to 1
        p.diff_trans = m_has_diff_trans ? m_diff_trans->eval_1(si, active) / 2.0f : 0.0f;

        p.c_tint = 1.0f;
        if (m_has_spec_tint || m_has_sheen_tint) {
            Float lum = mitsuba::luminance(p.base_color, si.wavelengths);
            p.c_tint = dr::select(lum > 0.0f, p.base_color / lum, 1.0f);
        }

        std::tie(p.alpha_x, p.alpha_y) =
            calc_dist_params(p.anisotropic, p.roughness, m_has_anisotropic);

        if (m_has_spec_trans) {
            // The transmission lobe scales the roughness by the index of
            // refraction (Burley 2015, Figure 15)
            Float roughness_scaled = (0.65f * p.eta - 0.35f) * p.roughness;
            std::tie(p.alpha_x_trans, p.alpha_y_trans) = calc_dist_params(
                p.anisotropic, roughness_scaled, m_has_anisotropic);
        }

        // Lobe selection probabilities
        Float diffuse = 1.0f - p.spec_trans;
        p.prob_spec_reflect =
            m_has_spec_trans ? p.spec_trans * m_spec_refl_srate / 2.0f : 0.0f;
        p.prob_spec_trans =
            m_has_spec_trans ? p.spec_trans * m_spec_trans_srate / 2.0f : 0.0f;
        p.prob_diff_reflect = m_diff_refl_srate * diffuse * (1.0f - p.diff_trans);
        p.prob_diff_trans =
            m_has_diff_trans ? m_diff_trans_srate * diffuse * p.diff_trans : 0.0f;

        Float rcp_total = dr::rcp(p.prob_spec_reflect + p.prob_spec_trans +
                                  p.prob_diff_reflect + p.prob_diff_trans);
        p.prob_spec_reflect *= rcp_total;
        p.prob_spec_trans   *= rcp_total;
        p.prob_diff_reflect *= rcp_total;
        p.prob_diff_trans   *= rcp_total;

        return p;
    }

    /// Jointly evaluate the BSDF times the cosine factor and the sampling pdf
    std::pair<UnpolarizedSpectrum, Float>
    eval_pdf_impl(const SurfaceInteraction3f &si, const Params &p,
                  const Vector3f &wo, Mask active) const {
        Float cos_theta_i = Frame3f::cos_theta(si.wi);

        // The thin BSDF is symmetric. Flip both directions so that the
        // incident direction lies in the upper hemisphere.
        Vector3f wi   = dr::mulsign(si.wi, cos_theta_i),
                 wo_t = dr::mulsign(wo, cos_theta_i);
        cos_theta_i = dr::abs(cos_theta_i);

        Float cos_theta_o = Frame3f::cos_theta(wo_t);
        Mask reflect = cos_theta_o > 0.0f,
             refract = cos_theta_o < 0.0f;

        // Half vector of wi and the outgoing direction mirrored into the
        // upper hemisphere. Specular transmission reflects at the microfacet
        // and flips the result to the other side.
        Vector3f wo_r(wo_t.x(), wo_t.y(), dr::abs(cos_theta_o));
        Vector3f wh = dr::normalize(wi + wo_r);

        Float dot_wi_h = dr::dot(wi, wh),
              dot_wo_h = dr::dot(wo_t, wh);

        UnpolarizedSpectrum value(0.0f);
        Float pdf(0.0f);

        if (m_has_spec_trans) {
            Float F_dielectric = std::get<0>(fresnel(dot_wi_h, p.eta));

            // Jacobian of the half vector mapping of the mirrored direction
            Float dwh_dwo = dr::abs(dr::rcp(4.0f * dr::dot(wo_r, wh)));

            // Specular reflection
            Mask spec_reflect_active = active && reflect;
            if (dr::any_or<true>(spec_reflect_active)) {
                MicrofacetDistribution distr(MicrofacetType::GGX, p.alpha_x,
                                             p.alpha_y);

                UnpolarizedSpectrum F_thin =
                    thin_fresnel(F_dielectric, p.spec_tint, p.c_tint,
                                 dot_wi_h, p.eta, m_has_spec_tint);

                Float D = distr.eval(wh),
                      G = distr.G(wi, wo_t, wh);

                dr::masked(value, spec_reflect_active) +=
                    p.spec_trans * F_thin * D * G / (4.0f * cos_theta_i);
                dr::masked(pdf, spec_reflect_active) +=
                    p.prob_spec_reflect * distr.pdf(wi, wh) * dwh_dwo;
            }

            // Specular transmission. No microfacet transmits into directions
            // that face the half vector from the incident side.
            Mask spec_trans_active = active && refract && dot_wo_h < 0.0f;
            if (dr::any_or<true>(spec_trans_active)) {
                MicrofacetDistribution distr(MicrofacetType::GGX,
                                             p.alpha_x_trans, p.alpha_y_trans);

                Float D = distr.eval(wh),
                      G = distr.G(wi, wo_t, wh);

                dr::masked(value, spec_trans_active) +=
                    p.spec_trans * p.base_color * (1.0f - F_dielectric) * D *
                    G / (4.0f * cos_theta_i);
                dr::masked(pdf, spec_trans_active) +=
                    p.prob_spec_trans * distr.pdf(wi, wh) * dwh_dwo;
            }
        }

        // Diffuse, retro-reflection, fake subsurface, and sheen lobes
        Mask diffuse_reflect_active = active && reflect;
        if (dr::any_or<true>(diffuse_reflect_active)) {
            Float Fo = schlick_weight(cos_theta_o),
                  Fi = schlick_weight(cos_theta_i);

            Float f_diff = (1.0f - 0.5f * Fi) * (1.0f - 0.5f * Fo);

            // Retro reflection
            Float Rr      = 2.0f * p.roughness * dr::square(dot_wo_h),
                  f_retro = Rr * (Fo + Fi + Fo * Fi * (Rr - 1.0f)),
                  f       = f_diff + f_retro;

            if (m_has_flatness) {
                // Fake subsurface scattering based on Hanrahan-Krueger
                Float Fss90 = Rr / 2.f,
                      Fss   = dr::lerp(1.f, Fss90, Fo) * dr::lerp(1.f, Fss90, Fi),
                      f_ss  = 1.25f * (Fss * (1.f / (cos_theta_o + cos_theta_i) - 0.5f) + 0.5f);

                f = dr::lerp(f, f_ss, p.flatness);
            }

            Float weight = (1.0f - p.spec_trans) * (1.0f - p.diff_trans);
            UnpolarizedSpectrum diffuse =
                weight * cos_theta_o * p.base_color * dr::InvPi<Float> * f;

            if (m_has_sheen) {
                Float Fd = schlick_weight(dot_wo_h);
                UnpolarizedSpectrum c_sheen =
                    m_has_sheen_tint ? dr::lerp(1.0f, p.c_tint, p.sheen_tint)
                                     : UnpolarizedSpectrum(1.0f);

                diffuse += p.sheen * weight * Fd * c_sheen * cos_theta_o;
            }

            dr::masked(value, diffuse_reflect_active) += diffuse;
            dr::masked(pdf, diffuse_reflect_active) +=
                p.prob_diff_reflect * dr::InvPi<Float> * cos_theta_o;
        }

        // Diffuse transmission
        if (m_has_diff_trans) {
            Mask diffuse_trans_active = active && refract;
            if (dr::any_or<true>(diffuse_trans_active)) {
                dr::masked(value, diffuse_trans_active) +=
                    (1.0f - p.spec_trans) * p.diff_trans * p.base_color *
                    dr::InvPi<Float> * (-cos_theta_o);
                dr::masked(pdf, diffuse_trans_active) +=
                    p.prob_diff_trans * dr::InvPi<Float> * (-cos_theta_o);
            }
        }

        return { value, pdf };
    }

    std::pair<BSDFSample3f, Spectrum>
    sample_impl(const SurfaceInteraction3f &si, const Params &p, Float sample1,
                const Point2f &sample2, Mask active) const {
        Float cos_theta_i = Frame3f::cos_theta(si.wi);
        BSDFSample3f bs   = dr::zeros<BSDFSample3f>();

        // The thin BSDF is symmetric. Sample relative to the incident
        // direction flipped into the upper hemisphere and flip the result
        // back at the end.
        Vector3f wi = dr::mulsign(si.wi, cos_theta_i);

        // Select a lobe
        Float cdf = p.prob_spec_reflect;
        Mask sample_spec_reflect = m_has_spec_trans && active && sample1 < cdf;
        Mask sample_spec_trans = m_has_spec_trans && active && sample1 >= cdf &&
                                 sample1 < cdf + p.prob_spec_trans;
        cdf += p.prob_spec_trans;
        Mask sample_diff_reflect = active && sample1 >= cdf &&
                                   sample1 < cdf + p.prob_diff_reflect;
        cdf += p.prob_diff_reflect;
        Mask sample_diff_trans = m_has_diff_trans && active && sample1 >= cdf;

        // Both sides of a thin surface share the same index of refraction
        bs.eta = 1.0f;

        // Specular reflection
        if (m_has_spec_trans && dr::any_or<true>(sample_spec_reflect)) {
            MicrofacetDistribution distr(MicrofacetType::GGX, p.alpha_x,
                                         p.alpha_y);
            Normal3f m  = std::get<0>(distr.sample(wi, sample2));
            Vector3f wo = reflect(wi, m);

            dr::masked(bs.wo, sample_spec_reflect) = wo;
            dr::masked(bs.sampled_component, sample_spec_reflect) = 3;
            dr::masked(bs.sampled_type, sample_spec_reflect) =
                +BSDFFlags::GlossyReflection;

            // Discard reflections into the wrong hemisphere
            active &= !sample_spec_reflect || Frame3f::cos_theta(wo) > 0.0f;
        }

        // Specular transmission. A thin surface does not bend the ray, so
        // the direction reflects at the microfacet and then flips to the
        // other side.
        if (m_has_spec_trans && dr::any_or<true>(sample_spec_trans)) {
            MicrofacetDistribution distr(MicrofacetType::GGX, p.alpha_x_trans,
                                         p.alpha_y_trans);
            Normal3f m  = std::get<0>(distr.sample(wi, sample2));
            Vector3f wo = reflect(wi, m);
            wo.z() = -wo.z();

            dr::masked(bs.wo, sample_spec_trans) = wo;
            dr::masked(bs.sampled_component, sample_spec_trans) = 2;
            dr::masked(bs.sampled_type, sample_spec_trans) =
                +BSDFFlags::GlossyTransmission;

            // Discard transmissions into the wrong hemisphere and directions
            // that face the microfacet, for which the lobe has no density
            active &= !sample_spec_trans ||
                      (Frame3f::cos_theta(wo) < 0.0f && dr::dot(wo, m) < 0.0f);
        }

        // Diffuse reflection
        if (dr::any_or<true>(sample_diff_reflect)) {
            dr::masked(bs.wo, sample_diff_reflect) =
                warp::square_to_cosine_hemisphere(sample2);
            dr::masked(bs.sampled_component, sample_diff_reflect) = 0;
            dr::masked(bs.sampled_type, sample_diff_reflect) =
                +BSDFFlags::DiffuseReflection;
        }

        // Diffuse transmission
        if (m_has_diff_trans && dr::any_or<true>(sample_diff_trans)) {
            dr::masked(bs.wo, sample_diff_trans) =
                -warp::square_to_cosine_hemisphere(sample2);
            dr::masked(bs.sampled_component, sample_diff_trans) = 1;
            dr::masked(bs.sampled_type, sample_diff_trans) =
                +BSDFFlags::DiffuseTransmission;
        }

        bs.wo = dr::mulsign(bs.wo, cos_theta_i);

        auto [value, pdf] = eval_pdf_impl(si, p, bs.wo, active);
        bs.pdf = pdf;
        active &= pdf > 0.0f;

        return { bs, depolarizer<Spectrum>(value / pdf) & active };
    }

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

    MI_TRAVERSE_CB(Base, m_base_color, m_roughness, m_anisotropic, m_sheen,
                   m_sheen_tint, m_spec_trans, m_flatness, m_spec_tint,
                   m_diff_trans, m_eta_thin)
};

MI_EXPORT_PLUGIN(PrincipledThin)
NAMESPACE_END(mitsuba)
