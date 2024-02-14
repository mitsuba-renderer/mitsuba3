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
        m_has_metallic = get_flag("metallic", props);
        m_metallic = props.texture<Texture>("metallic", 0.0f);
        m_has_clearcoat = get_flag("clearcoat", props);
        m_clearcoat = props.texture<Texture>("clearcoat", 0.0f);
        m_clearcoat_gloss = props.texture<Texture>("clearcoat_gloss", 0.0f);
        m_spec_srate = props.get("main_specular_sampling_rate", 1.0f);
        m_clearcoat_srate = props.get("clearcoat_sampling_rate", 1.0f);
        m_diff_refl_srate = props.get("diffuse_reflectance_sampling_rate", 1.0f);

        /*Eta and specular has one to one correspondence, both of them can
         * not be specified. */
        if (props.has_property("eta") && props.has_property("specular")) {
            Throw("Specified an invalid index of refraction property  "
                  "\"%s\", either use \"eta\" or \"specular\" !");
        } else if (props.has_property("eta")) {
            m_eta_specular = true;
            m_eta = props.get<float>("eta");
            // m_eta = 1 is not plausible for transmission
            dr::masked(m_eta, m_has_spec_trans && m_eta == 1) = 1.001f;
        } else {
            m_eta_specular = false;
            m_specular = props.get<float>("specular", 0.5f);
            // zero specular is not plausible for transmission
            dr::masked(m_specular, m_has_spec_trans && m_specular == 0.f) = 1e-3f;
            m_eta = 2.0f * dr::rcp(1.0f - dr::sqrt(0.08f * m_specular)) - 1.0f;
        }

        initialize_lobes();

        dr::make_opaque(m_eta);
        if (!m_eta_specular)
            dr::make_opaque(m_specular);
    }

    void initialize_lobes() {
        // Diffuse reflection lobe
        m_components.push_back(BSDFFlags::DiffuseReflection |
                               BSDFFlags::FrontSide);

        // Clearcoat lobe
        if (m_has_clearcoat) {
            m_components.push_back(BSDFFlags::GlossyReflection |
                                   BSDFFlags::FrontSide);
        }

        // Specular transmission lobe
        if (m_has_spec_trans) {
            uint32_t f = BSDFFlags::GlossyTransmission | BSDFFlags::FrontSide |
                         BSDFFlags::BackSide | BSDFFlags::NonSymmetric;
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
        callback->put_object("clearcoat",       m_clearcoat.get(),       +ParamFlags::Differentiable);
        callback->put_object("clearcoat_gloss", m_clearcoat_gloss.get(), +ParamFlags::Differentiable);
        callback->put_object("metallic",        m_metallic.get(),        +ParamFlags::Differentiable);

        callback->put_parameter("main_specular_sampling_rate",       m_spec_srate,      +ParamFlags::NonDifferentiable);
        callback->put_parameter("clearcoat_sampling_rate",           m_clearcoat_srate, +ParamFlags::NonDifferentiable);
        callback->put_parameter("diffuse_reflectance_sampling_rate", m_diff_refl_srate, +ParamFlags::NonDifferentiable);

        if (m_eta_specular) //Only one of them traversed! (based on xml file)
            callback->put_parameter("eta",      m_eta,      ParamFlags::Differentiable | ParamFlags::Discontinuous);
        else
            callback->put_parameter("specular", m_specular, ParamFlags::Differentiable | ParamFlags::Discontinuous);

        callback->put_object("roughness",       m_roughness.get(),   ParamFlags::Differentiable | ParamFlags::Discontinuous);
        callback->put_object("base_color",      m_base_color.get(),  +ParamFlags::Differentiable);
        callback->put_object("anisotropic",     m_anisotropic.get(), +ParamFlags::Differentiable);
        callback->put_object("spec_tint",       m_spec_tint.get(),   +ParamFlags::Differentiable);
        callback->put_object("sheen",           m_sheen.get(),       +ParamFlags::Differentiable);
        callback->put_object("sheen_tint",      m_sheen_tint.get(),  +ParamFlags::Differentiable);
        callback->put_object("spec_trans",      m_spec_trans.get(),  +ParamFlags::Differentiable);
        callback->put_object("flatness",        m_flatness.get(),    +ParamFlags::Differentiable);
    }

    void
    parameters_changed(const std::vector<std::string> &keys = {}) override {
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

        if (!m_eta_specular && string::contains(keys, "specular")) {
            /* Specular=0 is corresponding to eta=1 which is not plausible
               for transmission. */
            dr::masked(m_specular, m_specular == 0.0f) = 1e-3f;
            m_eta = 2.0f * dr::rcp(1.0f - dr::sqrt(0.08f * m_specular)) - 1.0f;
        }

        if (m_eta_specular && string::contains(keys, "eta")) {
            // Eta = 1 is not plausible for transmission.
            dr::masked(m_eta, m_eta == 1.0f) = 1.001f;
        }

        initialize_lobes();

        dr::make_opaque(m_eta);
        if (!m_eta_specular)
            dr::make_opaque(m_specular);
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
        Float anisotropic = m_has_anisotropic ? m_anisotropic->eval_1(si, active) : 0.0f,
        roughness = m_roughness->eval_1(si, active),
        spec_trans = m_has_spec_trans ? m_spec_trans->eval_1(si, active) : 0.0f,
        metallic = m_has_metallic ? m_metallic->eval_1(si, active) : 0.0f,
        clearcoat = m_has_clearcoat ? m_clearcoat->eval_1(si, active) : 0.0f;

        // Weights of BSDF and BRDF major lobes
        Float brdf = (1.0f - metallic) * (1.0f - spec_trans),
        bsdf = m_has_spec_trans ? (1.0f - metallic) * spec_trans : 0.0f;

        // Mask for incident side. (wi.z<0)
        Mask front_side = cos_theta_i > 0.0f;

        // Defining main specular reflection distribution
        auto [ax, ay] = calc_dist_params(anisotropic, roughness,m_has_anisotropic);
        MicrofacetDistribution spec_distr(MicrofacetType::GGX, ax, ay);
        Normal3f m_spec = std::get<0>(
                spec_distr.sample(dr::mulsign(si.wi, cos_theta_i), sample2));

        // Fresnel coefficient for the main specular.
        auto [F_spec_dielectric, cos_theta_t, eta_it, eta_ti] =
                fresnel(dr::dot(si.wi, m_spec), m_eta);

        // If BSDF major lobe is turned off, we do not sample the inside
        // case.
        active &= (front_side || (bsdf > 0.0f));

        // Probability definitions
        /* Inside  the material, just microfacet Reflection and
           microfacet Transmission is sampled. */
        Float prob_spec_reflect = dr::select(
                front_side,
                m_spec_srate * (1.0f - bsdf * (1.0f - F_spec_dielectric)),
                F_spec_dielectric);
        Float prob_spec_trans =
                m_has_spec_trans
                ? dr::select(front_side,
                             m_spec_srate * bsdf * (1.0f - F_spec_dielectric),
                             (1.0f - F_spec_dielectric))
                             : 0.0f;
        // Clearcoat has 1/4 of the main specular reflection energy.
        Float prob_clearcoat =
                m_has_clearcoat
                ? dr::select(front_side, 0.25f * clearcoat * m_clearcoat_srate,
                             0.0f)
                             : 0.0f;
        Float prob_diffuse = dr::select(front_side, brdf * m_diff_refl_srate, 0.0f);

        // Normalizing the probabilities.
        Float rcp_tot_prob = dr::rcp(prob_spec_reflect + prob_spec_trans +
                prob_clearcoat + prob_diffuse);
        prob_spec_trans *= rcp_tot_prob;
        prob_clearcoat *= rcp_tot_prob;
        prob_diffuse *= rcp_tot_prob;

        // Sampling mask definitions
        Float curr_prob(0.0f);
        Mask sample_diffuse = active && (sample1 < prob_diffuse);
        curr_prob += prob_diffuse;
        Mask sample_clearcoat = m_has_clearcoat && active &&
                (sample1 >= curr_prob) &&
                (sample1 < curr_prob + prob_clearcoat);
        curr_prob += prob_clearcoat;
        Mask sample_spec_trans = m_has_spec_trans && active &&
                (sample1 >= curr_prob) &&
                (sample1 < curr_prob + prob_spec_trans);
        curr_prob += prob_spec_trans;
        Mask sample_spec_reflect = active && (sample1 >= curr_prob);

        // Eta will be changed in transmission.
        bs.eta = 1.0f;

        // Main specular reflection sampling
        if (dr::any_or<true>(sample_spec_reflect)) {
            Vector3f wo                            = reflect(si.wi, m_spec);
            dr::masked(bs.wo, sample_spec_reflect) = wo;
            dr::masked(bs.sampled_component, sample_spec_reflect) = 3;
            dr::masked(bs.sampled_type, sample_spec_reflect) =
                    +BSDFFlags::GlossyReflection;

            /* Filter the cases where macro and micro surfaces do not agree
             on the same side and reflection is not successful*/
            Mask reflect = cos_theta_i * Frame3f::cos_theta(wo) > 0.0f;
            active &=
                    (!sample_spec_reflect ||
                    (mac_mic_compatibility(Vector3f(m_spec),
                                           si.wi, wo, cos_theta_i, true) &&
                    reflect));
        }
        // The main specular transmission sampling
        if (m_has_spec_trans && dr::any_or<true>(sample_spec_trans)) {
            Vector3f wo = refract(si.wi, m_spec, cos_theta_t, eta_ti);
            dr::masked(bs.wo, sample_spec_trans)                = wo;
            dr::masked(bs.sampled_component, sample_spec_trans) = 2;
            dr::masked(bs.sampled_type, sample_spec_trans) =
                    +BSDFFlags::GlossyTransmission;
            dr::masked(bs.eta, sample_spec_trans) = eta_it;

            /* Filter the cases where macro and micro surfaces do not agree
             on the same side and refraction is successful. */
            Mask refract = cos_theta_i * Frame3f::cos_theta(wo) < 0.0f;
            active &= (!sample_spec_trans ||
                    (mac_mic_compatibility(Vector3f(m_spec),
                                           si.wi, wo, cos_theta_i,
                                           false) &&
                                           refract));
        }
        // The secondary specular reflection sampling (clearcoat)
        if (m_has_clearcoat && dr::any_or<true>(sample_clearcoat)) {
            Float clearcoat_gloss = m_clearcoat_gloss->eval_1(si, active);

            // Clearcoat roughness is mapped between 0.1 and 0.001.
            GTR1 cc_dist(dr::lerp(0.1f, 0.001f, clearcoat_gloss));
            Normal3f m_cc                = cc_dist.sample(sample2);
            Vector3f wo                         = reflect(si.wi, m_cc);
            dr::masked(bs.wo, sample_clearcoat) = wo;
            dr::masked(bs.sampled_component, sample_clearcoat) = 1;
            dr::masked(bs.sampled_type, sample_clearcoat) =
                    +BSDFFlags::GlossyReflection;

            /* Filter the cases where macro and microfacets do not agree on
             the same side and reflection is not successful. */
            Mask reflect = cos_theta_i * Frame3f::cos_theta(wo) > 0.0f;
            active &= (!sample_clearcoat ||
                    (mac_mic_compatibility(Vector3f(m_cc),
                                           si.wi, wo,
                                           cos_theta_i, true) &&
                                           reflect));
        }
        // Cosine hemisphere reflection sampling
        if (dr::any_or<true>(sample_diffuse)) {
            Vector3f wo = warp::square_to_cosine_hemisphere(sample2);
            dr::masked(bs.wo, sample_diffuse)                = wo;
            dr::masked(bs.sampled_component, sample_diffuse) = 0;
            dr::masked(bs.sampled_type, sample_diffuse) =
                    +BSDFFlags::DiffuseReflection;
            Mask reflect = cos_theta_i * Frame3f::cos_theta(wo) > 0.0f;
            active &= (!sample_diffuse || reflect);
        }

        bs.pdf = pdf(ctx, si, bs.wo, active);
        active &= bs.pdf > 0.0f;
        Spectrum result = eval(ctx, si, bs.wo, active);
        return { bs, result / bs.pdf & active };
    }

    Spectrum eval(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                  const Vector3f &wo, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        Float cos_theta_i = Frame3f::cos_theta(si.wi);
        // Ignore perfectly grazing configurations
        active &= cos_theta_i != 0.0f;

        if (unlikely(dr::none_or<false>(active)))
            return 0.0f;

        // Store the weights.
        Float anisotropic = m_has_anisotropic ? m_anisotropic->eval_1(si, active) : 0.0f,
              roughness = m_roughness->eval_1(si, active),
              flatness = m_has_flatness ? m_flatness->eval_1(si, active) : 0.0f,
              spec_trans = m_has_spec_trans ? m_spec_trans->eval_1(si, active) : 0.0f,
              metallic = m_has_metallic ? m_metallic->eval_1(si, active) : 0.0f,
              clearcoat = m_has_clearcoat ? m_clearcoat->eval_1(si, active) : 0.0f,
              sheen = m_has_sheen ? m_sheen->eval_1(si, active) : 0.0f;
        UnpolarizedSpectrum base_color = m_base_color->eval(si, active);

        // Weights for BRDF and BSDF major lobes.
        Float brdf = (1.0f - metallic) * (1.0f - spec_trans),
        bsdf = (1.0f - metallic) * spec_trans;

        Float cos_theta_o = Frame3f::cos_theta(wo);

        // Reflection and refraction masks.
        Mask reflect = cos_theta_i * cos_theta_o > 0.0f;
        Mask refract = cos_theta_i * cos_theta_o < 0.0f;

        // Masks for the side of the incident ray (wi.z<0)
        Mask front_side = cos_theta_i > 0.0f;
        Float inv_eta   = dr::rcp(m_eta);

        // Eta value w.r.t. ray instead of the object.
        Float eta_path     = dr::select(front_side, m_eta, inv_eta);
        Float inv_eta_path = dr::select(front_side, inv_eta, m_eta);

        // Main specular reflection and transmission lobe
        auto [ax, ay] = calc_dist_params(anisotropic, roughness,m_has_anisotropic);
        MicrofacetDistribution spec_dist(MicrofacetType::GGX, ax, ay);

        // Halfway vector
        Vector3f wh =
                dr::normalize(si.wi + wo * dr::select(reflect, 1.0f, eta_path));

        // Make sure that the halfway vector points outwards the object
        wh = dr::mulsign(wh, Frame3f::cos_theta(wh));

        // Dielectric Fresnel
        auto [F_spec_dielectric, cos_theta_t, eta_it, eta_ti] =
                fresnel(dr::dot(si.wi, wh), m_eta);

        Mask reflection_compatibilty =
                mac_mic_compatibility(wh, si.wi, wo, cos_theta_i, true);
        Mask refraction_compatibilty =
                mac_mic_compatibility(wh, si.wi, wo, cos_theta_i, false);
        // Masks for evaluating the lobes.
        // Specular reflection mask
        Mask spec_reflect_active = active && reflect &&
                reflection_compatibilty &&
                (F_spec_dielectric > 0.0f);

        // Clearcoat mask
        Mask clearcoat_active = m_has_clearcoat && active &&
                (clearcoat > 0.0f) && reflect &&
                reflection_compatibilty && front_side;

        // Specular transmission mask
        Mask spec_trans_active = m_has_spec_trans && active && (bsdf > 0.0f) &&
                refract && refraction_compatibilty &&
                (F_spec_dielectric < 1.0f);

        // Diffuse, retro and fake subsurface mask
        Mask diffuse_active = active && (brdf > 0.0f) && reflect && front_side;

        // Sheen mask
        Mask sheen_active = m_has_sheen && active && (sheen > 0.0f) &&
                reflect && (1.0f - metallic > 0.0f) && front_side;

        // Evaluate the microfacet normal distribution
        Float D = spec_dist.eval(wh);

        // Smith's shadowing-masking function
        Float G = spec_dist.G(si.wi, wo, wh);

        // Initialize the final BSDF value.
        UnpolarizedSpectrum value(0.0f);

        // Main specular reflection evaluation
        if (dr::any_or<true>(spec_reflect_active)) {
            // No need to calculate luminance if there is no color tint.
            Float lum = m_has_spec_tint
                    ? mitsuba::luminance(base_color, si.wavelengths)
                    : 1.0f;
            Float spec_tint =
                    m_has_spec_tint ? m_spec_tint->eval_1(si, active) : 0.0f;

            // Fresnel term
            UnpolarizedSpectrum F_principled = principled_fresnel(
                    F_spec_dielectric, metallic, spec_tint, base_color, lum,
                    dr::dot(si.wi, wh), front_side, bsdf,m_eta,m_has_metallic,
                    m_has_spec_tint);

            // Adding the specular reflection component
            dr::masked(value, spec_reflect_active) +=
                    F_principled * D * G / (4.0f * dr::abs(cos_theta_i));
        }

        // Main specular transmission evaluation
        if (m_has_spec_trans && dr::any_or<true>(spec_trans_active)) {

            /* Account for the solid angle compression when tracing
               radiance. This is necessary for bidirectional methods. */
            Float scale = (ctx.mode == TransportMode::Radiance)
                    ? dr::square(inv_eta_path)
                    : Float(1.0f);

            // Adding the specular transmission component
            dr::masked(value, spec_trans_active) +=
                    dr::sqrt(base_color) * bsdf *
                    dr::abs((scale * (1.0f - F_spec_dielectric) * D * G * eta_path *
                    eta_path * dr::dot(si.wi, wh) * dr::dot(wo, wh)) /
                    (cos_theta_i * dr::square(dr::dot(si.wi, wh) +
                    eta_path * dr::dot(wo, wh))));
        }

        // Secondary isotropic specular reflection.
        if (m_has_clearcoat && dr::any_or<true>(clearcoat_active)) {
            Float clearcoat_gloss = m_clearcoat_gloss->eval_1(si, active);

            // Clearcoat lobe uses the schlick approximation for Fresnel
            // term.
            Float Fcc = calc_schlick<Float>(0.04f, dr::dot(si.wi, wh),m_eta);

            /* Clearcoat lobe uses GTR1 distribution. Roughness is mapped
             * between 0.1 and 0.001. */
            GTR1 mfacet_dist(dr::lerp(0.1f, 0.001f, clearcoat_gloss));
            Float Dcc = mfacet_dist.eval(wh);

            // Shadowing shadowing-masking term
            Float G_cc = clearcoat_G(si.wi, wo, wh, Float(0.25f));

            // Adding the clearcoat component.
            dr::masked(value, clearcoat_active) +=
                    (clearcoat * 0.25f) * Fcc * Dcc * G_cc * dr::abs(cos_theta_o);
        }

        // Evaluation of diffuse, retro reflection, fake subsurface and
        // sheen.
        if (dr::any_or<true>(diffuse_active)) {
            Float Fo = schlick_weight(dr::abs(cos_theta_o)),
            Fi = schlick_weight(dr::abs(cos_theta_i));

            // Diffuse
            Float f_diff = (1.0f - 0.5f * Fi) * (1.0f - 0.5f * Fo);

            Float cos_theta_d = dr::dot(wh, wo);
            Float Rr          = 2.0f * roughness * dr::square(cos_theta_d);

            // Retro reflection
            Float f_retro = Rr * (Fo + Fi + Fo * Fi * (Rr - 1.0f));

            if (m_has_flatness) {
                /* Fake subsurface implementation based on Hanrahan Krueger
                   Fss90 used to "flatten" retro reflection based on
                   roughness.*/
                Float Fss90 = Rr / 2.0f;
                Float Fss =
                        dr::lerp(1.0f, Fss90, Fo) * dr::lerp(1.0f, Fss90, Fi);

                Float f_ss = 1.25f * (Fss * (1.0f / (dr::abs(cos_theta_o) +
                        dr::abs(cos_theta_i)) -
                                0.5f) +
                                        0.5f);

                // Adding diffuse, retro and fake subsurface evaluation.
                dr::masked(value, diffuse_active) +=
                        brdf * dr::abs(cos_theta_o) * base_color *
                        dr::InvPi<Float> *
                        (dr::lerp(f_diff + f_retro, f_ss, flatness));
            } else {
                // Adding diffuse, retro evaluation. (no fake ss.)
                dr::masked(value, diffuse_active) +=
                        brdf * dr::abs(cos_theta_o) * base_color *
                        dr::InvPi<Float> * (f_diff + f_retro);
            }
            // Sheen evaluation
            if (m_has_sheen && dr::any_or<true>(sheen_active)) {
                Float Fd = schlick_weight(dr::abs(cos_theta_d));

                // Tint the sheen evaluation towards the base color.
                if (m_has_sheen_tint) {
                    Float sheen_tint = m_sheen_tint->eval_1(si, active);

                    // Luminance evaluation
                    Float lum = mitsuba::luminance(base_color, si.wavelengths);

                    // Normalize color with luminance and tint the result.
                    UnpolarizedSpectrum c_tint =
                            dr::select(lum > 0.0f, base_color / lum, 1.0f);
                    UnpolarizedSpectrum c_sheen = dr::lerp(1.0f, c_tint, sheen_tint);

                    // Adding sheen evaluation with tint.
                    dr::masked(value, sheen_active) +=
                            sheen * (1.0f - metallic) * Fd * c_sheen *
                            dr::abs(cos_theta_o);
                } else {
                    // Adding sheen evaluation without tint.
                    dr::masked(value, sheen_active) +=
                            sheen * (1.0f - metallic) * Fd * dr::abs(cos_theta_o);
                }
            }
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
                        m_has_spec_trans ? m_spec_trans->eval_1(si, active) : 0.0f;
        Float metallic = m_has_metallic ? m_metallic->eval_1(si, active) : 0.0f,
        clearcoat =
                m_has_clearcoat ? m_clearcoat->eval_1(si, active) : 0.0f;

        // BRDF and BSDF major lobe weights
        Float brdf = (1.0f - metallic) * (1.0f - spec_trans),
        bsdf = (1.0f - metallic) * spec_trans;

        // Masks if incident direction is inside (wi.z<0)
        Mask front_side = cos_theta_i > 0.0f;

        // Eta w.r.t. light path.
        Float eta_path    = dr::select(front_side, m_eta, dr::rcp(m_eta));
        Float cos_theta_o = Frame3f::cos_theta(wo);

        Mask reflect = cos_theta_i * cos_theta_o > 0.0f;
        Mask refract = cos_theta_i * cos_theta_o < 0.0f;

        // Halfway vector calculation
        Vector3f wh = dr::normalize(
                si.wi + wo * dr::select(reflect, Float(1.0f), eta_path));

        // Make sure that the halfway vector points outwards the object
        wh = dr::mulsign(wh, Frame3f::cos_theta(wh));

        // Main specular distribution for reflection and transmission.
        auto [ax, ay] = calc_dist_params(anisotropic, roughness,m_has_anisotropic);
        MicrofacetDistribution spec_distr(MicrofacetType::GGX, ax, ay);

        // Dielectric Fresnel calculation
        auto [F_spec_dielectric, cos_theta_t, eta_it, eta_ti] =
                fresnel(dr::dot(si.wi, wh), m_eta);

        // Defining the probabilities
        Float prob_spec_reflect = dr::select(
                front_side,
                m_spec_srate * (1.0f - bsdf * (1.0f - F_spec_dielectric)),
                F_spec_dielectric);
        Float prob_spec_trans =
                m_has_spec_trans
                ? dr::select(front_side,
                             m_spec_srate * bsdf * (1.0f - F_spec_dielectric),
                             (1.0f - F_spec_dielectric))
                             : 0.0f;
        Float prob_clearcoat =
                m_has_clearcoat
                ? dr::select(front_side, 0.25f * clearcoat * m_clearcoat_srate,
                             0.0f)
                             : 0.0f;
        Float prob_diffuse =
                dr::select(front_side, brdf * m_diff_refl_srate, 0.f);

        // Normalizing the probabilities.
        Float rcp_tot_prob = dr::rcp(prob_spec_reflect + prob_spec_trans +
                prob_clearcoat + prob_diffuse);
        prob_spec_reflect *= rcp_tot_prob;
        prob_spec_trans *= rcp_tot_prob;
        prob_clearcoat *= rcp_tot_prob;
        prob_diffuse *= rcp_tot_prob;

        /* Calculation of dwh/dwo term. Different for reflection and
         transmission. */
        Float dwh_dwo_abs;
        if (m_has_spec_trans) {
            Float dot_wi_h = dr::dot(si.wi, wh);
            Float dot_wo_h = dr::dot(wo, wh);
            dwh_dwo_abs    = dr::abs(
                    dr::select(reflect, dr::rcp(4.0f * dot_wo_h),
                               (dr::square(eta_path) * dot_wo_h) /
                               dr::square(dot_wi_h + eta_path * dot_wo_h)));
        } else {
            dwh_dwo_abs = dr::abs(dr::rcp(4.0f * dr::dot(wo, wh)));
        }

        // Initializing the final pdf value.
        Float pdf(0.0f);

        // Macro-micro surface compatibility mask for reflection.
        Mask mfacet_reflect_macmic =
                mac_mic_compatibility(wh, si.wi, wo, cos_theta_i, true) && reflect;

        // Adding main specular reflection pdf
        dr::masked(pdf, mfacet_reflect_macmic) +=
                prob_spec_reflect *
                spec_distr.pdf(dr::mulsign(si.wi, cos_theta_i), wh) * dwh_dwo_abs;
        // Adding cosine hemisphere reflection pdf
        dr::masked(pdf, reflect) +=
                prob_diffuse * warp::square_to_cosine_hemisphere_pdf(wo);
        // Main specular transmission
        if (m_has_spec_trans) {
            // Macro-micro surface mask for transmission.
            Mask mfacet_trans_macmic =
                    mac_mic_compatibility(wh, si.wi, wo, cos_theta_i, false) &&
                    refract;

            // Adding main specular transmission pdf
            dr::masked(pdf, mfacet_trans_macmic) +=
                    prob_spec_trans *
                    spec_distr.pdf(dr::mulsign(si.wi, cos_theta_i), wh) *
                    dwh_dwo_abs;
        }
        // Adding the secondary specular reflection pdf.(clearcoat)
        if (m_has_clearcoat) {
            Float clearcoat_gloss = m_clearcoat_gloss->eval_1(si, active);
            GTR1 cc_dist(dr::lerp(0.1f, 0.001f, clearcoat_gloss));
            dr::masked(pdf, mfacet_reflect_macmic) +=
                    prob_clearcoat * cc_dist.pdf(wh) * dwh_dwo_abs;
        }
        return pdf;
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
    MI_DECLARE_CLASS()
private:
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
    Float m_eta;
    Float m_specular;
    bool m_eta_specular;

    /// Sampling rates
    ScalarFloat m_diff_refl_srate;
    ScalarFloat m_spec_srate;
    ScalarFloat m_clearcoat_srate;

    /// Whether the lobes are active or not.
    bool m_has_clearcoat;
    bool m_has_sheen;
    bool m_has_spec_trans;
    bool m_has_metallic;
    bool m_has_spec_tint;
    bool m_has_sheen_tint;
    bool m_has_anisotropic;
    bool m_has_flatness;
};

MI_IMPLEMENT_CLASS_VARIANT(Principled, BSDF)
MI_EXPORT_PLUGIN(Principled, "The Principled Material")
NAMESPACE_END(mitsuba)
