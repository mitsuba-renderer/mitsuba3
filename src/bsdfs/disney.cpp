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

/*
 * \brief GTR1 Microfacet Distribution class
 *
 * This class implements GTR1 Microfacet Distribution Methods.
 * It is not included in Microfacet Class so it is added here.
 * Only Principled BSDF is using it.

 /parameters:
 * - alpha (float)
   - the roughness of the Microfacet Distribution
 */
template <typename Float, typename Spectrum> class GTR1 {
public:
    MTS_IMPORT_TYPES()

    GTR1(Float r) : alpha(r){};

    Float eval(const Vector3f &m) const {
        Float cos_theta  = Frame3f::cos_theta(m),
              cos_theta2 = ek::sqr(cos_theta), alpha2 = ek ::sqr(alpha);

        Float result = (alpha2 - 1.f) / (ek::Pi<Float> * ek::log(alpha2) *
                                         (1.f + (alpha2 - 1.f) * cos_theta2));

        return ek::select(result * cos_theta > 1e-20f, result, 0.f);
    }

    Float pdf(const Vector3f &m) const {
        return ek::select(m.z() < .0f, .0f, Frame3f::cos_theta(m) * eval(m));
    }

    Normal3f sample(const Point2f &sample) const {
        Float sin_phi, cos_phi;
        std::tie(sin_phi, cos_phi) =
            ek::sincos((2.f * ek::Pi<Float>) *sample.x());
        Float alpha2 = ek::sqr(alpha);

        Float cos_theta2 =
            (1.f - ek::pow(alpha2, 1.f - sample.y())) / (1.f - alpha2);

        Float sin_theta = ek::sqrt(ek::max(0.f, 1.f - cos_theta2)),
              cos_theta = ek::sqrt(ek::max(0.f, cos_theta2));

        return Normal3f(cos_phi * sin_theta, sin_phi * sin_theta, cos_theta);
    }

private:
    Float alpha;
};

/**!

.. _bsdf-disney:

Disney Principled BSDF (:monosp:`disney`)
-----------------------------------------------------
.. pluginparameters::

 * - thin
   - |bool|
   - Specifies whether `3D` Principled BSDF or 2D `thin` BSDF is going to be used.(Default:False)
 * - base_color
   - |spectrum| or |texture|
   - The color of the material.(Default:0.5)
 * - roughness
   - |float| or |texture|
   - Controls the roughness parameter of main specular lobes.(Default:0.5)
 * - anisotropic
   - |float| or |texture|
   - Controls the roughness parameters of main specular lobes. '0.0' makes the material isotropic.(Default:0.0)
 * - metallic
   - |texture| or |float|
   - The rate of the metallic major lobe.(Default:0.0) Only used in `3D`
 * - spec_trans
   - |texture| or |float|
   - has different role in 2 models.

     - `3D`: The rate of the bsdf major lobe.
     - `thin`: The rate of the all specular lobes.
 * - eta
   - Different for

     - `3D`: |float|
     - `thin`: |float| or |texture|
   - Interior IOR/Exterior IOR(Default:1.5 for `thin`)
 * - specular
   - |float|
   - Controls the Fresnel reflection coefficient, it has one to one
     correspondance with eta, so both of them can not be specified in xml. (Default:0.5) Only used in `3D`.
 * - spec_tint
   - |texture| or |float|
   - The fraction that dielectric reflection lobe is tinted towards base color.(Default:0.0) Only used in `3D`.
 * - sheen
   - |float| or |texture|
   - The coefficient of the sheen lobe.(Default:0.0)
 * - sheen_tint
   - |float| or |texture|
   - The fraction that sheen lobe is tinted towards base color.(Default:0.0)
 * - flatness
   - |float| or |texture|
   - The coefficient of the fake subsurface approximation lobe based on Hanrahan-Krueger.(Default:0.0)
 * - clearcoat
   - |texture| or |float|
   - The coefficient of the secondary isotropic specular lobe. (Default:0.0) Only used in `3D`.
 * - clearcoat_gloss
   - |texture| or |float|
   - Controls the roughness of the secondary specular lobe. (Default:0.0) Only used in `3D`.
 * - diff_trans
   - |texture| or |float|
   - The rate that the energy of diffuse reflection is given to the transmission.(Default:0.0) only used in `thin`


 * - diffuse_reflectance_sampling_rate
   - |float|
   - The rate of cosine hemisphere reflection in sampling.(Default:2.0 for `3D`, 1.0 for `thin`)
 * - main_specular_sampling_rate
   - |float|
   - The rate of main specular lobe in sampling.(Default:1.0) Only used in `3D`.
 * - clearcoat_sampling_rate
   - |float|
   - The rate of the secondary specular reflection in sampling.(Default:0.0) Only used in `3D`.
 * - specular_reflectance_sampling_rate
   - |float|
   - The rate of main specular reflection in sampling.(Default:1.0) Only used in `thin`
 * - specular_transmittance_sampling_rate
   - |float|
   - The rate of main specular transmission in sampling.(Default:1.0) Only used in `thin`.
 * - diffuse_transmittance_sampling_rate
   - |float|
   - The rate of cosine hemisphere transmission in sampling.(Default:1.0) only used in `thin`

All of the parameters except sampling rates, :monosp:`diff_trans` and :monosp:`eta` should take values
between 0.0 and 1.0 .  The range of :monosp:`diff_trans` is 0.0 to 2.0 .
For faster performance in jit compiler, the parameters which have 0.0 default value should not be specified
in the xml file if the default values are used.



.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/disney_blend.jpg
   :caption: Blending of parameters when transmission lobe is turned off. ( `3D` material)
.. subfigure:: ../../resources/data/docs/images/render/disney_st_blend.jpg
    :caption: Blending of parameters when transmission lobe is turned on. ( `3D` material)
.. subfigure:: ../../resources/data/docs/images/render/thin_disney_blend.jpg
    :caption: Blending of parameters. ( `thin` material)
.. subfigend::
    :label: fig-blend-disney

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/3D_disney.png
   :caption: General structure of the `3D` Principled BSDF
.. subfigure:: ../../resources/data/docs/images/render/thin_disney.png
    :caption: :caption: General structure of the `thin` Principled BSDF
.. subfigend::
    :label: fig-structure-disney
 */
