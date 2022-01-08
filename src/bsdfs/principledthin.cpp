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

NAMESPACE_BEGIN(mitsuba)

<<<<<<< HEAD
=======

>>>>>>> 582ed20d8218fe745bd6238b19df72dc0e546a34
/**!
.. _bsdf-principledthin:

The Thin Principled BSDF (:monosp:`principledthin`)
-----------------------------------------------------
.. pluginparameters::

 * - base_color
   - |spectrum| or |texture|
<<<<<<< HEAD
   - The color of the material. (Default: 0.5)
 * - roughness
   - |float| or |texture|
   - Controls the roughness parameter of the main specular lobes. (Default: 0.5)
 * - anisotropic
   - |float| or |texture|
   - Controls the degree of anisotropy. (0.0: isotropic material) (Default: 0.0)
=======
   - The color of the material. (Default:0.5)
 * - roughness
   - |float| or |texture|
   - Controls the roughness parameter of the main specular lobes. (Default:0.5)
 * - anisotropic
   - |float| or |texture|
   - Controls the degree of anisotropy. (0.0 : isotropic material) (Default:0.0)
>>>>>>> 582ed20d8218fe745bd6238b19df72dc0e546a34
 * - spec_trans
   - |texture| or |float|
   - Blends diffuse and specular responses. (1.0: only
     specular response, 0.0 : only diffuse response.)(Default: 0.0)
 * - eta
   - |float| or |texture|
<<<<<<< HEAD
   - Interior IOR/Exterior IOR (Default: 1.5)
 * - spec_tint
   - |texture| or |float|
   - The fraction of `base_color` tint applied onto the dielectric reflection
     lobe. (Default: 0.0)
 * - sheen
   - |float| or |texture|
   - The rate of the sheen lobe. (Default: 0.0)
 * - sheen_tint
   - |float| or |texture|
   - The fraction of `base_color` tint applied onto the sheen lobe. (Default: 0.0)
=======
   - Interior IOR/Exterior IOR (Default:1.5)
 * - spec_tint
   - |texture| or |float|
   - The fraction of `base_color` tint applied onto the dielectric reflection
     lobe. (Default:0.0)
 * - sheen
   - |float| or |texture|
   - The rate of the sheen lobe. (Default:0.0)
 * - sheen_tint
   - |float| or |texture|
   - The fraction of `base_color` tint applied onto the sheen lobe. (Default:0.0)
>>>>>>> 582ed20d8218fe745bd6238b19df72dc0e546a34
 * - flatness
   - |float| or |texture|
   - Blends between the diffuse response and fake subsurface approximation based
     on Hanrahan-Krueger approximation. (0.0:only diffuse response, 1.0:only
<<<<<<< HEAD
     fake subsurface scattering.) (Default: 0.0)
 * - diff_trans
   - |texture| or |float|
   - The fraction that the energy of diffuse reflection is given to the
     transmission. (0.0: only diffuse reflection, 2.0: only diffuse
     transmission) (Default:0.0)
 * - diffuse_reflectance_sampling_rate
   - |float|
   - The rate of the cosine hemisphere reflection in sampling. (Default: 1.0)
 * - specular_reflectance_sampling_rate
   - |float|
   - The rate of the main specular reflection in sampling. (Default: 1.0)
 * - specular_transmittance_sampling_rate
   - |float|
   - The rate of the main specular transmission in sampling. (Default: 1.0)
 * - diffuse_transmittance_sampling_rate
   - |float|
   - The rate of the cosine hemisphere transmission in sampling. (Default: 1.0)
=======
     fake subsurface scattering.) (Default:0.0)
 * - diff_trans
   - |texture| or |float|
   - The fraction that the energy of diffuse reflection is given to the
     transmission. (0.0: only diffuse reflection, 2.0 : only diffuse
     transmission) (Default:0.0)
 * - diffuse_reflectance_sampling_rate
   - |float|
   - The rate of the cosine hemisphere reflection in sampling. (Default:1.0)
 * - specular_reflectance_sampling_rate
   - |float|
   - The rate of the main specular reflection in sampling. (Default:1.0)
 * - specular_transmittance_sampling_rate
   - |float|
   - The rate of the main specular transmission in sampling. (Default:1.0)
 * - diffuse_transmittance_sampling_rate
   - |float|
   - The rate of the cosine hemisphere transmission in sampling. (Default:1.0)
>>>>>>> 582ed20d8218fe745bd6238b19df72dc0e546a34

The thin principled BSDF is a complex BSDF which is designed by approximating
some features of thin, translucent materials. The implementation is based on
the papers *Physically Based Shading at Disney* :cite:`Disney2012` and
*Extending the Disney BRDF to a BSDF with Integrated Subsurface Scattering*
:cite:`Disney2015` by Brent Burley.

Images below show how the input parameters affect the appearance of the objects
while one of the parameters is changed for each row.

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/thin_disney_blend.png
    :caption: Blending of parameters
.. subfigend::
    :label: fig-blend-principledthin

You can see the general structure of the BSDF below.

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/bsdf/thin_disney.png
    :caption: The general structure of the thin principled BSDF
.. subfigend::
    :label: fig-structure-thin

The following XML snippet describes a material definition for
:monosp:`principledthin` material:

.. code-block:: xml
    :name: principledthin

    <bsdf type="principledthin">
        <rgb name="base_color" value="0.7,0.1,0.1 "/>
        <float name="roughness" value="0.15" />
        <float name="spec_tint" value="0.1" />
        <float name="anisotropic" value="0.5" />
        <float name="spec_trans" value="0.8" />
        <float name="diff_trans" value="0.3" />
        <float name="eta" value="1.33" />
        <float name="spec_tint" value="0.4" />
    </bsdf>

All of the parameters, except sampling rates, `diff_trans` and
<<<<<<< HEAD
`eta`, should take values between 0.0 and 1.0. The range of
`diff_trans` is 0.0 to 2.0.
=======
`eta`, should take values between 0.0 and 1.0 . The range of
`diff_trans` is 0.0 to 2.0 .
>>>>>>> 582ed20d8218fe745bd6238b19df72dc0e546a34
 */
