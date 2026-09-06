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
.. _bsdf-principled:

The Principled BSDF (:monosp:`principled`)
-----------------------------------------------------
.. pluginparameters::

 * - base_color
   - |spectrum| or |texture|
   - The color of the material. (Default:0.5)
   - |exposed|, |differentiable|

 * - roughness
   - |float| or |texture|
   - Controls the roughness parameter of the main specular lobes. (Default:0.5)
   - |exposed|, |differentiable|, |discontinuous|

 * - anisotropic
   - |float| or |texture|
   - Controls the degree of anisotropy. (0.0 : isotropic material) (Default:0.0)
   - |exposed|, |differentiable|, |discontinuous|

 * - metallic
   - |texture| or |float|
   - The "metallicness" of the model. (Default:0.0)
   - |exposed|, |differentiable|, |discontinuous|

 * - spec_trans
   - |texture| or |float|
   - Blends BRDF and BSDF major lobe. (1.0: only BSDF
     response, 0.0 : only BRDF response.) (Default: 0.0)
   - |exposed|, |differentiable|, |discontinuous|

 * - eta
   - |float|
   - Interior IOR/Exterior IOR
   - |exposed|, |differentiable|, |discontinuous|

 * - specular
   - |float|
   - Controls the Fresnel reflection coefficient. This parameter has one to one
     correspondence with `eta`, so both of them can not be specified in xml.
     (Default:0.5)
   - |exposed|, |differentiable|, |discontinuous|

 * - spec_tint
   - |texture| or |float|
   - The fraction of `base_color` tint applied onto the dielectric reflection
     lobe. (Default:0.0)
   - |exposed|, |differentiable|

 * - sheen
   - |float| or |texture|
   - The rate of the sheen lobe. (Default:0.0)
   - |exposed|, |differentiable|

 * - sheen_tint
   - |float| or |texture|
   - The fraction of `base_color` tint applied onto the sheen lobe. (Default:0.0)
   - |exposed|, |differentiable|

 * - flatness
   - |float| or |texture|
   - Blends between the diffuse response and fake subsurface approximation based
     on Hanrahan-Krueger approximation. (0.0:only diffuse response, 1.0:only
     fake subsurface scattering.) (Default:0.0)
   - |exposed|, |differentiable|

 * - clearcoat
   - |texture| or |float|
   - The rate of the secondary isotropic specular lobe. (Default:0.0)
   - |exposed|, |differentiable|, |discontinuous|

 * - clearcoat_gloss
   - |texture| or |float|
   - Controls the roughness of the secondary specular lobe. Clearcoat response
     gets glossier as the parameter increases. (Default:0.0)
   - |exposed|, |differentiable|, |discontinuous|

 * - diffuse_reflectance_sampling_rate
   - |float|
   - The rate of the cosine hemisphere reflection in sampling. (Default:1.0)
   - |exposed|

 * - main_specular_sampling_rate
   - |float|
   - The rate of the main specular lobe in sampling. (Default:1.0)
   - |exposed|

 * - clearcoat_sampling_rate
   - |float|
   - The rate of the secondary specular reflection in sampling. (Default:0.0)
   - |exposed|

The principled BSDF is a complex BSDF with numerous reflective and transmissive
lobes. It is able to produce great number of material types ranging from metals
to rough dielectrics. Moreover, the set of input parameters are designed to be
artist-friendly and do not directly correspond to physical units.

The implementation is based on the papers *Physically Based Shading at Disney*
:cite:`Disney2012` and *Extending the Disney BRDF to a BSDF with Integrated
Subsurface Scattering* :cite:`Disney2015` by Brent Burley.

 .. note::

    Subsurface scattering and volumetric extinction is not supported!

Images below show how the input parameters affect the appearance of the objects
while one of the parameters is changed for each column.

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/principled_blend.png
   :caption: Blending of parameters when transmission lobe is turned off.
.. subfigure:: ../../resources/data/docs/images/render/principled_st_blend.png
    :caption: Blending of parameters when transmission lobe is turned on.
.. subfigend::
    :label: fig-blend-principled

You can see the general structure of the BSDF below.

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/bsdf/principled.png
    :caption: The general structure of the principled BSDF