template <typename Float, typename Spectrum>
class Disney final : public BSDF<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(BSDF, m_flags, m_components)
    MTS_IMPORT_TYPES(Texture, MicrofacetDistribution)
    using GTR1 = GTR1<Float, Spectrum>;

    Disney(const Properties &props) : Base(props) {

        if (props.has_property("thin")) {
            thin = props.get<bool>("thin");
        } else
            thin = false;

        // parameters that are used in both models
        base_color = props.texture<Texture>("base_color", .5f);
        roughness  = props.texture<Texture>("roughness", .5f);

        has_anisotropic = props.has_property("anisotropic");
        anisotropic     = props.texture<Texture>("anisotropic", .0f);

        has_spec_trans = props.has_property("spec_trans");
        spec_trans     = props.texture<Texture>("spec_trans", .0f);

        has_sheen      = props.has_property("sheen");
        sheen          = props.texture<Texture>("sheen", .0f);
        has_sheen_tint = props.has_property("sheen_tint");
        sheen_tint     = props.texture<Texture>("sheen_tint", .0f);

        has_flatness = props.has_property("flatness");
        flatness     = props.texture<Texture>("flatness", .0f);

        // thin and 3d models use different parameters
        if (!thin) {
            has_clearcoat   = props.has_property("clearcoat");
            clearcoat       = props.texture<Texture>("clearcoat", .0f);
            clearcoat_gloss = props.texture<Texture>("clearcoat_gloss", .0f);

            has_spec_tint = props.has_property("spec_tint");
            spec_tint     = props.texture<Texture>("spec_tint", .0f);

            main_specular_sampling_rate =
                props.get("main_specular_sampling_rate", 1.f);
            clearcoat_sampling_rate =
                props.get("clearcoat_sampling_rate", 1.f);
            diffuse_reflectance_sampling_rate =
                props.get("diffuse_reflectance_sampling_rate", 2.f);

            has_metallic = props.has_property("metallic");
            metallic     = props.texture<Texture>("metallic", .0f);


            /*eta and specular has one to one correspondence, both of them can
             * not be specified. */
            if (props.has_property("eta") && props.has_property("specular")) {
                Throw("Specified an invalid index of refraction property  "
                      "\"%s\", either use "
                      "\"eta\" or \"specular\" !");

            } else if (props.has_property("eta")) {
                eta_specular = true;
                eta        = props.get<float>("eta");
                // eta = 1 is not plausible for transmission
                ek::masked(eta,has_spec_trans && eta==1) = 1.001f;
            } else {
                eta_specular = false;
                specular     = props.get<float>("specular",0.5f);
                // zero specular is not plausible for transmission
                ek::masked(specular,has_spec_trans && specular==0.f) = 1e-3f;
                eta = 2.f * ek::rcp(1.f - ek::sqrt(0.08f * specular)) - 1.f;
            }

        } else {
            // Thin material can also take texture parameters for eta while 3d
            // can not !!
            eta_thin = props.texture<Texture>("eta", 1.5f);
            has_diff_trans = props.has_property("diff_trans");
            diff_trans     = props.texture<Texture>("diff_trans", .0f);

            spec_reflect_sampling_rate =
                props.get("spec_reflect_sampling_rate", 1.f);
            spec_trans_sampling_rate =
                props.get("spec_trans_sampling_rate", 1.f);
            diffuse_transmittance_sampling_rate =
                props.get("diffuse_transmittance_sampling_rate", 1.f);
            diffuse_reflectance_sampling_rate =
                props.get("diffuse_reflectance_sampling_rate", 1.f);
        }

        if (thin) {
            // diffuse reflection lobe for thin
            m_components.push_back(BSDFFlags::DiffuseReflection |
                                   BSDFFlags::FrontSide | BSDFFlags::BackSide);

            // specular diffuse lobe for thin
            m_components.push_back(BSDFFlags::DiffuseTransmission |
                                   BSDFFlags::FrontSide | BSDFFlags::BackSide);

            // specular transmission lobe for thin
            m_components.push_back(BSDFFlags::GlossyTransmission | BSDFFlags::FrontSide |
                    BSDFFlags::BackSide | BSDFFlags::Anisotropic);
        }

        else { // 3d case
            // diffuse reflection lobe in disney brdf lobe
            m_components.push_back(BSDFFlags::DiffuseReflection |
                                   BSDFFlags::FrontSide);

            // clearcoat lobe (only exists in 3d model)

            m_components.push_back(BSDFFlags::GlossyReflection |
                                   BSDFFlags::FrontSide);

            // specular transmission lobe in Disney bsdf lobe
            m_components.push_back(
                    BSDFFlags::GlossyTransmission | BSDFFlags::FrontSide |
                    BSDFFlags::BackSide | BSDFFlags::NonSymmetric |
                    BSDFFlags::Anisotropic);

        }

        // main specular reflection (same flags for both thin and 3d)
        m_components.push_back(BSDFFlags::GlossyReflection |
                                   BSDFFlags::FrontSide | BSDFFlags::BackSide |
                                   BSDFFlags::Anisotropic);


        m_flags = m_components[0] | m_components[1] | m_components[2] |
                  m_components[3];

        ek::set_attr(this, "flags", m_flags);
    }
    // Helper Functions
    // Functions Related to the Schlick Appproximation

    Float schlick_weight(Float cos_i) const {
        Float m = ek::clamp(1.0f - cos_i, .0f, 1.0f);
        return (m * m) * (m * m) * m;
    }

    /** Schlick Approximation for Fresnel Reflection coefficient F = R0 + (1-R0)
      (1-cos^5(i)). Transmitted ray's angle should be used for eta<1. **/
    template <typename T>
    T calc_schlick(T R0, Float cos_theta_i, Float eta_) const {

        Mask outside_mask = cos_theta_i >= 0.f;
        Float rcp_eta     = ek::rcp(eta_),
              eta_it      = ek::select(outside_mask, eta_, rcp_eta),
              eta_ti      = ek::select(outside_mask, rcp_eta, eta_);

        Float cos_theta_t_sqr = ek::fnmadd(
            ek::fnmadd(cos_theta_i, cos_theta_i, 1.f), eta_ti * eta_ti, 1.f);

        Float cos_theta_t = ek::safe_sqrt(cos_theta_t_sqr);

        return ek::select(
            eta_it > 1.f,
            ek::lerp(schlick_weight(ek::abs(cos_theta_i)), 1.0f, R0),
            ek::lerp(schlick_weight(cos_theta_t), 1.0f, R0));
    }

    /* For a dielectric, R(0) = (eta - 1)^2 / (eta + 1)^2 */
    Float schlick_R0_eta(Float eta_) const {
        return ek::sqr(eta_ - 1.0f) / ek::sqr(eta_ + 1.0f);
    }

    /** Modified fresnel function for disney 3d material. It blends metallic and
    dilectric responses (not true metallic) spec_tint portion of dielectric
    response is tinted towards base color. Schlick approximation is used for
     spec_tint and metallic parts whereas dielectric part is calculated with the
    true value. **/
    UnpolarizedSpectrum
    disney_fresnel(const Float &F_dielectric, const Float &metallic_w,
                   const Float &spec_tint_w, const Float &eta_,
                   const UnpolarizedSpectrum &color, const Float &lum,
                   const Float &cos_theta_i, const Mask &front_side,
                   const Float &bsdf_w) const {

        auto outside_mask = cos_theta_i >= 0.f;
        Float rcp_eta     = ek::rcp(eta_),
              eta_it      = ek::select(outside_mask, eta_, rcp_eta);

        UnpolarizedSpectrum F_schlick(0);
        if (has_metallic) {
            F_schlick += metallic_w * calc_schlick<UnpolarizedSpectrum>(
                                          color, cos_theta_i, eta_);
        }
        if (has_spec_tint) {
            UnpolarizedSpectrum c_tint =
                ek::select(lum > 0.0f, color / lum, UnpolarizedSpectrum(1.f));
            UnpolarizedSpectrum F0_spec_tint = c_tint * schlick_R0_eta(eta_it);
            F_schlick += (1 - metallic_w) * spec_tint_w *
                         calc_schlick<UnpolarizedSpectrum>(F0_spec_tint,
                                                           cos_theta_i, eta_);
        }

        UnpolarizedSpectrum F_front =
            (1 - metallic_w) * (1 - spec_tint_w) * F_dielectric + F_schlick;

        // for inside case there is no tint or metallic, just true dilectric
        // coefficient
        return ek::select(front_side, F_front, bsdf_w * F_dielectric);
    }

    /** Calculates the microfacet distribution parameters.
    Rougness is squared for more intuitive results. (for artists) **/
    std::pair<Float, Float> calc_dist_params(Float anisotropic_w,
                                             Float roughness_w) const {
        Float roughness_2 = ek::sqr(roughness_w);
        if (!has_anisotropic) {
            Float a = ek::max(0.001f, roughness_2);
            return { a, a };
        }
        Float aspect = ek::sqrt(1.f - 0.9f * anisotropic_w);
        return { ek::max(0.001f, roughness_2 / aspect),
                 ek::max(0.001f, roughness_2 * aspect) };
    }

    /** seperable shadowing-masking for GGX1. Mitsuba does not have a GGX1
    support in microfacet so it is added in Disney material plugin. **/
    Float clearcoat_G(const Vector3f &wi, const Vector3f &wo,
                      const Float &alpha) const {
        return smith_ggx1(wi, alpha) * smith_ggx1(wo, alpha);
    }

    /** calculates smith ggx1 shadowing masking function. **/
    Float smith_ggx1(const Vector3f &v, const Float &alpha) const {

        Float alpha2     = ek::sqr(alpha),
              cos_theta  = ek::abs(Frame3f::cos_theta(v)),
              cos_theta2 = ek::sqr(cos_theta);

        return 1.f / (cos_theta +
                      ek::sqrt(alpha2 + cos_theta2 - alpha2 * cos_theta2));
    }


    std::pair<BSDFSample3f, Spectrum>
    sample(const BSDFContext &ctx, const SurfaceInteraction3f &si,
           Float sample1, const Point2f &sample2, Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::BSDFSample, active);

        Float cos_theta_i = Frame3f::cos_theta(si.wi);
        BSDFSample3f bs   = ek::zero<BSDFSample3f>();

        // ignoring perfectly grazing incoming rays
        active &= ek::neq(cos_theta_i, 0.f);

        if (unlikely(ek::none_or<false>(active)))
            return { bs, 0.f };

        // store the weights that are both used in thin and 3D
        Float anisotropic_w = has_anisotropic ? anisotropic->eval_1(si, active)
                                              : Float(0.f),
              roughness_w   = roughness->eval_1(si, active),
              strans_w =
                  has_spec_trans ? spec_trans->eval_1(si, active) : Float(0.f);

        if (thin) { // thin model implementation

            // diffuse transmission weight
            Float diff_trans_w = has_diff_trans
                                     ? diff_trans->eval_1(si, active) / 2.f
                                     : Float(0.f);

            // there is no negative incoming angle for a thin surface, so we
            // change the direction for back_side case. The direction change is
            // taken into account after sampling the outgoing direction.
            Vector3f wi = ek::mulsign(si.wi, cos_theta_i);

            // probability for each minor lobe
            Float prob_spec_reflect =
                has_spec_trans ? strans_w * spec_reflect_sampling_rate
                               : Float(0.f);
            Float prob_spec_trans = has_spec_trans
                                        ? strans_w * spec_trans_sampling_rate
                                        : Float(0.f);
            // since we have more lobes in reflection part (diffuse, sheen and
            // retro) that are unaffacted by the transmission,  reflection  has
            // 1/3 prob even when  diff_trans is 1.(given that the all sampling
            // rates are 1)
            Float prob_coshemi_reflect = diffuse_reflectance_sampling_rate *
                                         (1.f - strans_w) *
                                         (3 - 2 * diff_trans_w) / 3.f;
            Float prob_coshemi_trans =
                has_diff_trans
                    ? diffuse_transmittance_sampling_rate * (1.f - strans_w) *
                          2.f * (diff_trans_w) / 3.f
                    : Float(0.f);

            // normalizing the probabilities for the specular minor lobes
            Float total_prob = prob_spec_reflect + prob_spec_trans +
                               prob_coshemi_reflect + prob_coshemi_trans;
            prob_spec_reflect /= total_prob;
            prob_spec_trans /= total_prob;
            prob_coshemi_reflect /= total_prob;
            // prob_coshemi_trans /= total_prob;

            // sampling masks
            Float curr_prob(0.f);
            Mask sample_spec_reflect =
                has_spec_trans ? active && sample1 < prob_spec_reflect : 0;
            curr_prob += prob_spec_reflect;
            Mask sample_spec_trans =
                has_spec_trans ? active && sample1 >= curr_prob &&
                                     sample1 < curr_prob + prob_spec_trans
                               : 0;
            curr_prob += prob_spec_trans;
            Mask sample_coshemi_reflect =
                active && sample1 >= curr_prob &&
                sample1 < curr_prob + prob_coshemi_reflect;
            curr_prob += prob_coshemi_reflect;
            Mask sample_coshemi_trans =
                has_diff_trans ? active && sample1 >= curr_prob : 0;

            // thin model is just a surface, both mediums have the same index of
            // refraction
            bs.eta = 1.0f;

            // Microfacet reflection lobe
            if (has_spec_trans && ek::any_or<true>(sample_spec_reflect)) {

                // defining the Microfacet Distribution.
                auto [ax, ay] = calc_dist_params(anisotropic_w, roughness_w);
                MicrofacetDistribution spec_reflect_distr(MicrofacetType::GGX,
                                                          ax, ay);

                Normal3f m_spec_reflect =
                    std::get<0>(spec_reflect_distr.sample(wi, sample2));

                Vector3f wo = reflect(wi, m_spec_reflect);
                ek::masked(bs.wo, sample_spec_reflect)                = wo;
                ek::masked(bs.sampled_component, sample_spec_reflect) = 3;
                ek::masked(bs.sampled_type, sample_spec_reflect) =
                    +BSDFFlags::GlossyReflection;

                // filter the cases where macro and microfacets do not agree on
                // the same side and the ray is not reflected.
                Mask reflect = Frame3f::cos_theta(wo) > 0.0f;
                active &= (!sample_spec_reflect ||
                           (ek::dot(wi, m_spec_reflect) > 0.f &&
                            ek::dot(wo, m_spec_reflect) > 0.f && reflect));
            }

            if (has_spec_trans && ek::any_or<true>(sample_spec_trans)) {

                Float eta_t = eta_thin->eval_1(si, active);

                // Defining the scaled distribution for thin specular
                // transmission Scale roughness based on IOR (Burley 2015,
                // Figure 15).
                Float rougness_scaled = (0.65f * eta_t - 0.35f) * roughness_w;
                auto [ax_scaled, ay_scaled] =
                    calc_dist_params(anisotropic_w, rougness_scaled);

                MicrofacetDistribution spec_trans_distr(MicrofacetType::GGX,
                                                        ax_scaled, ay_scaled);
                Normal3f m_spec_trans =
                    std::get<0>(spec_trans_distr.sample(wi, sample2));

                // here we are reflecting and turning the ray to the other side
                // since there is no overall refraction on thin surfaces
                Vector3f wo = reflect(wi, m_spec_trans);
                wo.z()      = -wo.z();
                ek::masked(bs.wo, sample_spec_trans)                = wo;
                ek::masked(bs.sampled_component, sample_spec_trans) = 2;
                ek::masked(bs.sampled_type, sample_spec_trans) =
                    +BSDFFlags::GlossyTransmission;

                // filter the cases where macro and microfacets do not agree on
                // the same side and the ray is not refracted.
                Mask transmission = Frame3f::cos_theta(wo) < 0.0f;
                active &= (!sample_spec_trans ||
                           (ek::dot(wi, m_spec_trans) > 0.f &&
                            ek::dot(wo, m_spec_trans) < 0.f && transmission));
            }

            // cosine hemisphere reflection for  reflection lobes (diffuse,
            // sheen, retroreflection)
            if (ek::any_or<true>(sample_coshemi_reflect)) {
                ek::masked(bs.wo, sample_coshemi_reflect) =
                    warp::square_to_cosine_hemisphere(sample2);
                ek::masked(bs.sampled_component, sample_coshemi_reflect) = 0;
                ek::masked(bs.sampled_type, sample_coshemi_reflect) =
                    +BSDFFlags::DiffuseReflection;
            }

            // diffuse transmission lobe for thin
            if (has_diff_trans && ek::any_or<true>(sample_coshemi_trans)) {
                ek::masked(bs.wo, sample_coshemi_trans) =
                    -1.f * warp::square_to_cosine_hemisphere(sample2);
                ek::masked(bs.sampled_component, sample_coshemi_trans) = 1;
                ek::masked(bs.sampled_type, sample_coshemi_trans) =
                    +BSDFFlags::DiffuseTransmission;
            }

            // The direction is changed once more. (because it was changed in
            // the beginning)
            bs.wo = ek::mulsign(bs.wo, cos_theta_i);

        }

        else {
            // defining the weights peculiar to the 3D case.
            Float metallic_w =
                has_metallic ? metallic->eval_1(si, active) : Float(0.f);

            // in case eta value is changed through specular with python plugin,
            // we need to update eta
            //Float eta;
            /*
            if (!eta_specular)
                eta = 2.f * ek::rcp(1.f - ek::sqrt(0.08f * specular)) - 1.f;
            else
                eta = m_eta;

             */
            Float clearcoat_w =
                has_clearcoat ? clearcoat->eval_1(si, active) : Float(0.f);
            Float brdf_w = (1.0f - metallic_w) * (1.0f - strans_w),
                  bsdf_w = has_spec_trans ? (1.0f - metallic_w) * strans_w
                                          : Float(0.f);

            // masks if incident direction is front (wi.z<0)
            Mask front_side = cos_theta_i > 0;

            // defining main specular reflection distribution
            auto [ax, ay] = calc_dist_params(anisotropic_w, roughness_w);
            MicrofacetDistribution spec_distr(MicrofacetType::GGX, ax, ay);
            Normal3f m_spec = std::get<0>(
                spec_distr.sample(ek::mulsign(si.wi, cos_theta_i), sample2));

            // Fresnel coefficient for the main specular.
            auto [F_spec_dielectric, cos_theta_t, eta_it, eta_ti] =
                fresnel(ek::dot(si.wi, m_spec), eta);

            // if bsdf major lobe is turned of, we do not sample the inside
            // case.
            active &= (front_side || bsdf_w > 0.f);

            // probabilities for sampled lobes.
            Float prob_spec_reflect(0.f);
            Float prob_spec_trans(0.f);
            Float prob_clearcoat(0.f);
            Float prob_diffuse(0.f);

            // probability definitions
            /* for inside of the material, just Microfacet Reflection and
             * Microfacet Transmission is used. */
            prob_spec_reflect =
                ek::select(front_side,
                           main_specular_sampling_rate *
                               (1.f - bsdf_w * (1 - F_spec_dielectric)),
                           F_spec_dielectric);
            prob_spec_trans =
                has_spec_trans
                    ? ek::select(front_side,
                                 main_specular_sampling_rate * bsdf_w *
                                     (1 - F_spec_dielectric),
                                 (1.f - F_spec_dielectric))
                    : Float(0.f);
            // Clearcoat has 1/4 of the main specular reflection energy.
            prob_clearcoat =
                has_clearcoat
                    ? ek::select(front_side,
                                 0.25f * clearcoat_w * clearcoat_sampling_rate,
                                 0.f)
                    : Float(0.f);
            prob_diffuse = ek::select(
                front_side, brdf_w * diffuse_reflectance_sampling_rate, 0.f);

            // normalizing the probabilities.
            Float tot_prob = prob_spec_reflect + prob_spec_trans +
                             prob_clearcoat + prob_diffuse;
            // prob_spec_reflect/=tot_prob;
            prob_spec_trans /= tot_prob;
            prob_clearcoat /= tot_prob;
            prob_diffuse /= tot_prob;

            // sampling mask definitions
            Float curr_prob     = 0.f;
            Mask sample_diffuse = active && sample1 < prob_diffuse;
            curr_prob += prob_diffuse;
            Mask sample_clearcoat =
                has_clearcoat ? active && sample1 >= curr_prob &&
                                    sample1 < curr_prob + prob_clearcoat
                              : 0;
            curr_prob += prob_clearcoat;
            Mask sample_spec_trans =
                has_spec_trans ? active && (sample1 >= curr_prob) &&
                                     (sample1 < curr_prob + prob_spec_trans)
                               : 0;
            curr_prob += prob_spec_trans;
            Mask sample_spec_reflect = active && sample1 >= curr_prob;

            // this will be changed only for transmission.
            bs.eta = 1.0f;

            // main specular reflection sampling
            if (ek::any_or<true>(sample_spec_reflect)) {
                Vector3f wo                            = reflect(si.wi, m_spec);
                ek::masked(bs.wo, sample_spec_reflect) = wo;
                ek::masked(bs.sampled_component, sample_spec_reflect) = 3;
                ek::masked(bs.sampled_type, sample_spec_reflect) =
                    +BSDFFlags::GlossyReflection;

                /* Filter the cases where macro and microfacets do not agree on
                 the same side and reflection is not successful*/
                Mask reflect = cos_theta_i * Frame3f::cos_theta(wo) > 0.0f;
                active &=
                    (!sample_spec_reflect ||
                     (ek::dot(si.wi, ek::mulsign(m_spec, cos_theta_i)) > 0.f &&
                      ek::dot(wo, ek::mulsign(m_spec, cos_theta_i)) > 0.f &&
                      reflect));
            }

            // main specular transmission
            if (has_spec_trans && ek::any_or<true>(sample_spec_trans)) {
                Vector3f wo = refract(si.wi, m_spec, cos_theta_t, eta_ti);
                ek::masked(bs.wo, sample_spec_trans)                = wo;
                ek::masked(bs.sampled_component, sample_spec_trans) = 2;
                ek::masked(bs.sampled_type, sample_spec_trans) =
                    +BSDFFlags::GlossyTransmission;
                ek::masked(bs.eta, sample_spec_trans) = eta_it;

                /* Filter the cases where macro and microfacets do not agree on
                 the same side and refraction is succesful*/
                Mask refract = cos_theta_i * Frame3f::cos_theta(wo) < 0.0f;
                active &=
                    (!sample_spec_trans ||
                     (ek::dot(si.wi, ek::mulsign(m_spec, cos_theta_i)) > 0.f &&
                      ek::dot(wo, ek::mulsign_neg(m_spec, cos_theta_i)) > 0.f &&
                      refract));
            }

            // the secondary specular reflection lobe (clearcoat)
            if (has_clearcoat && ek::any_or<true>(sample_clearcoat)) {
                // defining the clearcoat distribution. (it uses GGX1)
                Float clearcoat_glossw = clearcoat_gloss->eval_1(si, active);
                GTR1 cc_dist(ek::lerp(0.1f, 0.001f, clearcoat_glossw));
                Normal3f m_clearcoat = cc_dist.sample(sample2);

                Vector3f wo = reflect(si.wi, m_clearcoat);
                ek::masked(bs.wo, sample_clearcoat)                = wo;
                ek::masked(bs.sampled_component, sample_clearcoat) = 1;
                ek::masked(bs.sampled_type, sample_clearcoat) =
                    +BSDFFlags::GlossyReflection;

                /* Filter the cases where macro and microfacets do not agree on
                 the same side and reflection is not successful*/
                Mask reflect = cos_theta_i * Frame3f::cos_theta(wo) > 0.0f;
                active &= (!sample_clearcoat ||
                           (ek::dot(si.wi, ek::mulsign(m_clearcoat,
                                                       cos_theta_i)) > 0.f &&
                            ek::dot(wo, ek::mulsign(m_clearcoat, cos_theta_i)) >
                                0.f &&
                            reflect));
            }

            // cosine hemisphere reflection lobe
            if (ek::any_or<true>(sample_diffuse)) {
                Vector3f wo = ek::mulsign(
                    warp::square_to_cosine_hemisphere(sample2), cos_theta_i);
                ek::masked(bs.wo, sample_diffuse)                = wo;
                ek::masked(bs.sampled_component, sample_diffuse) = 0;
                ek::masked(bs.sampled_type, sample_diffuse) =
                    +BSDFFlags::DiffuseReflection;
                Mask reflect = cos_theta_i * Frame3f::cos_theta(wo) > 0.0f;
                active &= (!sample_diffuse || reflect);
            }
        }

        bs.pdf = pdf(ctx, si, bs.wo, active);
        active &= bs.pdf > 0.f;
        Spectrum result = eval(ctx, si, bs.wo, active);
        return { bs, result / bs.pdf & active };
    }

    Spectrum eval(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                  const Vector3f &wo, Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        Float cos_theta_i = Frame3f::cos_theta(si.wi);

        // Ignore perfectly grazing configurations
        active &= ek::neq(cos_theta_i, 0.f);

        if (unlikely(ek::none_or<false>(active)))
            return 0.f;

        // definition of common parameters
        Float anisotropic_w = has_anisotropic ? anisotropic->eval_1(si, active)
                                              : Float(0.f),
              roughness_w   = roughness->eval_1(si, active),
              flatness_w =
                  has_flatness ? flatness->eval_1(si, active) : Float(0.f),
              strans_w =
                  has_spec_trans ? spec_trans->eval_1(si, active) : Float(0.f);

        UnpolarizedSpectrum color = base_color->eval(si, active);

        // thin model evaluation
        if (thin) {

            // parameters for the thin model
            Float eta_t       = eta_thin->eval_1(si, active);
            Float diff_transw = has_diff_trans
                                    ? diff_trans->eval_1(si, active) / 2.f
                                    : Float(0.f);

            // Changing the signs in a way that we are always at the front side.
            // Thin BSDF is symmetric!
            Vector3f wi       = ek::mulsign(si.wi, cos_theta_i);
            Vector3f wo_t     = ek::mulsign(wo, cos_theta_i);
            cos_theta_i       = ek::abs(cos_theta_i);
            Float cos_theta_o = Frame3f::cos_theta(wo_t);

            Mask reflect = cos_theta_o > 0.f;
            Mask refract = cos_theta_o < 0.f;

            // halfway vector
            Vector3f wo_r = wo_t;
            wo_r.z()      = ek::abs(wo_r.z());
            Vector3f wh   = ek::normalize(wi + wo_r);

            // masks for controlling the micro-macro surface incompatibilities
            // and correct sides.
            Mask spec_reflect_active =
                active && reflect && ek::dot(wi, wh) > 0.f &&
                ek::dot(wo_t, wh) > 0.f && strans_w > 0.f;
            Mask spec_trans_active = active && refract &&
                                     ek::dot(wi, wh) > 0.f &&
                                     ek::dot(wo_t, wh) < 0.f && strans_w > 0.f;
            Mask diffuse_reflect_active = active && reflect && strans_w < 1;
            Mask diffuse_trans_active =
                active && refract && strans_w < 1 && diff_transw > 0.f;

            UnpolarizedSpectrum value = 0.f;

            if (has_spec_trans) {

                // dielctric Fresnel
                Float F_dielectric =
                    std::get<0>(fresnel(ek::dot(wi, wh), eta_t));

                if (ek::any_or<true>(spec_reflect_active)) {
                    // specular reflection distribution
                    auto [ax, ay] =
                        calc_dist_params(anisotropic_w, roughness_w);
                    MicrofacetDistribution spec_reflect_distr(
                        MicrofacetType::GGX, ax, ay);

                    // Evaluate the microfacet normal distribution
                    Float D = spec_reflect_distr.eval(wh);
                    // Smith's shadow-masking function
                    Float G = spec_reflect_distr.G(wi, wo_t, wh);
                    // Calculate the specular reflection component
                    ek::masked(value, spec_reflect_active) +=
                        strans_w * F_dielectric * D * G / (4.f * cos_theta_i);
                }

                if (ek::any_or<true>(spec_trans_active)) {
                    // Defining the scaled distribution for thin specular
                    // reflection Scale roughness based on IOR (Burley 2015,
                    // Figure 15).
                    Float rougness_scaled =
                        (0.65f * eta_t - 0.35f) * roughness_w;
                    auto [ax_scaled, ay_scaled] =
                        calc_dist_params(anisotropic_w, rougness_scaled);

                    MicrofacetDistribution spec_trans_distr(
                        MicrofacetType::GGX, ax_scaled, ay_scaled);
                    // Evaluate the microfacet normal distribution
                    Float D = spec_trans_distr.eval(wh);
                    // Smith's shadow-masking function
                    Float G = spec_trans_distr.G(wi, wo_t, wh);
                    // Calculate the specular transmission component

                    ek::masked(value, spec_trans_active) +=
                        strans_w * color * (1.f - F_dielectric) * D * G /
                        (4.f * cos_theta_i);
                }
            }

            // diffuse, retroreflection, sheen and fake-subsurface evaluation
            if (ek::any_or<true>(diffuse_reflect_active)) {
                // diffuse and retro reflection part.
                Float Fo = schlick_weight(ek::abs(cos_theta_o)),
                      Fi = schlick_weight(cos_theta_i);

                UnpolarizedSpectrum f_diff =
                    (1.0f - 0.5f * Fi) * (1.0f - 0.5f * Fo);
                Float cos_theta_d = ek::dot(wh, wo_t);
                Float Rr          = 2.0f * roughness_w * ek::sqr(cos_theta_d);
                UnpolarizedSpectrum f_retro =
                    Rr * (Fo + Fi + Fo * Fi * (Rr - 1.0f));

                if (has_flatness) {
                    // fake subsurface implementation based on Hanrahan Krueger
                    // Fss90 used to "flatten" retroreflection based on
                    // roughness
                    Float Fss90 = cos_theta_d * cos_theta_d * roughness_w;
                    Float Fss =
                        ek::lerp(1.0, Fss90, Fo) * ek::lerp(1.0, Fss90, Fi);

                    Float f_ss = 1.25f * (Fss * (1 / (ek::abs(cos_theta_o) +
                                                      ek::abs(cos_theta_i)) -
                                                 .5f) +
                                          .5f);

                    ek::masked(value, diffuse_reflect_active) +=
                        (1.f - strans_w) * cos_theta_o * color *
                        ek::InvPi<Float> *
                        ((1.f - diff_transw) *
                             ek::lerp(f_diff, f_ss, flatness_w) +
                         f_retro);
                } else
                    ek::masked(value, diffuse_reflect_active) +=
                        (1.f - strans_w) * cos_theta_o * color *
                        ek::InvPi<Float> *
                        ((1.f - diff_transw) * f_diff + f_retro);

                // sheen evaluation
                Float sheen_w =
                    has_sheen ? sheen->eval_1(si, active) : Float(0.f);
                if (has_sheen && ek::any_or<true>(sheen_w > 0.f)) {

                    Float Fd = schlick_weight(ek::abs(cos_theta_d));

                    // tints the sheen evaluation to the base_color
                    if (has_sheen_tint) {
                        Float sheen_tintw = sheen_tint->eval_1(si, active);
                        Float lum(1.f);
                        if constexpr (is_rgb_v<Spectrum>)
                            lum = mitsuba::luminance(color);
                        else
                            lum = mitsuba::luminance(color, si.wavelengths);

                        UnpolarizedSpectrum c_tint = ek::select(
                            lum > 0.0f, color / lum, UnpolarizedSpectrum(1.f));
                        UnpolarizedSpectrum c_sheen = ek::lerp(
                            UnpolarizedSpectrum(1.f), c_tint, sheen_tintw);

                        ek::masked(value, diffuse_reflect_active) +=
                            sheen_w * (1.f - strans_w) * Fd * c_sheen *
                            ek::abs(cos_theta_o);
                    } else
                        ek::masked(value, diffuse_reflect_active) +=
                            sheen_w * (1.f - strans_w) * Fd *
                            ek::abs(cos_theta_o);
                }
            }

            // diffuse Lambertian Transmission Evaluation
            if (has_diff_trans && ek::any_or<true>(diffuse_trans_active)) {
                ek::masked(value, diffuse_trans_active) +=
                    (1.f - strans_w) * diff_transw * color * ek::InvPi<Float> *
                    ek::abs(cos_theta_o);
            }

            return depolarizer<Spectrum>(value) & active;
        } else {

            Float metallic_w =
                has_metallic ? metallic->eval_1(si, active) : Float(0.f);
            Float clearcoat_w = has_clearcoat ? clearcoat->eval_1(si, active)
                                              : Float(0.f),
                  sheen_w = has_sheen ? sheen->eval_1(si, active) : Float(0.f);
            Float brdf_w  = (1.0f - metallic_w) * (1.0f - strans_w),
                  bsdf_w  = (1.0f - metallic_w) * strans_w;



            Float cos_theta_o = Frame3f::cos_theta(wo);

            Mask reflect = cos_theta_i * cos_theta_o > 0.f;
            Mask refract = cos_theta_i * cos_theta_o < 0.f;

            // masks if incident direction is inside (wi.z<0)
            Mask front_side = cos_theta_i > 0;
            Float inv_eta   = ek::rcp(eta);
            // eta value w.r.t. ray instead of the object.
            Float eta_path     = ek::select(front_side, eta, inv_eta);
            Float inv_eta_path = ek::select(front_side, inv_eta, eta);

            // main specular reflection and transmission lobe
            auto [ax, ay] = calc_dist_params(anisotropic_w, roughness_w);
            MicrofacetDistribution spec_dist(MicrofacetType::GGX, ax, ay);

            // halfway vector
            Vector3f wh = ek::normalize(
                si.wi + wo * ek::select(reflect, Float(1.f), eta_path));

            // make sure that the halfway vector points outwards the object
            wh = ek::mulsign(wh, Frame3f::cos_theta(wh));

            // dielectric Fresnel
            auto [F_spec_dielectric, cos_theta_t, eta_it, eta_ti] =
                fresnel(ek::dot(si.wi, wh), eta);

            // masks for evaluating the lobes.
            Mask spec_reflect_active =
                active && reflect &&
                ek::dot(si.wi, ek::mulsign(wh, cos_theta_i)) > 0.f &&
                ek::dot(wo, ek::mulsign(wh, cos_theta_i)) > 0.f &&
                F_spec_dielectric > 0.0f;

            Mask clearcoat_active =
                has_clearcoat
                    ? active && (clearcoat_w > 0.f) && reflect &&
                          ek::dot(si.wi, ek::mulsign(wh, cos_theta_i)) > 0.f &&
                          ek::dot(wo, ek::mulsign(wh, cos_theta_i)) > 0.f &&
                          front_side
                    : 0;

            Mask spec_trans_active =
                has_spec_trans
                    ? active && (bsdf_w > 0.0f) && refract &&
                          ek::dot(si.wi, ek::mulsign(wh, cos_theta_i)) > 0.f &&
                          ek::dot(wo, ek::mulsign_neg(wh, cos_theta_i)) > 0.f &&
                          F_spec_dielectric < 1.f
                    : 0;

            Mask diffuse_active =
                active && (brdf_w > 0.f) && reflect &&
                front_side; // diffuse - retro and Hanrahan Krueger
            Mask sheen_active = has_sheen
                                    ? active && (sheen_w > 0.f) && reflect &&
                                          (1.f - metallic_w > 0.f) && front_side
                                    : 0;

            // Evaluate the microfacet normal distribution
            Float D = spec_dist.eval(wh);

            // Smith's shadow-masking function
            Float G = spec_dist.G(si.wi, wo, wh);

            UnpolarizedSpectrum value(0.f);

            // main specular reflection evaluation
            if (ek::any_or<true>(spec_reflect_active)) {
                Float lum(0.f);
                Float stint_w(0.f);
                // store the needed quantities if spec_tint is active.
                if (has_spec_tint) {
                    if constexpr (is_rgb_v<Spectrum>)
                        lum = mitsuba::luminance(color);
                    else
                        lum = mitsuba::luminance(color, si.wavelengths);
                    stint_w = spec_tint->eval_1(si, active);
                }
                // Fresnel term
                UnpolarizedSpectrum F_disney = disney_fresnel(
                    F_spec_dielectric, metallic_w, stint_w, eta, color, lum,
                    ek::dot(si.wi, wh), front_side, bsdf_w);

                // Calculate the specular reflection component
                ek::masked(value, spec_reflect_active) +=
                    F_disney * D * G / (4.f * ek::abs(cos_theta_i));
            }

            // main specular transmission evaluatuion
            if (has_spec_trans && ek::any_or<true>(spec_trans_active)) {

                /* account for the solid angle compression when tracing radiance
                 * -- this is necessary for bidirectional methods. */
                Float scale = (ctx.mode == TransportMode::Radiance)
                                  ? ek::sqr(inv_eta_path)
                                  : Float(1.f);

                // Calculate the specular transmission component
                ek::masked(value, spec_trans_active) +=
                    ek::sqrt(color) * bsdf_w *
                    ek::abs(
                        (scale * (1.f - F_spec_dielectric) * D * G * eta_path *
                         eta_path * ek::dot(si.wi, wh) * ek::dot(wo, wh)) /
                        (cos_theta_i * ek::sqr(ek::dot(si.wi, wh) +
                                               eta_path * ek::dot(wo, wh))));
            }

            // secondary isotropic specular reflection
            if (has_clearcoat && ek::any_or<true>(clearcoat_active)) {

                Float clearcoat_glossw = clearcoat_gloss->eval_1(si, active);
                Float Fcc = calc_schlick<Float>(0.04f, ek::dot(si.wi, wh), eta);

                GTR1 mfacet_dist(ek::lerp(0.1f, 0.001f, clearcoat_glossw));

                Float D_cc = mfacet_dist.eval(wh);
                Float G_cc = clearcoat_G(si.wi, wo, 0.25f);

                ek::masked(value, clearcoat_active) += (clearcoat_w * 0.25f) *
                                                       Fcc * D_cc * G_cc *
                                                       ek::abs(cos_theta_o);
            }

            // evaluation of diffuse, retroreflection, fake subsurface and sheen
            // evaluation
            if (ek::any_or<true>(diffuse_active)) {

                // diffuse and retro reflection part
                Float Fo = schlick_weight(ek::abs(cos_theta_o)),
                      Fi = schlick_weight(ek::abs(cos_theta_i));

                // diffuse
                UnpolarizedSpectrum f_diff =
                    (1.0f - 0.5f * Fi) * (1.0f - 0.5f * Fo);

                Float cos_theta_d = ek::dot(ek::mulsign(wh, cos_theta_i), wo);

                Float Rr = 2.0f * roughness_w * ek::sqr(cos_theta_d);

                // retroreflection
                UnpolarizedSpectrum f_retro =
                    Rr * (Fo + Fi + Fo * Fi * (Rr - 1.0f));

                if (has_flatness) {
                    // fake subsurface implementation based on Hanrahan Krueger
                    // Fss90 used to "flatten" retroreflection based on
                    // roughness
                    Float Fss90 = cos_theta_d * cos_theta_d * roughness_w;
                    Float Fss =
                        ek::lerp(1.0, Fss90, Fo) * ek::lerp(1.0, Fss90, Fi);

                    Float f_ss = 1.25f * (Fss * (1 / (ek::abs(cos_theta_o) +
                                                      ek::abs(cos_theta_i)) -
                                                 .5f) +
                                          .5f);

                    ek::masked(value, diffuse_active) +=
                        brdf_w * ek::abs(cos_theta_o) * color *
                        ek::InvPi<Float> *
                        (ek::lerp(f_diff, f_ss, flatness_w) + f_retro);
                } else {
                    ek::masked(value, diffuse_active) +=
                        brdf_w * ek::abs(cos_theta_o) * color *
                        ek::InvPi<Float> * (f_diff + f_retro);
                }

                // sheen evaluation
                if (has_sheen && ek::any_or<true>(sheen_active)) {

                    Float Fd = schlick_weight(ek::abs(cos_theta_d));

                    // tint the sheen evaluation towards the base color.
                    if (has_sheen_tint) {
                        Float sheen_tintw = sheen_tint->eval_1(si, active);
                        Float lum(1.f);
                        if constexpr (is_rgb_v<Spectrum>)
                            lum = mitsuba::luminance(color);
                        else
                            lum = mitsuba::luminance(color, si.wavelengths);

                        UnpolarizedSpectrum c_tint = ek::select(
                            lum > 0.0f, color / lum, UnpolarizedSpectrum(1.f));

                        UnpolarizedSpectrum c_sheen = ek::lerp(
                            UnpolarizedSpectrum(1.f), c_tint, sheen_tintw);

                        ek::masked(value, sheen_active) +=
                            sheen_w * (1.f - metallic_w) * Fd * c_sheen *
                            ek::abs(cos_theta_o);
                    } else {
                        ek::masked(value, sheen_active) +=
                            sheen_w * (1.f - metallic_w) * Fd *
                            ek::abs(cos_theta_o);
                    }
                }
            }
            return depolarizer<Spectrum>(value) & active;
        }
    }

    Float pdf(const BSDFContext &, const SurfaceInteraction3f &si,
              const Vector3f &wo, Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        Float cos_theta_i = Frame3f::cos_theta(si.wi);

        // Ignore perfectly grazing configurations
        active &= ek::neq(cos_theta_i, 0.f);

        if (unlikely(ek::none_or<false>(active)))
            return 0.f;

        // store the weights as Float
        Float anisotropic_w = has_anisotropic ? anisotropic->eval_1(si, active)
                                              : Float(0.f),
              roughness_w   = roughness->eval_1(si, active),
              strans_w =
                  has_spec_trans ? spec_trans->eval_1(si, active) : Float(0.f);

        if (thin) { // thin model's pdf

            Float eta_t        = eta_thin->eval_1(si, active);
            Float diff_trans_w = has_diff_trans
                                     ? diff_trans->eval_1(si, active) / 2.f
                                     : Float(0.f);

            // Changing the signs in a way that we are always at the front side.
            // Thin BSDF is symmetric !!
            Vector3f wi   = ek::mulsign(si.wi, cos_theta_i);
            Vector3f wo_t = ek::mulsign(wo, cos_theta_i);

            Float cos_theta_o = Frame3f::cos_theta(wo_t);

            Mask reflect = cos_theta_o > 0.f;
            Mask refract = cos_theta_o < 0.f;

            // probability definitions
            Float prob_spec_reflect =
                has_spec_trans ? strans_w * spec_reflect_sampling_rate
                               : Float(0.f);
            Float prob_spec_trans = has_spec_trans
                                        ? strans_w * spec_trans_sampling_rate
                                        : Float(0.f);
            // since we have more lobes in reflection part (diffuse, sheen and
            // retro) that are unaffacted by the transmission,  reflection  has
            // 1/3 prob even when  diff_trans is 1.(given that the all sampling
            // rates are 1)
            Float prob_coshemi_reflect = diffuse_reflectance_sampling_rate *
                                         (1.f - strans_w) *
                                         (3 - 2 * diff_trans_w) / 3.f;
            Float prob_coshemi_trans =
                has_diff_trans
                    ? diffuse_transmittance_sampling_rate * (1.f - strans_w) *
                          2.f * (diff_trans_w) / 3.f
                    : Float(0.f);

            // normalizing the probabilities
            Float total_prob = prob_spec_reflect + prob_spec_trans +
                               prob_coshemi_reflect + prob_coshemi_trans;

            prob_spec_reflect /= total_prob;
            prob_spec_trans /= total_prob;
            prob_coshemi_reflect /= total_prob;
            prob_coshemi_trans /= total_prob;

            Float pdf(0.f);
            // specular lobe pdf evaluations
            if (has_spec_trans) {
                // halfway vector
                Vector3f wo_r = wo_t;
                wo_r.z()      = ek::abs(wo_r.z());
                Vector3f wh   = ek::normalize(wi + wo_r);

                // macro-micro surface masks for lobes
                Mask mfacet_reflect_macmic =
                    ek::dot(wh, wi) > 0.f && ek::dot(wo_t, wh) > 0.f && reflect;

                Mask mfacet_trans_macmic = ek::dot(wi, wh) > 0.f &&
                                           ek::dot(-wo_t, wh) > 0.f && refract;

                Float dot_wor_wh  = ek::dot(wo_r, wh);
                Float dwh_dwo_abs = ek::abs(ek::rcp(4.f * dot_wor_wh));

                auto [ax, ay] = calc_dist_params(anisotropic_w, roughness_w);
                MicrofacetDistribution spec_reflect_distr(MicrofacetType::GGX,
                                                          ax, ay);

                // Defining the scaled distribution for thin specular reflection
                // Scale roughness based on IOR (Burley 2015, Figure 15).
                Float rougness_scaled = (0.65f * eta_t - 0.35f) * roughness_w;
                auto [ax_scaled, ay_scaled] =
                    calc_dist_params(anisotropic_w, rougness_scaled);
                MicrofacetDistribution spec_trans_distr(MicrofacetType::GGX,
                                                        ax_scaled, ay_scaled);

                ek::masked(pdf, mfacet_reflect_macmic) +=
                    prob_spec_reflect * spec_reflect_distr.pdf(wi, wh) *
                    dwh_dwo_abs;

                ek::masked(pdf, mfacet_trans_macmic) +=
                    prob_spec_trans * spec_trans_distr.pdf(wi, wh) *
                    dwh_dwo_abs;
            }

            // cosine hemisphere reflection pdf
            ek::masked(pdf, reflect) +=
                prob_coshemi_reflect *
                warp::square_to_cosine_hemisphere_pdf(wo_t);

            // cosine hemisphere transmission pdf
            if (has_diff_trans) {
                ek::masked(pdf, refract) +=
                    prob_coshemi_trans *
                    warp::square_to_cosine_hemisphere_pdf(-wo_t);
            }

            return pdf;

        } else { // 3D case


            Float metallic_w =
                      has_metallic ? metallic->eval_1(si, active) : Float(0.f),
                  clearcoat_w = has_clearcoat ? clearcoat->eval_1(si, active)
                                              : Float(0.f);
            Float brdf_w      = (1.0f - metallic_w) * (1.0f - strans_w),
                  bsdf_w      = (1.0f - metallic_w) * strans_w;

            // masks if incident direction is inside (wi.z<0)
            Mask front_side = cos_theta_i > 0;
            Float eta_path  = ek::select(front_side, eta, ek::rcp(eta));

            Float cos_theta_o = Frame3f::cos_theta(wo);

            Mask reflect = cos_theta_i * cos_theta_o > 0.f;
            Mask refract = cos_theta_i * cos_theta_o < 0.f;

            // halfway vector
            Vector3f wh = ek::normalize(
                si.wi + wo * ek::select(reflect, Float(1.f), eta_path));

            // make sure that the halfway vector points outwards the object
            wh = ek::mulsign(wh, Frame3f::cos_theta(wh));

            // main specular reflection
            auto [ax, ay] = calc_dist_params(anisotropic_w, roughness_w);
            MicrofacetDistribution spec_distr(MicrofacetType::GGX, ax, ay);

            // Dielectric Fresnel calculation
            auto [F_spec_dielectric, cos_theta_t, eta_it, eta_ti] =
                fresnel(ek::dot(si.wi, wh), eta);

            // defining the probabilties
            Float prob_spec_reflect =
                ek::select(front_side,
                           main_specular_sampling_rate *
                               (1.f - bsdf_w * (1 - F_spec_dielectric)),
                           F_spec_dielectric);
            Float prob_spec_trans =
                has_spec_trans ? ek::select(front_side,
                                 main_specular_sampling_rate * bsdf_w *
                                     (1 - F_spec_dielectric),
                                 (1.f - F_spec_dielectric))
                    : Float(0.f);
            Float prob_clearcoat =
                has_clearcoat
                    ? ek::select(front_side,
                                 0.25f * clearcoat_w * clearcoat_sampling_rate,
                                 0.f)
                    : Float(0.f);
            Float prob_diffuse = ek::select(
                front_side, brdf_w * diffuse_reflectance_sampling_rate, 0.f);

            // normalizing the probabilities
            Float tot_prob = prob_spec_reflect + prob_spec_trans +
                             prob_clearcoat + prob_diffuse;

            prob_spec_reflect /= tot_prob;
            prob_spec_trans /= tot_prob;
            prob_clearcoat /= tot_prob;
            prob_diffuse /= tot_prob;

            Float dwh_dwo_abs;
            if (has_spec_trans) {
                Float dot_wi_h = ek::dot(si.wi, wh);
                Float dot_wo_h = ek::dot(wo, wh);
                dwh_dwo_abs    = ek::abs(
                       ek::select(reflect, ek::rcp(4.f * dot_wo_h),
                                  (eta_path * eta_path * dot_wo_h) /
                                      ek::sqr(dot_wi_h + eta_path * dot_wo_h)));
            } else {
                dwh_dwo_abs = ek::abs(ek::rcp(4.f * ek::dot(wo, wh)));
            }

            Float pdf(0.f);

            // macro-micro surface masks for lobes
            Mask mfacet_reflect_macmic =
                ek::dot(si.wi, ek::mulsign(wh, cos_theta_i)) > 0.f &&
                ek::dot(wo, ek::mulsign(wh, cos_theta_i)) > 0.f && reflect;

            // main specular reflection pdf
            ek::masked(pdf, mfacet_reflect_macmic) +=
                prob_spec_reflect *
                spec_distr.pdf(ek::mulsign(si.wi, cos_theta_i), wh) *
                dwh_dwo_abs;

            // cosine hemisphere reflection pdf
            ek::masked(pdf, reflect) +=
                prob_diffuse * warp::square_to_cosine_hemisphere_pdf(
                                   ek::mulsign(wo, cos_theta_o));

            // main specular transmission pdf
            if (has_spec_trans) {
                Mask mfacet_trans_macmic =
                    ek::dot(si.wi, ek::mulsign(wh, cos_theta_i)) > 0.f &&
                    ek::dot(wo, ek::mulsign_neg(wh, cos_theta_i)) > 0.f &&
                    refract;

                ek::masked(pdf, mfacet_trans_macmic) +=
                    prob_spec_trans *
                    spec_distr.pdf(ek::mulsign(si.wi, cos_theta_i), wh) *
                    dwh_dwo_abs;
            }

            // the secondary specular reflection pdf
            if (has_clearcoat) {
                Float clearcoat_glossw = clearcoat_gloss->eval_1(si, active);
                GTR1 cc_dist(ek::lerp(0.1f, 0.001f, clearcoat_glossw));
                ek::masked(pdf, mfacet_reflect_macmic) +=
                    prob_clearcoat * cc_dist.pdf(wh) * dwh_dwo_abs;
            }

            return pdf;
        }
    }

    std::pair<Spectrum, Float> eval_pdf(const BSDFContext &ctx,
                                        const SurfaceInteraction3f &si,
                                        const Vector3f &wo,
                                        Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        return { this->eval(ctx, si, wo, active),
                 this->pdf(ctx, si, wo, active) };
    }

    void traverse(TraversalCallback *callback) override {

        /*The parameters are not traversed if they are not active! */
        if (!thin) {// 3d traversed parameters
            callback->put_object("clearcoat", clearcoat.get());
            callback->put_object("clearcoat_gloss", clearcoat_gloss.get());
            callback->put_object("spec_tint", spec_tint.get());
            callback->put_object("metallic", metallic.get());

            callback->put_parameter("main_specular_sampling_rate",
                                    main_specular_sampling_rate);
            callback->put_parameter("clearcoat_sampling_rate",
                                    clearcoat_sampling_rate);

            if (eta_specular) // Only one of them traversed based on xml flie!
                callback->put_parameter("eta", eta);
            else
                callback->put_parameter("specular", specular);

        } else { // thin parameters
                callback->put_object("diff_trans", diff_trans.get());
                callback->put_object("eta", eta_thin.get());

                callback->put_parameter("spec_reflect_sampling_rate",
                                        spec_reflect_sampling_rate);
                callback->put_parameter("diffuse_transmittance_sampling_rate",
                                        diffuse_transmittance_sampling_rate);
                callback->put_parameter("spec_trans_sampling_rate",
                                        spec_trans_sampling_rate);
        }

        // common parameters
        callback->put_object("base_color", base_color.get());
        callback->put_object("roughness", roughness.get());
        callback->put_object("anisotropic", anisotropic.get());
        callback->put_object("sheen", sheen.get());
        callback->put_object("sheen_tint", sheen_tint.get());
        callback->put_object("spec_trans", spec_trans.get());
        callback->put_object("flatness", flatness.get());

        callback->put_parameter("diffuse_reflectance_sampling_rate",
                                diffuse_reflectance_sampling_rate);

    }

    void parameters_changed(const std::vector<std::string> & keys = {}) override {

        // in case the parameters are changed from zero to something else
        // boolean flags need to be changed also.
        if(thin){

            if (string::contains(keys, "spec_trans")) has_spec_trans = true;
            if (string::contains(keys, "diff_trans")) has_diff_trans = true;
            if (string::contains(keys, "sheen")) has_sheen = true;
            if (string::contains(keys, "sheen_tint")) has_sheen_tint = true;
            if (string::contains(keys, "anisotropic")) has_anisotropic = true;
            if (string::contains(keys, "flatness")) has_flatness = true;
        }
        else{

            if (string::contains(keys, "spec_trans")) has_spec_trans = true;
            if (string::contains(keys, "clearcoat")) has_clearcoat = true;
            if (string::contains(keys, "sheen")) has_sheen = true;
            if (string::contains(keys, "sheen_tint")) has_sheen_tint = true;
            if (string::contains(keys, "anisotropic")) has_anisotropic = true;
            if (string::contains(keys, "metallic")) has_metallic = true;
            if (string::contains(keys, "spec_tint")) has_spec_tint = true;
            if (string::contains(keys, "flatness")) has_flatness = true;

            if(!eta_specular && string::contains(keys, "specular")) {
                // 0 specular is corresponding to eta=1 which is not plausible for transmission
                ek::masked(specular,specular==0.f) = 1e-3f;
                eta = 2.f * ek::rcp(1.f - ek::sqrt(0.08f * specular)) - 1.f;
                ek::make_opaque(eta,specular);
            }
            if(eta_specular && string::contains(keys, "eta")){
                ek::masked(eta,eta==1.f) = 1.001f;
                ek::make_opaque(eta);
            }
        }
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Disney:" << std::endl;

        if (thin) {
            oss << "2D Thin Principled BSDF :" << std::endl
                << "base_color: " << base_color << "," << std::endl
                << "spec_trans: " << spec_trans << "," << std::endl
                << "diff_trans: " << diff_trans << "," << std::endl
                << "anisotropic: " << anisotropic << "," << std::endl
                << "roughness: " << roughness << "," << std::endl
                << "sheen: " << sheen << "," << std::endl
                << "sheen_tint: " << sheen_tint << "," << std::endl
                << "flatness: " << flatness << "," << std::endl
                << "eta: " << eta_thin << "," << std::endl;
        } else {
            oss << "3D Principled BSDF :" << std::endl
                << "base_color: " << base_color << "," << std::endl
                << "spec_trans: " << spec_trans << "," << std::endl
                << "anisotropic: " << anisotropic << "," << std::endl
                << "roughness: " << roughness << "," << std::endl
                << "sheen: " << sheen << "," << std::endl
                << "sheen_tint: " << sheen_tint << "," << std::endl
                << "flatness: " << flatness << "," << std::endl;

            if (eta_specular)
                oss << "eta: " << eta << "," << std::endl;
            else
                oss << "specular: " << specular << "," << std::endl;

            oss << "clearcoat: " << clearcoat << "," << std::endl
                << "clearcoat_gloss: " << clearcoat_gloss << "," << std::endl
                << "metallic: " << metallic << "," << std::endl
                << "spec_tint: " << spec_tint << "," << std::endl;
        }
        return oss.str();
    }

    MTS_DECLARE_CLASS()