template <typename Float, typename Spectrum>
class PrincipledThin final : public BSDF<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(BSDF, m_flags, m_components)
    MTS_IMPORT_TYPES(Texture, MicrofacetDistribution)

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

        // Diffuse reflection lobe
        m_components.push_back(BSDFFlags::DiffuseReflection |
                               BSDFFlags::FrontSide | BSDFFlags::BackSide);
        // Specular diffuse lobe
        m_components.push_back(BSDFFlags::DiffuseTransmission |
                               BSDFFlags::FrontSide | BSDFFlags::BackSide);
        // Specular transmission lobe
        m_components.push_back(BSDFFlags::GlossyTransmission |
                               BSDFFlags::FrontSide | BSDFFlags::BackSide |
                               BSDFFlags::Anisotropic);
        // Main specular reflection lobe
        m_components.push_back(BSDFFlags::GlossyReflection |
                               BSDFFlags::FrontSide | BSDFFlags::BackSide |
                               BSDFFlags::Anisotropic);

        m_flags = m_components[0] | m_components[1] | m_components[2] |
                  m_components[3];
        ek::set_attr(this, "flags", m_flags);

        parameters_changed();
    }

    // Helper Functions
    /**
     * \brief Get the flag which determines whether the corresponding
     * feature is going to be implemented or not.
     * \param name
     *     Name of the feature.
     * \param props
     *     Given properties.
     * \return the flag of the feature.
     */
    bool get_flag(std::string name, const Properties &props) {
        if (props.has_property(name)) {
            if (props.type(name) == Properties::Type::Float &&
            std::stof(props.as_string(name)) == 0.0f)
                return false;
            else
                return true;
        } else {
            return false;
        }
    }

    /**
     * \brief Computes the schlick weight for Fresnel Schlick approximation.
     * \param cos_i
     *     Incident angle of the ray based on microfacet normal.
     * \return schlick weight
     */
    Float schlick_weight(Float cos_i) const {
        Float m = ek::clamp(1.0f - cos_i, 0.0f, 1.0f);
        return ek::sqr(ek::sqr(m)) * m;
    }

    /**
     * \brief Schlick Approximation for Fresnel Reflection coefficient F = R0 +
     * (1-R0) (1-cos^5(i)). Transmitted ray's angle should be used for eta<1.
     * \param R0
     *     Incident specular. (Fresnel term when incident ray is aligned with
     *     the surface normal.)
     * \param cos_theta_i
     *     Incident angle of the ray based on microfacet normal.
     * \param eta
     *     Relative index of refraction.
     * \return Schlick approximation result.
     */
    template <typename T> T calc_schlick(T R0, Float cos_theta_i, Float eta) const {
        Mask outside_mask = cos_theta_i >= 0.0f;
        Float rcp_eta     = ek::rcp(eta),
              eta_it      = ek::select(outside_mask, eta, rcp_eta),
              eta_ti      = ek::select(outside_mask, rcp_eta, eta);

        Float cos_theta_t_sqr = ek::fnmadd(
            ek::fnmadd(cos_theta_i, cos_theta_i, 1.0f), ek::sqr(eta_ti), 1.0f);

        Float cos_theta_t = ek::safe_sqrt(cos_theta_t_sqr);

        return ek::select(
            eta_it > 1.f,
            ek::lerp(schlick_weight(ek::abs(cos_theta_i)), 1.0f, R0),
            ek::lerp(schlick_weight(cos_theta_t), 1.0f, R0));
    }

    /**
     * \brief Approximation of incident specular based on relative index of
     * refraction.
     * \param eta
     *     Relative index of refraction.
     * \return Incident specular
     */
    Float schlick_R0_eta(Float eta) const {
        return ek::sqr((eta - 1.0f) / (eta + 1.0f));
    }

     /**
     * \brief  Modified Fresnel function for thin film approximation. It
     * calculates the tinted Fresnel factor with Schlick Approximation.
     * \param F_dielectric
     *     True dielectric response.
     * \param spec_tint
     *     Specular tint weight.
     * \param base_color
     *     Base color of the material.
     * \param lum
     *     Luminance of the base color.
     * \param cos_theta_i
     *     Incident angle of the ray based on microfacet normal.
     * \param eta_t
     *     Relative index of Refraction of the thin Film
     * \return Fresnel term of the thin BSDF with normal and tinted response
     * combined.
     */
    UnpolarizedSpectrum
    thin_fresnel(const Float &F_dielectric, const Float &spec_tint,
                 const UnpolarizedSpectrum &base_color, const Float &lum,
                 const Float &cos_theta_i, const Float &eta_t) const {
        UnpolarizedSpectrum F_schlick(0.0f);
        // Tinted dielectric component based on Schlick.
        if (m_has_spec_tint) {
            auto c_tint       = ek::select(lum > 0.0f, base_color / lum, 1.0f);
            auto F0_spec_tint = c_tint * schlick_R0_eta(eta_t);
            F_schlick = calc_schlick<UnpolarizedSpectrum>(F0_spec_tint, cos_theta_i,eta_t);
        }
        return ek::lerp(F_dielectric,F_schlick,spec_tint);
    }

    /**
     * \brief Calculates the microfacet distribution parameters based on
     * Disney Course Notes.
     * \param anisotropic
     *     Anisotropy weight.
     * \param roughness
     *     Roughness based on Disney course notes.
     * \return Microfacet Distribution roughness parameters: alpha_x, alpha_y.
     */
    std::pair<Float, Float> calc_dist_params(Float anisotropic,
                                             Float roughness) const {
        Float roughness_2 = ek::sqr(roughness);
        if (!m_has_anisotropic) {
            Float a = ek::max(0.001f, roughness_2);
            return { a, a };
        }
        Float aspect = ek::sqrt(1.0f - 0.9f * anisotropic);
        return { ek::max(0.001f, roughness_2 / aspect),
                 ek::max(0.001f, roughness_2 * aspect) };
    }

    /**
     * \brief Computes a mask for macro-micro surface incompatibilities.
     * \param m
     *     Micro surface normal.
     * \param wi
     *     Incident direction.
     * \param wo
     *     Outgoing direction.
     * \param cos_theta_i
     *     Incident angle
     * \param reflection
     *     Flag for determining reflection or refraction case.
     * \return  Macro-micro surface compatibility mask.
     */
    Mask mac_mic_compatibility(const Vector3f &m, const Vector3f &wi,
                               const Vector3f &wo, const Float &cos_theta_i,
                               bool reflection) const {
        if (reflection) {
            return (ek::dot(wi, ek::mulsign(m, cos_theta_i)) > 0.0f) &&
                   (ek::dot(wo, ek::mulsign(m, cos_theta_i)) > 0.0f);
        } else {
            return (ek::dot(wi, ek::mulsign(m, cos_theta_i)) > 0.0f) &&
                   (ek::dot(wo, ek::mulsign_neg(m, cos_theta_i)) > 0.0f);
        }
    }

    std::pair<BSDFSample3f, Spectrum>
    sample(const BSDFContext &ctx, const SurfaceInteraction3f &si,
           Float sample1, const Point2f &sample2, Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::BSDFSample, active);

        Float cos_theta_i = Frame3f::cos_theta(si.wi);
        BSDFSample3f bs   = ek::zero<BSDFSample3f>();

        // Ignoring perfectly grazing incoming rays
        active &= ek::neq(cos_theta_i, 0.0f);

        if (unlikely(ek::none_or<false>(active)))
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
        Vector3f wi = ek::mulsign(si.wi, cos_theta_i);

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
                ek::rcp(prob_spec_reflect + prob_spec_trans + prob_coshemi_reflect +
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
        if (m_has_spec_trans && ek::any_or<true>(sample_spec_reflect)) {
            // Defining the Microfacet Distribution.
            auto [ax, ay] = calc_dist_params(anisotropic, roughness);
            MicrofacetDistribution spec_reflect_distr(MicrofacetType::GGX, ax,
                                                      ay);
            Normal3f m_spec_reflect =
                    std::get<0>(spec_reflect_distr.sample(wi, sample2));

            // Sampling
            Vector3f wo = reflect(wi, m_spec_reflect);
            ek::masked(bs.wo, sample_spec_reflect)                = wo;
            ek::masked(bs.sampled_component, sample_spec_reflect) = 3;
            ek::masked(bs.sampled_type, sample_spec_reflect) =
                    +BSDFFlags::GlossyReflection;

            // Filter the cases where macro and micro SURFACES do not agree
            // on the same side and the ray is not reflected.
            Mask reflect = Frame3f::cos_theta(wo) > 0.0f;
            active &=
                    (!sample_spec_reflect ||
                    (mac_mic_compatibility(m_spec_reflect, wi, wo, wi.z(), true) &&
                    reflect));
        }
        // Specular transmission lobe
        if (m_has_spec_trans && ek::any_or<true>(sample_spec_trans)) {
            // Relative index of refraction.
            Float eta_t = m_eta_thin->eval_1(si, active);

            // Defining the scaled distribution for thin specular
            // transmission Scale roughness based on IOR (Burley 2015,
            // Figure 15).
            Float roughness_scaled = (0.65f * eta_t - 0.35f) * roughness;
            auto [ax_scaled, ay_scaled] =
                    calc_dist_params(anisotropic, roughness_scaled);
            MicrofacetDistribution spec_trans_distr(MicrofacetType::GGX,
                                                    ax_scaled, ay_scaled);
            Normal3f m_spec_trans =
                    std::get<0>(spec_trans_distr.sample(wi, sample2));

            // Here, we are reflecting and turning the ray to the other side
            // since there is no bending on thin surfaces.
            Vector3f wo                          = reflect(wi, m_spec_trans);
            wo.z()                               = -wo.z();
            ek::masked(bs.wo, sample_spec_trans) = wo;
            ek::masked(bs.sampled_component, sample_spec_trans) = 2;
            ek::masked(bs.sampled_type, sample_spec_trans) =
                    +BSDFFlags::GlossyTransmission;

            // Filter the cases where macro and micro SURFACES do not agree
            // on the same side and the ray is not refracted.
            Mask transmission = Frame3f::cos_theta(wo) < 0.0f;
            active &=
                    (!sample_spec_trans ||
                    (mac_mic_compatibility(m_spec_trans, wi, wo, wi.z(), false) &&
                    transmission));
        }
        // Cosine hemisphere reflection for  reflection lobes (diffuse,
        //  retro reflection)
        if (ek::any_or<true>(sample_coshemi_reflect)) {
            ek::masked(bs.wo, sample_coshemi_reflect) =
                    warp::square_to_cosine_hemisphere(sample2);
            ek::masked(bs.sampled_component, sample_coshemi_reflect) = 0;
            ek::masked(bs.sampled_type, sample_coshemi_reflect) =
                    +BSDFFlags::DiffuseReflection;
        }
        // Diffuse transmission lobe
        if (m_has_diff_trans && ek::any_or<true>(sample_coshemi_trans)) {
            ek::masked(bs.wo, sample_coshemi_trans) =
                    -1.0f * warp::square_to_cosine_hemisphere(sample2);
            ek::masked(bs.sampled_component, sample_coshemi_trans) = 1;
            ek::masked(bs.sampled_type, sample_coshemi_trans) =
                    +BSDFFlags::DiffuseTransmission;
        }

        /* The direction is changed once more. (Because it was changed in
           the beginning.) */
        bs.wo = ek::mulsign(bs.wo, cos_theta_i);

        bs.pdf = pdf(ctx, si, bs.wo, active);
        active &= bs.pdf > 0.0f;
        Spectrum result = eval(ctx, si, bs.wo, active);
        return { bs, result / bs.pdf & active };
    }

    Spectrum eval(const BSDFContext &, const SurfaceInteraction3f &si,
                  const Vector3f &wo, Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        Float cos_theta_i = Frame3f::cos_theta(si.wi);
        // Ignore perfectly grazing configurations
        active &= ek::neq(cos_theta_i, 0.0f);

        if (unlikely(ek::none_or<false>(active)))
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
        auto base_color = m_base_color->eval(si, active);

        // Changing the signs in a way that we are always at the front side.
        // Thin BSDF is symmetric!
        Vector3f wi       = ek::mulsign(si.wi, cos_theta_i);
        Vector3f wo_t     = ek::mulsign(wo, cos_theta_i);
        cos_theta_i       = ek::abs(cos_theta_i);
        Float cos_theta_o = Frame3f::cos_theta(wo_t);

        Mask reflect = cos_theta_o > 0.0f;
        Mask refract = cos_theta_o < 0.0f;

        // Halfway vector calculation
        Vector3f wo_r = wo_t;
        wo_r.z()      = ek::abs(wo_r.z());
        Vector3f wh   = ek::normalize(wi + wo_r);

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
            Float F_dielectric = std::get<0>(fresnel(ek::dot(wi, wh), eta_t));

            // Specular reflection lobe
            if (ek::any_or<true>(spec_reflect_active)) {
                // Specular reflection distribution
                auto [ax, ay] = calc_dist_params(anisotropic, roughness);
                MicrofacetDistribution spec_reflect_distr(MicrofacetType::GGX,
                                                          ax, ay);

                // No need to calculate luminance if there is no color tint.
                Float lum = m_has_spec_tint
                        ? mitsuba::luminance(base_color, si.wavelengths)
                        : 1.0f;
                Float spec_tint =
                        m_has_spec_tint ? m_spec_tint->eval_1(si, active) : 0.0f;

                auto F_thin = thin_fresnel(F_dielectric,spec_tint,base_color,
                                           lum,ek::dot(wi,wh),eta_t);

                // Evaluate the microfacet normal distribution
                Float D = spec_reflect_distr.eval(wh);

                // Smith's shadow-masking function
                Float G = spec_reflect_distr.G(wi, wo_t, wh);

                // Calculate the specular reflection component.
                ek::masked(value, spec_reflect_active) +=
                        spec_trans * F_thin * D * G / (4.0f * cos_theta_i);
            }
            // Specular Transmission lobe
            if (ek::any_or<true>(spec_trans_active)) {
                // Defining the scaled distribution for thin specular
                // reflection Scale roughness based on IOR. (Burley 2015,
                // Figure 15).
                Float roughness_scaled = (0.65f * eta_t - 0.35f) * roughness;
                auto [ax_scaled, ay_scaled] =
                        calc_dist_params(anisotropic, roughness_scaled);
                MicrofacetDistribution spec_trans_distr(MicrofacetType::GGX,
                                                        ax_scaled, ay_scaled);

                // Evaluate the microfacet normal distribution
                Float D = spec_trans_distr.eval(wh);

                // Smith's shadow-masking function
                Float G = spec_trans_distr.G(wi, wo_t, wh);

                // Calculate the specular transmission component.
                ek::masked(value, spec_trans_active) +=
                        spec_trans * base_color * (1.0f - F_dielectric) * D * G /
                        (4.0f * cos_theta_i);
            }
        }
        // Diffuse, retro reflection, sheen and fake-subsurface evaluation
        if (ek::any_or<true>(diffuse_reflect_active)) {
            Float Fo = schlick_weight(ek::abs(cos_theta_o)),
            Fi = schlick_weight(cos_theta_i);

            // Diffuse response
            Float f_diff = (1.0f - 0.5f * Fi) * (1.0f - 0.5f * Fo);

            // Retro response
            Float cos_theta_d = ek::dot(wh, wo_t);
            Float Rr          = 2.0f * roughness * ek::sqr(cos_theta_d);
            Float f_retro     = Rr * (Fo + Fi + Fo * Fi * (Rr - 1.0f));

            /* Fake subsurface implementation based on Hanrahan-Krueger
               Fss90 used to "flatten" retro reflection based on
               roughness. */
            if (m_has_flatness) {
                Float Fss90 = Rr / 2.0f;
                Float Fss = ek::lerp(1.0, Fss90, Fo) * ek::lerp(1.0, Fss90, Fi);
                Float f_ss = 1.25f * (Fss * (1.0f / (ek::abs(cos_theta_o) +
                        ek::abs(cos_theta_i)) -
                                0.5f) +
                                        0.5f);

                // Adding diffuse, retro and fake subsurface components.
                ek::masked(value, diffuse_reflect_active) +=
                        (1.0f - spec_trans) * cos_theta_o * base_color *
                        ek::InvPi<Float> * (1.0f - diff_trans) *
                        ek::lerp(f_diff + f_retro, f_ss, flatness);
            } else
                // Adding diffuse and retro components. (no fake subsurface)
                ek::masked(value, diffuse_reflect_active) +=
                        (1.0f - spec_trans) * cos_theta_o * base_color *
                        ek::InvPi<Float> * (1.0f - diff_trans) * (f_diff + f_retro);

            // Sheen evaluation
            Float sheen =
                    m_has_sheen ? m_sheen->eval_1(si, active) : Float(0.0f);
            if (m_has_sheen && ek::any_or<true>(sheen > 0.0f)) {

                Float Fd = schlick_weight(ek::abs(cos_theta_d));

                if (m_has_sheen_tint) { // Tints the sheen evaluation to the
                    // base_color.
                    Float sheen_tint = m_sheen_tint->eval_1(si, active);

                    // Calculation of luminance of base_color.
                    Float lum = mitsuba::luminance(base_color, si.wavelengths);

                    // Normalize color with luminance and apply tint.
                    auto c_tint =
                            ek::select(lum > 0.0f, base_color / lum, 1.0f);
                    auto c_sheen = ek::lerp(1.0f, c_tint, sheen_tint);

                    // Adding the sheen component with tint.
                    ek::masked(value, diffuse_reflect_active) +=
                            sheen * (1.0f - spec_trans) * Fd * c_sheen *
                            (1.0f - diff_trans) * ek::abs(cos_theta_o);
                } else
                    // Adding the sheen component without tint.
                    ek::masked(value, diffuse_reflect_active) +=
                            sheen * (1.0f - spec_trans) * Fd * (1.0f - diff_trans) *
                            ek::abs(cos_theta_o);
            }
        }
        // Adding diffuse Lambertian transmission component.
        if (m_has_diff_trans && ek::any_or<true>(diffuse_trans_active)) {
            ek::masked(value, diffuse_trans_active) +=
                    (1.0f - spec_trans) * diff_trans * base_color *
                    ek::InvPi<Float> * ek::abs(cos_theta_o);
        }
        return depolarizer<Spectrum>(value) & active;
    }

    Float pdf(const BSDFContext &, const SurfaceInteraction3f &si,
              const Vector3f &wo, Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        Float cos_theta_i = Frame3f::cos_theta(si.wi);
        // Ignore perfectly grazing configurations.
        active &= ek::neq(cos_theta_i, 0.0f);

        if (unlikely(ek::none_or<false>(active)))
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
        Vector3f wi = ek::mulsign(si.wi, cos_theta_i);
        // wo_t stands for thin wo.
        Vector3f wo_t     = ek::mulsign(wo, cos_theta_i);
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
                ek::rcp(prob_spec_reflect + prob_spec_trans + prob_coshemi_reflect +
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
            wo_r.z()      = ek::abs(wo_r.z());
            Vector3f wh   = ek::normalize(wi + wo_r);

            // Macro-micro surface compatibility masks
            Mask mfacet_reflect_macmic =
                    mac_mic_compatibility(wh, wi, wo_t, wi.z(), true) && reflect;
            Mask mfacet_trans_macmic =
                    mac_mic_compatibility(wh, wi, wo_t, wi.z(), false) && refract;

            // d(wh) / d(wo) calculation. Inverted wo is used (wo_r) !
            Float dot_wor_wh  = ek::dot(wo_r, wh);
            Float dwh_dwo_abs = ek::abs(ek::rcp(4.0f * dot_wor_wh));

            // Specular reflection distribution.
            auto [ax, ay] = calc_dist_params(anisotropic, roughness);
            MicrofacetDistribution spec_reflect_distr(MicrofacetType::GGX, ax,
                                                      ay);
            // Defining the scaled distribution for thin specular reflection
            // Scale roughness based on IOR (Burley 2015, Figure 15).
            Float roughness_scaled = (0.65f * eta_t - 0.35f) * roughness;
            auto [ax_scaled, ay_scaled] =
                    calc_dist_params(anisotropic, roughness_scaled);
            MicrofacetDistribution spec_trans_distr(MicrofacetType::GGX,
                                                    ax_scaled, ay_scaled);
            // Adding specular lobes' pdfs
            ek::masked(pdf, mfacet_reflect_macmic) +=
                    prob_spec_reflect * spec_reflect_distr.pdf(wi, wh) *
                    dwh_dwo_abs;
            ek::masked(pdf, mfacet_trans_macmic) +=
                    prob_spec_trans * spec_trans_distr.pdf(wi, wh) * dwh_dwo_abs;
        }
        // Adding cosine hemisphere reflection pdf
        ek::masked(pdf, reflect) +=
                prob_coshemi_reflect * warp::square_to_cosine_hemisphere_pdf(wo_t);

        // Adding cosine hemisphere transmission pdf
        if (m_has_diff_trans) {
            ek::masked(pdf, refract) +=
                    prob_coshemi_trans *
                    warp::square_to_cosine_hemisphere_pdf(-wo_t);
        }
        return pdf;
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_object("diff_trans", m_diff_trans.get());
        callback->put_object("eta", m_eta_thin.get());
        callback->put_parameter("specular_reflectance_sampling_rate",
                                m_spec_refl_srate);
        callback->put_parameter("diffuse_transmittance_sampling_rate",
                                m_diff_trans_srate);
        callback->put_parameter("specular_transmittance_sampling_rate",
                                m_spec_trans_srate);
        callback->put_object("base_color", m_base_color.get());
        callback->put_object("roughness", m_roughness.get());
        callback->put_object("anisotropic", m_anisotropic.get());
        callback->put_object("spec_tint", m_spec_tint.get());
        callback->put_object("sheen", m_sheen.get());
        callback->put_object("sheen_tint", m_sheen_tint.get());
        callback->put_object("spec_trans", m_spec_trans.get());
        callback->put_object("flatness", m_flatness.get());
        callback->put_parameter("m_diff_refl_srate", m_diff_refl_srate);
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
        ek::make_opaque(m_base_color, m_roughness, m_anisotropic, m_sheen,
                        m_sheen_tint, m_spec_trans, m_flatness, m_spec_tint,
                        m_diff_trans, m_eta_thin, m_diff_refl_srate,
                        m_spec_refl_srate, m_spec_trans_srate,
                        m_diff_trans_srate);
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
    MTS_DECLARE_CLASS()
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

MTS_IMPLEMENT_CLASS_VARIANT(PrincipledThin, BSDF)
MTS_EXPORT_PLUGIN(PrincipledThin, "The Principled Thin Material")
NAMESPACE_END(mitsuba)