.. subfigend::
    :label: fig-structure-principled

The following XML snippet describes a material definition for :monosp:`principled`
material:

.. tabs::
    .. code-tab:: xml
        :name: principled

        <bsdf type="principled">
            <rgb name="base_color" value="1.0,1.0,1.0"/>
            <float name="metallic" value="0.7" />
            <float name="specular" value="0.6" />
            <float name="roughness" value="0.2" />
            <float name="spec_tint" value="0.4" />
            <float name="anisotropic" value="0.5" />
            <float name="sheen" value="0.3" />
            <float name="sheen_tint" value="0.2" />
            <float name="clearcoat" value="0.6" />
            <float name="clearcoat_gloss" value="0.3" />
            <float name="spec_trans" value="0.4" />
        </bsdf>

    .. code-tab:: python

        'type': 'principled',
        'base_color': {
            'type': 'rgb',
            'value': [1.0, 1.0, 1.0]
        },
        'metallic': 0.7,
        'specular': 0.6,
        'roughness': 0.2,
        'spec_tint': 0.4,
        'anisotropic': 0.5,
        'sheen': 0.3,
        'sheen_tint': 0.2,
        'clearcoat': 0.6,
        'clearcoat_gloss': 0.3,
        'spec_trans': 0.4

All of the parameters except sampling rates and `eta` should take values
between 0.0 and 1.0.
 */
template <typename Float, typename Spectrum>
class Principled final : public BSDF<Float, Spectrum> {
public:
    MI_IMPORT_BASE(BSDF, m_flags, m_components)
    MI_IMPORT_TYPES(Texture, MicrofacetDistribution)

    using GTR1 = GTR1Isotropic<Float, Spectrum>;

    Principled(const Properties &props) : Base(props) {
        // Parameter definitions
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
        m_has_metallic = get_flag("metallic", props);
        m_metallic = props.get_texture<Texture>("metallic", 0.0f);
        m_has_clearcoat = get_flag("clearcoat", props);
        m_clearcoat = props.get_texture<Texture>("clearcoat", 0.0f);
        m_clearcoat_gloss = props.get_texture<Texture>("clearcoat_gloss", 0.0f);
        m_spec_srate = props.get("main_specular_sampling_rate", 1.0f);
        m_clearcoat_srate = props.get("clearcoat_sampling_rate", 1.0f);
        m_diff_refl_srate = props.get("diffuse_reflectance_sampling_rate", 1.0f);

        // Eta and specular has one to one correspondence, both of them can
        // not be specified.
        if (props.has_property("eta") && props.has_property("specular")) {
            Throw("Specified an invalid index of refraction property  "
                  "\"%s\", either use \"eta\" or \"specular\" !");
        } else if (props.has_property("eta")) {
            m_eta_specular = true;
            m_eta = props.get<float>("eta");
        } else {
            m_eta_specular = false;
            m_specular = props.get<float>("specular", 0.5f);
        }

        update_eta();
        initialize_lobes();

        dr::make_opaque(m_eta, m_inv_eta);
        if (!m_eta_specular)
            dr::make_opaque(m_specular);
    }

    // Refraction through an interface with eta == 1 is degenerate (the half
    // vector of wi and the refracted direction vanishes), so nudge eta away
    // from 1 whenever the transmission lobe is enabled.
    void update_eta() {
        if (!m_eta_specular) {
            if (m_has_spec_trans)
                dr::masked(m_specular, m_specular == 0.f) = 1e-3f;
            m_eta = 2.f * dr::rcp(1.f - dr::sqrt(0.08f * m_specular)) - 1.f;
        } else if (m_has_spec_trans) {
            dr::masked(m_eta, m_eta == 1.f) = 1.001f;
        }
        m_inv_eta = dr::rcp(m_eta);
    }