private:
    // whether the used model is 2D thin or 3D principled-BSDF.
    bool thin;
    // Parameters that are both used in 3D and thin.
    ref<Texture> base_color;
    ref<Texture> roughness;
    ref<Texture> anisotropic;
    ref<Texture> sheen;
    ref<Texture> sheen_tint;
    ref<Texture> spec_trans;
    ref<Texture> flatness;

    // parameters peculiar to the thin model
    ref<Texture> diff_trans;
    ref<Texture> eta_thin;

    // parameters peculiar to 3D bsdf
    ref<Texture> clearcoat;
    ref<Texture> clearcoat_gloss;
    ref<Texture> metallic;
    ref<Texture> spec_tint;
    Float eta;
    Float specular;
    bool eta_specular;

    // sampling rates that are both used in 3D and thin
    Float diffuse_reflectance_sampling_rate;

    // sampling rates peculiar to the thin model
    Float main_specular_sampling_rate;
    Float clearcoat_sampling_rate;

    // sampling rates peculiar to 3D bsdf
    Float spec_reflect_sampling_rate;
    Float spec_trans_sampling_rate;
    Float diffuse_transmittance_sampling_rate;

    // whether the parameters are active or not. (specified by the input xml
    // file.)
    bool has_clearcoat;
    bool has_sheen;
    bool has_diff_trans;
    bool has_spec_trans;
    bool has_metallic;
    bool has_spec_tint;
    bool has_sheen_tint;
    bool has_anisotropic;
    bool has_flatness;
};

MTS_IMPLEMENT_CLASS_VARIANT(Disney, BSDF)
MTS_EXPORT_PLUGIN(Disney, "Disney Material")
NAMESPACE_END(mitsuba)