    void initialize_lobes() {
        m_components.clear();
        m_flags = +BSDFFlags::Empty;

        // Diffuse reflection lobe
        m_components.push_back(BSDFFlags::DiffuseReflection |
                               BSDFFlags::FrontSide);

        // Clearcoat lobe
        if (m_has_clearcoat) {
            m_clearcoat_index = (uint32_t) m_components.size();
            m_components.push_back(BSDFFlags::GlossyReflection |
                                   BSDFFlags::FrontSide);
        }

        // Specular transmission lobe
        if (m_has_spec_trans) {
            uint32_t f = BSDFFlags::GlossyTransmission | BSDFFlags::FrontSide |
                         BSDFFlags::BackSide | BSDFFlags::NonSymmetric;
            if (m_has_anisotropic)
                f = f | BSDFFlags::Anisotropic;
            m_spec_trans_index = (uint32_t) m_components.size();
            m_components.push_back(f);
        }

        // Main specular reflection lobe
        uint32_t f = BSDFFlags::GlossyReflection | BSDFFlags::FrontSide |
                     BSDFFlags::BackSide;
        if (m_has_anisotropic)
            f = f | BSDFFlags::Anisotropic;
        m_spec_reflect_index = (uint32_t) m_components.size();
        m_components.push_back(f);

        for (auto c : m_components)
            m_flags |= c;
    }

    void traverse(TraversalCallback *cb) override {
        cb->put("clearcoat",       m_clearcoat,       ParamFlags::Differentiable);
        cb->put("clearcoat_gloss", m_clearcoat_gloss, ParamFlags::Differentiable);
        cb->put("metallic",        m_metallic,        ParamFlags::Differentiable);

        cb->put("main_specular_sampling_rate",       m_spec_srate,      ParamFlags::NonDifferentiable);
        cb->put("clearcoat_sampling_rate",           m_clearcoat_srate, ParamFlags::NonDifferentiable);
        cb->put("diffuse_reflectance_sampling_rate", m_diff_refl_srate, ParamFlags::NonDifferentiable);

        if (m_eta_specular) //Only one of them traversed! (based on xml file)
            cb->put("eta",      m_eta,      ParamFlags::Differentiable | ParamFlags::Discontinuous);
        else
            cb->put("specular", m_specular, ParamFlags::Differentiable | ParamFlags::Discontinuous);

        cb->put("roughness",   m_roughness,   ParamFlags::Differentiable | ParamFlags::Discontinuous);
        cb->put("base_color",  m_base_color,  ParamFlags::Differentiable);
        cb->put("anisotropic", m_anisotropic, ParamFlags::Differentiable);
        cb->put("spec_tint",   m_spec_tint,   ParamFlags::Differentiable);
        cb->put("sheen",       m_sheen,       ParamFlags::Differentiable);
        cb->put("sheen_tint",  m_sheen_tint,  ParamFlags::Differentiable);
        cb->put("spec_trans",  m_spec_trans,  ParamFlags::Differentiable);
        cb->put("flatness",    m_flatness,    ParamFlags::Differentiable);
    }

    void parameters_changed(const std::vector<std::string> &keys = {}) override {
        if (string::contains(keys, "spec_trans"))
            m_has_spec_trans = true;
        if (string::contains(keys, "clearcoat"))
            m_has_clearcoat = true;
        if (string::contains(keys, "sheen"))
            m_has_sheen = true;
        if (string::contains(keys, "sheen_tint"))
            m_has_sheen_tint = true;
        if (string::contains(keys, "anisotropic"))
            m_has_anisotropic = true;
        if (string::contains(keys, "metallic"))
            m_has_metallic = true;
        if (string::contains(keys, "spec_tint"))
            m_has_spec_tint = true;
        if (string::contains(keys, "flatness"))
            m_has_flatness = true;

        update_eta();
        initialize_lobes();

        dr::make_opaque(m_eta, m_inv_eta);
        if (!m_eta_specular)
            dr::make_opaque(m_specular);
    }

    std::pair<BSDFSample3f, Spectrum>
    sample(const BSDFContext &ctx, const SurfaceInteraction3f &si,
           Float sample1, const Point2f &sample2, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFSample, active);

        // Ignore perfectly grazing configurations
        active &= Frame3f::cos_theta(si.wi) != 0.0f;

        if (unlikely(dr::none_or<false>(active)))
            return { dr::zeros<BSDFSample3f>(), 0.0f };

        return sample_impl(ctx, si, eval_params(si, active), sample1, sample2,
                           active);
    }

    Spectrum eval(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                  const Vector3f &wo, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        active &= Frame3f::cos_theta(si.wi) != 0.0f;

        if (unlikely(dr::none_or<false>(active)))
            return 0.0f;

        auto [value, pdf] =
            eval_pdf_impl(ctx, si, eval_params(si, active), wo, active);

        return depolarizer<Spectrum>(value) & active;
    }

    Float pdf(const BSDFContext &ctx, const SurfaceInteraction3f &si,
              const Vector3f &wo, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        active &= Frame3f::cos_theta(si.wi) != 0.0f;

        if (unlikely(dr::none_or<false>(active)))
            return 0.0f;

        auto [value, pdf] =
            eval_pdf_impl(ctx, si, eval_params(si, active), wo, active);

        return pdf;
    }

    std::pair<Spectrum, Float> eval_pdf(const BSDFContext &ctx,
                                        const SurfaceInteraction3f &si,
                                        const Vector3f &wo,
                                        Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        active &= Frame3f::cos_theta(si.wi) != 0.0f;

        if (unlikely(dr::none_or<false>(active)))
            return { 0.0f, 0.0f };

        auto [value, pdf] =
            eval_pdf_impl(ctx, si, eval_params(si, active), wo, active);

        return { depolarizer<Spectrum>(value) & active, pdf };
    }

    std::tuple<Spectrum, Float, BSDFSample3f, Spectrum>
    eval_pdf_sample(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                    const Vector3f &wo, Float sample1, const Point2f &sample2,
                    Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        active &= Frame3f::cos_theta(si.wi) != 0.0f;

        if (unlikely(dr::none_or<false>(active)))
            return { 0.0f, 0.0f, dr::zeros<BSDFSample3f>(), 0.0f };

        // The material parameters are shared by both queries
        Params p = eval_params(si, active);

        auto [value, pdf] = eval_pdf_impl(ctx, si, p, wo, active);
        auto [bs, weight] = sample_impl(ctx, si, p, sample1, sample2, active);

        return { depolarizer<Spectrum>(value) & active, pdf, bs, weight };
    }

    Spectrum eval_diffuse_reflectance(const SurfaceInteraction3f &si,
                                      Mask active) const override {
        return m_base_color->eval(si, active);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Principled BSDF :" << std::endl
            << "base_color: " << m_base_color << "," << std::endl
            << "spec_trans: " << m_spec_trans << "," << std::endl
            << "anisotropic: " << m_anisotropic << "," << std::endl
            << "roughness: " << m_roughness << "," << std::endl
            << "sheen: " << m_sheen << "," << std::endl
            << "sheen_tint: " << m_sheen_tint << "," << std::endl
            << "flatness: " << m_flatness << "," << std::endl;
        if (m_eta_specular)
            oss << "eta: " << m_eta << "," << std::endl;
        else
            oss << "specular: " << m_specular << "," << std::endl;
        oss << "clearcoat: " << m_clearcoat << "," << std::endl
            << "clearcoat_gloss: " << m_clearcoat_gloss << "," << std::endl
            << "metallic: " << m_metallic << "," << std::endl
            << "spec_tint: " << m_spec_tint << "," << std::endl;

        return oss.str();
    }
    MI_DECLARE_CLASS(Principled)
private:
    /// Material parameters at a surface position, fetched once per query
    struct Params {
        Float roughness, anisotropic, metallic, spec_trans, clearcoat,
              clearcoat_gloss, sheen, sheen_tint, spec_tint, flatness;
        UnpolarizedSpectrum base_color;

        /// Base color normalized by its luminance
        UnpolarizedSpectrum c_tint;

        /// Weights of the BRDF and BSDF major lobes
        Float brdf, bsdf;

        /// Roughness of the main specular lobe
        Float alpha_x, alpha_y;

        /// Roughness of the clearcoat lobe
        Float clearcoat_alpha;
    };

    /// Discrete probabilities of the four sampling techniques
    struct LobeProbs {
        Float spec_reflect, spec_trans, clearcoat, diffuse;
    };

    Params eval_params(const SurfaceInteraction3f &si, Mask active) const {
        Params p;

        p.roughness       = m_roughness->eval_1(si, active);
        p.anisotropic     = m_has_anisotropic ? m_anisotropic->eval_1(si, active) : 0.0f;
        p.metallic        = m_has_metallic ? m_metallic->eval_1(si, active) : 0.0f;
        p.spec_trans      = m_has_spec_trans ? m_spec_trans->eval_1(si, active) : 0.0f;
        p.clearcoat       = m_has_clearcoat ? m_clearcoat->eval_1(si, active) : 0.0f;
        p.clearcoat_gloss = m_has_clearcoat ? m_clearcoat_gloss->eval_1(si, active) : 0.0f;
        p.sheen           = m_has_sheen ? m_sheen->eval_1(si, active) : 0.0f;
        p.sheen_tint      = m_has_sheen_tint ? m_sheen_tint->eval_1(si, active) : 0.0f;
        p.spec_tint       = m_has_spec_tint ? m_spec_tint->eval_1(si, active) : 0.0f;
        p.flatness        = m_has_flatness ? m_flatness->eval_1(si, active) : 0.0f;
        p.base_color      = m_base_color->eval(si, active);

        p.c_tint = 1.0f;
        if (m_has_spec_tint || m_has_sheen_tint) {
            Float lum = mitsuba::luminance(p.base_color, si.wavelengths);
            p.c_tint = dr::select(lum > 0.0f, p.base_color / lum, 1.0f);
        }

        p.brdf = (1.0f - p.metallic) * (1.0f - p.spec_trans);
        p.bsdf = (1.0f - p.metallic) * p.spec_trans;

        std::tie(p.alpha_x, p.alpha_y) =
            calc_dist_params(p.anisotropic, p.roughness, m_has_anisotropic);

        // Clearcoat roughness is mapped between 0.1 and 0.001
        p.clearcoat_alpha = dr::lerp(0.1f, 0.001f, p.clearcoat_gloss);

        return p;
    }

    /**
     * Lobe selection probabilities given the dielectric Fresnel term at the
     * microfacet normal of the main specular lobe.
     *
     * Inside the material, only reflection and transmission through the main
     * specular lobe are sampled. On the front side, the reflection and
     * transmission probabilities sum to a value that does not depend on the
     * Fresnel term. This matters because the pdf reconstructs the microfacet
     * normal from wo, which differs from the sampled normal for diffuse and
     * clearcoat samples.
     */
    LobeProbs lobe_probs(const Params &p, Float F_dielectric,
                         Mask front_side) const {
        LobeProbs r;

        r.spec_reflect = dr::select(
            front_side, m_spec_srate * (1.0f - p.bsdf * (1.0f - F_dielectric)),
            F_dielectric);

        r.spec_trans = m_has_spec_trans
            ? dr::select(front_side, m_spec_srate * p.bsdf * (1.0f - F_dielectric),
                         1.0f - F_dielectric)
            : 0.0f;

        // The clearcoat lobe carries 1/4 of the main specular energy
        r.clearcoat = m_has_clearcoat
            ? dr::select(front_side, 0.25f * p.clearcoat * m_clearcoat_srate, 0.0f)
            : 0.0f;

        r.diffuse = dr::select(front_side, p.brdf * m_diff_refl_srate, 0.0f);

        Float rcp_total =
            dr::rcp(r.spec_reflect + r.spec_trans + r.clearcoat + r.diffuse);

        r.spec_reflect *= rcp_total;
        r.spec_trans   *= rcp_total;
        r.clearcoat    *= rcp_total;
        r.diffuse      *= rcp_total;

        return r;
    }

    /// Jointly evaluate the BSDF times the cosine factor and the sampling pdf
    std::pair<UnpolarizedSpectrum, Float>
    eval_pdf_impl(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                  const Params &p, const Vector3f &wo, Mask active) const {
        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);

        Mask front_side = cos_theta_i > 0.0f,
             reflect    = cos_theta_i * cos_theta_o > 0.0f,
             refract    = cos_theta_i * cos_theta_o < 0.0f;

        // Relative index of refraction along the light path
        Float eta_path     = dr::select(front_side, m_eta, m_inv_eta),
              inv_eta_path = dr::select(front_side, m_inv_eta, m_eta);

        // Half vector, oriented towards the exterior of the object
        Vector3f wh = dr::normalize(
            si.wi + wo * dr::select(reflect, Float(1.0f), eta_path));
        wh = dr::mulsign(wh, Frame3f::cos_theta(wh));

        Float dot_wi_h = dr::dot(si.wi, wh),
              dot_wo_h = dr::dot(wo, wh);

        auto [F_dielectric, cos_theta_t, eta_it, eta_ti] =
            fresnel(dot_wi_h, m_eta);

        // Main specular lobe
        MicrofacetDistribution spec_distr(MicrofacetType::GGX, p.alpha_x,
                                          p.alpha_y);
        Float D    = spec_distr.eval(wh),
              G1_i = spec_distr.smith_g1(si.wi, wh),
              G1_o = spec_distr.smith_g1(wo, wh),
              G    = G1_i * G1_o;

        // Density of the half vector under visible normal sampling
        Float pdf_wh = D * G1_i * dr::abs(dot_wi_h) / dr::abs(cos_theta_i);

        // Jacobian of the half vector mapping
        Float dwh_dwo;
        if (m_has_spec_trans) {
            dwh_dwo = dr::abs(dr::select(
                reflect, dr::rcp(4.0f * dot_wo_h),
                dr::square(eta_path) * dot_wo_h /
                    dr::square(dot_wi_h + eta_path * dot_wo_h)));
        } else {
            dwh_dwo = dr::abs(dr::rcp(4.0f * dot_wo_h));
        }

        LobeProbs prob = lobe_probs(p, F_dielectric, front_side);

        UnpolarizedSpectrum value(0.0f);
        Float pdf(0.0f);

        // Main specular reflection
        Mask spec_reflect_active = active && reflect;
        if (dr::any_or<true>(spec_reflect_active)) {
            UnpolarizedSpectrum F = principled_fresnel(
                F_dielectric, dot_wi_h, dr::abs(cos_theta_t), eta_it,
                p.metallic, p.spec_tint, p.base_color, p.c_tint, front_side,
                p.bsdf, m_has_metallic, m_has_spec_tint);

            dr::masked(value, spec_reflect_active) +=
                F * D * G / (4.0f * dr::abs(cos_theta_i));
            dr::masked(pdf, spec_reflect_active) +=
                prob.spec_reflect * pdf_wh * dwh_dwo;
        }

        // Main specular transmission
        if (m_has_spec_trans) {
            // No microfacet refracts into directions that face the half
            // vector from the incident side
            Mask spec_trans_active =
                active && refract && dot_wo_h * cos_theta_o > 0.0f;

            if (dr::any_or<true>(spec_trans_active)) {
                // Account for the solid angle compression when tracing
                // radiance. This is necessary for bidirectional methods.
                Float scale = (ctx.mode == TransportMode::Radiance)
                                  ? dr::square(inv_eta_path)
                                  : Float(1.0f);

                dr::masked(value, spec_trans_active) +=
                    dr::sqrt(p.base_color) * p.bsdf *
                    dr::abs((scale * (1.0f - F_dielectric) * D * G *
                             dr::square(eta_path) * dot_wi_h * dot_wo_h) /
                            (cos_theta_i *
                             dr::square(dot_wi_h + eta_path * dot_wo_h)));
                dr::masked(pdf, spec_trans_active) +=
                    prob.spec_trans * pdf_wh * dwh_dwo;
            }
        }

        // Clearcoat: a fixed IOR 1.5 coating (F0 = 0.04) with a GTR1
        // distribution, only evaluated from the front
        if (m_has_clearcoat) {
            Mask clearcoat_active = active && reflect && front_side;

            if (dr::any_or<true>(clearcoat_active)) {
                GTR1 cc_distr(p.clearcoat_alpha);
                MicrofacetDistribution cc_g_distr(MicrofacetType::GGX, 0.25f);

                Float Fcc = dr::lerp(schlick_weight(dot_wi_h), 1.0f, 0.04f),
                      Dcc = cc_distr.eval(wh),
                      Gcc = cc_g_distr.G(si.wi, wo, wh);

                dr::masked(value, clearcoat_active) +=
                    (0.25f * p.clearcoat) * Fcc * Dcc * Gcc /
                    (4.0f * dr::abs(cos_theta_i));
                dr::masked(pdf, clearcoat_active) +=
                    prob.clearcoat * Frame3f::cos_theta(wh) * Dcc * dwh_dwo;
            }
        }

        // Diffuse, retro-reflection, fake subsurface, and sheen lobes. All
        // of them are restricted to the front side.
        Mask diffuse_active = active && reflect && front_side;
        if (dr::any_or<true>(diffuse_active)) {
            Float Fo = schlick_weight(cos_theta_o),
                  Fi = schlick_weight(cos_theta_i);

            Float f_diff = (1.0f - 0.5f * Fi) * (1.0f - 0.5f * Fo);

            // Retro reflection
            Float Rr      = 2.0f * p.roughness * dr::square(dot_wo_h),
                  f_retro = Rr * (Fo + Fi + Fo * Fi * (Rr - 1.0f)),
                  f       = f_diff + f_retro;

            if (m_has_flatness) {
                // Fake subsurface scattering based on Hanrahan-Krueger
                Float Fss90 = 0.5f * Rr,
                      Fss   = dr::lerp(1.0f, Fss90, Fo) * dr::lerp(1.0f, Fss90, Fi),
                      f_ss  = 1.25f * (Fss * (dr::rcp(cos_theta_o + cos_theta_i) - 0.5f) + 0.5f);

                f = dr::lerp(f, f_ss, p.flatness);
            }

            UnpolarizedSpectrum diffuse = p.brdf * p.base_color * (dr::InvPi<Float> * f);

            if (m_has_sheen) {
                Float Fd = schlick_weight(dot_wo_h);
                UnpolarizedSpectrum c_sheen =
                    m_has_sheen_tint ? dr::lerp(1.0f, p.c_tint, p.sheen_tint)
                                     : UnpolarizedSpectrum(1.0f);

                diffuse += p.sheen * (1.0f - p.metallic) * Fd * c_sheen;
            }

            dr::masked(value, diffuse_active) += diffuse * cos_theta_o;
            dr::masked(pdf, diffuse_active) +=
                prob.diffuse * dr::InvPi<Float> * cos_theta_o;
        }

        return { value, pdf };
    }

    std::pair<BSDFSample3f, Spectrum>
    sample_impl(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                const Params &p, Float sample1, const Point2f &sample2,
                Mask active) const {
        Float cos_theta_i = Frame3f::cos_theta(si.wi);
        Mask front_side   = cos_theta_i > 0.0f;
        BSDFSample3f bs   = dr::zeros<BSDFSample3f>();

        // Inside the material, only the transmissive lobe can be sampled
        active &= front_side || p.bsdf > 0.0f;

        // Sample a visible normal of the main specular lobe
        MicrofacetDistribution spec_distr(MicrofacetType::GGX, p.alpha_x,
                                          p.alpha_y);
        Normal3f m_spec = std::get<0>(
            spec_distr.sample(dr::mulsign(si.wi, cos_theta_i), sample2));

        auto [F_dielectric, cos_theta_t, eta_it, eta_ti] =
            fresnel(dr::dot(si.wi, m_spec), m_eta);

        LobeProbs prob = lobe_probs(p, F_dielectric, front_side);

        // Select a lobe
        Float cdf = prob.diffuse;
        Mask sample_diffuse = active && sample1 < cdf;
        Mask sample_clearcoat = m_has_clearcoat && active && sample1 >= cdf &&
                                sample1 < cdf + prob.clearcoat;
        cdf += prob.clearcoat;
        Mask sample_spec_trans = m_has_spec_trans && active && sample1 >= cdf &&
                                 sample1 < cdf + prob.spec_trans;
        cdf += prob.spec_trans;
        Mask sample_spec_reflect = active && sample1 >= cdf;

        bs.eta = 1.0f;

        // Main specular reflection
        if (dr::any_or<true>(sample_spec_reflect)) {
            Vector3f wo = reflect(si.wi, m_spec);
            dr::masked(bs.wo, sample_spec_reflect) = wo;
            dr::masked(bs.sampled_component, sample_spec_reflect) =
                m_spec_reflect_index;
            dr::masked(bs.sampled_type, sample_spec_reflect) =
                +BSDFFlags::GlossyReflection;

            // Discard reflections into the wrong hemisphere
            Mask reflect = cos_theta_i * Frame3f::cos_theta(wo) > 0.0f;
            active &= !sample_spec_reflect || reflect;
        }

        // Main specular transmission
        if (m_has_spec_trans && dr::any_or<true>(sample_spec_trans)) {
            Vector3f wo = refract(si.wi, m_spec, cos_theta_t, eta_ti);
            dr::masked(bs.wo, sample_spec_trans) = wo;
            dr::masked(bs.sampled_component, sample_spec_trans) =
                m_spec_trans_index;
            dr::masked(bs.sampled_type, sample_spec_trans) =
                +BSDFFlags::GlossyTransmission;
            dr::masked(bs.eta, sample_spec_trans) = eta_it;

            // Discard refractions into the wrong hemisphere
            Mask refract = cos_theta_i * Frame3f::cos_theta(wo) < 0.0f;
            active &= !sample_spec_trans || refract;
        }

        // Clearcoat
        if (m_has_clearcoat && dr::any_or<true>(sample_clearcoat)) {
            GTR1 cc_distr(p.clearcoat_alpha);
            Normal3f m_cc = cc_distr.sample(sample2);
            Vector3f wo   = reflect(si.wi, m_cc);
            dr::masked(bs.wo, sample_clearcoat) = wo;
            dr::masked(bs.sampled_component, sample_clearcoat) =
                m_clearcoat_index;
            dr::masked(bs.sampled_type, sample_clearcoat) =
                +BSDFFlags::GlossyReflection;

            // The clearcoat normal is not sampled relative to si.wi, so the
            // microfacet may face away from it. Such samples and reflections
            // into the wrong hemisphere are discarded.
            Mask reflect = cos_theta_i * Frame3f::cos_theta(wo) > 0.0f;
            active &= !sample_clearcoat ||
                      (reflect && dr::dot(si.wi, m_cc) * cos_theta_i > 0.0f);
        }

        // Diffuse
        if (dr::any_or<true>(sample_diffuse)) {
            Vector3f wo = warp::square_to_cosine_hemisphere(sample2);
            dr::masked(bs.wo, sample_diffuse) = wo;
            dr::masked(bs.sampled_component, sample_diffuse) = 0;
            dr::masked(bs.sampled_type, sample_diffuse) =
                +BSDFFlags::DiffuseReflection;
        }

        auto [value, pdf] = eval_pdf_impl(ctx, si, p, bs.wo, active);
        bs.pdf = pdf;
        active &= pdf > 0.0f;

        return { bs, depolarizer<Spectrum>(value / pdf) & active };
    }
    /// Parameters
    ref<Texture> m_base_color;
    ref<Texture> m_roughness;
    ref<Texture> m_anisotropic;
    ref<Texture> m_sheen;
    ref<Texture> m_sheen_tint;
    ref<Texture> m_spec_trans;
    ref<Texture> m_flatness;
    ref<Texture> m_spec_tint;
    ref<Texture> m_clearcoat;
    ref<Texture> m_clearcoat_gloss;
    ref<Texture> m_metallic;
    Float m_eta, m_inv_eta;
    Float m_specular;
    bool m_eta_specular;

    /// Sampling rates
    ScalarFloat m_diff_refl_srate;
    ScalarFloat m_spec_srate;
    ScalarFloat m_clearcoat_srate;

    /// Component indices of the optional and main specular lobes
    uint32_t m_clearcoat_index = 0, m_spec_trans_index = 0,
             m_spec_reflect_index = 0;

    /// Whether the lobes are active or not.
    bool m_has_clearcoat;
    bool m_has_sheen;
    bool m_has_spec_trans;
    bool m_has_metallic;
    bool m_has_spec_tint;
    bool m_has_sheen_tint;
    bool m_has_anisotropic;
    bool m_has_flatness;

    MI_TRAVERSE_CB(Base, m_base_color, m_roughness, m_anisotropic, m_sheen,
                   m_sheen_tint, m_spec_trans, m_flatness, m_spec_tint,
                   m_clearcoat, m_clearcoat_gloss, m_metallic, m_eta,
                   m_inv_eta, m_specular)
};

MI_EXPORT_PLUGIN(Principled)
NAMESPACE_END(mitsuba)
