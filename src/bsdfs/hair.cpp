#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/fresnel.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/render/ior.h>
#include <mitsuba/render/srgb.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _bsdf-hair:

Hair material (:monosp:`hair`)
-------------------------------------------

.. pluginparameters::
 :extra-rows: 6

 * - eumelanin, pheomelanin
   - |float|
   - Concentration of pigments (Default: 1.3 eumelanin and 0.2 pheomelanin)
   - |exposed|, |differentiable|, |discontinuous|

 * - sigma_a
   - |spectrum| or |texture|
   - Absorption coefficient in inverse scene units. The absorption can either
     be specified with pigmentation concentrations or this parameter, not both.
   - |exposed|, |differentiable|

 * - scale
   - |float|
   - Optional scale factor that will be applied to the `sigma_a` parameter.
     (Default :1)
   - |exposed|

 * - int_ior
   - |float| or |string|
   - Interior index of refraction specified numerically or using a known
     material name. (Default: amber / 1.55f)

 * - ext_ior
   - |float| or |string|
   - Exterior index of refraction specified numerically or using a known
     material name.  (Default: air / 1.000277)

 * - longitudinal_roughness, azimuthal_roughness
   - |float|
   - Hair roughness along each dimension (Default: 0.3 for both)
   - |exposed|, |differentiable|, |discontinuous|

 * - scale_tilt
   - |float|
   - Angle of the scales on the hair w.r.t. to the hair fiber's surface. The
     angle is given in degrees. (Default: 2)
   - |exposed|, |differentiable|, |discontinuous|

 * - use_pigmentation
   - |bool|
   - Specifies whether to use the pigmentation concentration values or the
     absorption coefficient `sigma_a`
   - |exposed|, |differentiable|, |discontinuous|

 * - eta
   - |float|
   - Relative index of refraction from the exterior to the interior
   - |exposed|, |differentiable|, |discontinuous|

This plugin is an implementation of the hair model described in the paper *A
Practical and Controllable Hair and Fur Model for Production Path Tracing* by
Chiang et al. :cite:`Chiang2016hair`.

When no parameters are given, the plugin activates the default settings,
which describe a brown-ish hair color.

This BSDF is meant to be used with the `bsplinecurve` shape. Attaching this
material to any other shape might produce unexpected results. This is due to
assumptions about the local geoemtry and frame in the model itself.

This model is considered a "near field" model: the viewer can be close to the
hair fibers and observe accurate scattering across the entire fiber. In its
implementation, a single scattering event computes the outgoing radiance by
assuming no/one/many event(s) inside the fiber and then exiting it. In short,
a single interaction encapsulates the entire walk inside fiber. Unintuitively,
this also means that the entry and exit point of the walk are exactly the same
point on the shape's geometry.

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/bsdf_hair_blonde.jpg
   :caption: Low pigmentation concentrations
.. subfigure:: ../../resources/data/docs/images/render/bsdf_hair_brown.jpg
   :caption: Higher pigmentation concentrations
.. subfigend::
   :label: fig-hair

.. note:: The hair geometry used in the figures above was created by `Benedikt Bitterli <https://benedikt-bitterli.me>`_.

.. tabs::
    .. code-tab:: xml
        :name: lst-hair-xml

        <bsdf type="hair">
            <float name="eumelanin" value="0.2"/>
            <float name="pheomelanin" value="0.4"/>
        </bsdf>

    .. code-tab:: python

        'type': 'hair',
        'eumelanin': 0.2,
        'pheomelanin': 0.4

Alternatively, the absorption coefficient can be specified manually:

.. tabs::
    .. code-tab:: xml
        :name: lst-hair-sigma

        <bsdf type="hair">
            <rgb name="sigma_a" value="0.2, 0.25, 0.7"/>
            <float name="scale" value="200"/>
        </bsdf>

    .. code-tab:: python

        'type': 'hair',
        'sigma_a': {
            'type': 'rgb',
            'value': [0.2, 0.25, 0.7]
        },
        'scale': 200
*/
template <typename Float, typename Spectrum>
class Hair final : public BSDF<Float, Spectrum> {
public:
    MI_IMPORT_BASE(BSDF, m_flags, m_components)
    MI_IMPORT_TYPES(Texture)


    /* Implementation notes:

     Internally the methods use a special frame where `theta` refers to the
     longitudinal angle and `phi` to the azimuthal angle. This frame is *not*
     a spherical coordinate system. The longitudinal angle is defined as the
     angle between a given direction `w` and the "normal plane" (cross section
     of a fiber, perpendicular to the fiber's length). As for the azimuthal
     angle, it is defined as the angle between `w` projected into the normal
     plane and some arbitrary reference direction within that plane (local `y`
     coordinate axis, equivalent to the normal direction).
    */

    Hair(const Properties &props) : Base(props) {
        // Roughness (longitudinal & azimuthal)
        ScalarFloat longitudinal_roughness =
            props.get<ScalarFloat>("longitudinal_roughness", 0.3f);
        m_longitudinal_roughness = longitudinal_roughness;
        ScalarFloat azimuthal_roughness =
            props.get<ScalarFloat>("azimuthal_roughness", 0.3f);
        m_azimuthal_roughness = azimuthal_roughness;

        // Scale tilt
        m_alpha = props.get<ScalarFloat>("scale_tilt", 2.f);

        // Indices of refraction at the interface
        ScalarFloat ext_ior = lookup_ior(props, "ext_ior", "air");
        ScalarFloat int_ior = lookup_ior(props, "int_ior", "amber");
        m_eta = int_ior / ext_ior;

        // Absorption
        m_eumelanin = props.get<ScalarFloat>("eumelanin", 1.3f);
        m_pheomelanin = props.get<ScalarFloat>("pheomelanin", 0.2f);
        if (props.has_property("sigma_a")){
            m_sigma_a = props.texture<Texture>("sigma_a");
            m_use_pigmentation = false;
        }
        m_scale = props.get<ScalarFloat>("scale", 1.f);

        if (longitudinal_roughness < 0 || longitudinal_roughness > 1.f)
            Throw("The longitudinal roughness should be in the range [0, 1]!");
        if (azimuthal_roughness < 0 || azimuthal_roughness > 1.f)
            Throw("The azimuthal roughness should be in the range [0, 1]!");
        if (int_ior < 0.f || ext_ior < 0.f || int_ior == ext_ior)
            Throw("The interior and exterior indices of refraction must be "
                  "positive and differ!");
        if (props.has_property("sigma_a") &&
            (props.has_property("eumelanin") ||
             props.has_property("pheomelanin")))
            Throw("Only one of pigmentation or aborption can be specified, not "
                  "both!");

        dr::make_opaque(m_eumelanin, m_pheomelanin, m_sigma_a);

        m_components.push_back(BSDFFlags::Glossy | BSDFFlags::Anisotropic |
                               BSDFFlags::NonSymmetric | BSDFFlags::FrontSide);
        m_components.push_back(BSDFFlags::Null | BSDFFlags::BackSide);

        m_flags = m_components[0] | m_components[1];

        update();
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_parameter("longitudinal_roughness", m_longitudinal_roughness, ParamFlags::Differentiable | ParamFlags::Discontinuous);
        callback->put_parameter("azimuthal_roughness",    m_azimuthal_roughness,    ParamFlags::Differentiable | ParamFlags::Discontinuous);
        callback->put_parameter("scale_tilt",             m_alpha,                  ParamFlags::Differentiable | ParamFlags::Discontinuous);
        callback->put_parameter("eta",                    m_eta,                    ParamFlags::Differentiable | ParamFlags::Discontinuous);
        callback->put_parameter("eumelanin",              m_eumelanin,              ParamFlags::Differentiable | ParamFlags::Discontinuous);
        callback->put_parameter("pheomelanin",            m_pheomelanin,            ParamFlags::Differentiable | ParamFlags::Discontinuous);
        callback->put_parameter("use_pigmentation",       m_use_pigmentation,      +ParamFlags::NonDifferentiable);
        callback->put_object("sigma_a",                   m_sigma_a,               +ParamFlags::Differentiable);
        callback->put_parameter("scale",                  m_scale,                 +ParamFlags::NonDifferentiable);
    }

    void parameters_changed(const std::vector<std::string> &keys = {}) override {
        if (keys.empty() || string::contains(keys, "longitudinal_roughness") ||
            string::contains(keys, "azimuthal_roughness") ||
            string::contains(keys, "scale_tilt")) {
            dr::make_opaque(m_longitudinal_roughness, m_azimuthal_roughness, m_alpha);
            update();
        }

        dr::make_opaque(m_eumelanin, m_pheomelanin, m_sigma_a);
    }

    std::pair<BSDFSample3f, Spectrum>
    sample(const BSDFContext &ctx, const SurfaceInteraction3f &si,
           Float sample1, const Point2f &sample2, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFSample, active);

        BSDFSample3f bs = dr::zeros<BSDFSample3f>(dr::width(si));

        if (unlikely(dr::none_or<false>(active) ||
                     !ctx.is_enabled(BSDFFlags::Glossy)))
            return { bs, 0.f };

        // Parameterization of incident direction
        Float gamma_i = gamma(si.wi);
        Float h = dr::sin(gamma_i);
        auto [sin_theta_i, cos_theta_i] = sincos_theta(si.wi);
        Float phi_i = azimuthal_angle(si.wi);
        auto [sin_phi_i, cos_phi_i] = dr::sincos(phi_i);

        // Sample segment length `p`
        dr::Array<Float, P_MAX + 1> a_p_pdf = attenuation_pdf(cos_theta_i, si);

        Point2f u[2] = { { sample1, 0 }, sample2 };
        // u[0][1] is the rescaled random number after using u[0][0]
        u[0][1] = u[0][0] / a_p_pdf[0];

        UInt32 p = dr::zeros<UInt32>(dr::width(si));
        for (size_t i = 0; i < P_MAX; ++i) {
            Bool sample_p = a_p_pdf[i] < u[0][0];
            u[0][0] -= a_p_pdf[i];

            dr::masked(p, sample_p) = (uint32_t)i + 1;
            dr::masked(u[0][1], sample_p) = u[0][0] / a_p_pdf[i + 1];
        }

        // Account for scales on hair surface
        Float sin_theta_ip(0.f);
        Float cos_theta_ip(0.f);
        for (size_t j = 0; j < P_MAX; j++) {
            auto [sin_theta_ij, cos_theta_ij] =
                reframe_with_scales(sin_theta_i, cos_theta_i, j);
            dr::masked(sin_theta_ip, p == j) = sin_theta_ij;
            dr::masked(cos_theta_ip, p == j) = cos_theta_ij;
        }

        // Sample longitudinal scattering
        Float cos_theta =
            1 + m_v[P_MAX] *
                dr::log(u[1][0] + (1.f - u[1][0]) * dr::exp(-2.f / m_v[P_MAX]));
        for (size_t i = 0; i < P_MAX; i++)
            dr::masked(cos_theta, p == i) =
                1 + m_v[i] * dr::log(u[1][0] + (1.f - u[1][0]) * dr::exp(-2.f / m_v[i]));
        Float sin_theta = dr::safe_sqrt(1.f - dr::square(cos_theta));
        Float cos_phi = dr::cos(2 * dr::Pi<ScalarFloat> * u[1][1]);
        Float sin_theta_o =
            -cos_theta * sin_theta_ip + sin_theta * cos_phi * cos_theta_ip;
        Float cos_theta_o = dr::safe_sqrt(1.f - dr::square(sin_theta_o));

        // Transmission angle in azimuthal plane
        Float eta_p = azimuthal_ior(sin_theta_i, cos_theta_i);
        Float sin_gamma_t = h / eta_p;
        Float gamma_t = dr::safe_asin(sin_gamma_t);

        // Sample azimuthal scattering
        Float perfect_delta_phi =
            2 * p * gamma_t - 2 * gamma_i + p * dr::Pi<ScalarFloat>;
        Float delta_phi_first_terms =
            perfect_delta_phi + trimmed_logistic_sample(u[0][1], m_s);
        Float delta_phi_remainder = 2 * dr::Pi<ScalarFloat> * u[0][1];
        Float delta_phi =
            dr::select(p < P_MAX, delta_phi_first_terms, delta_phi_remainder);

        // Outgoing direction
        Float phi_o = phi_i + delta_phi;
        auto [sin_phi_o, cos_phi_o] = dr::sincos(phi_o);
        Vector3f wo(cos_theta_o * cos_phi_o, sin_theta_o,
                    cos_theta_o * sin_phi_o);

        // PDF for sampled outgoing direction
        for (size_t i = 0; i < P_MAX; ++i) {
            // Account for scales on hair surface
            auto [sin_theta_ip_, cos_theta_ip_] =
                reframe_with_scales(sin_theta_i, cos_theta_i, i);
            Vector3f wi_p(cos_theta_ip_ * cos_phi_i, sin_theta_ip_,
                          cos_theta_ip_ * sin_phi_i);

            bs.pdf += longitudinal_scattering(wi_p, wo, { 0, 1.f, 0 }, m_v[i]) *
                      dr::TwoPi<Float> * a_p_pdf[i] *
                      azimuthal_scattering(delta_phi, i, m_s, gamma_i, gamma_t);
        }
        bs.pdf +=
            longitudinal_scattering(si.wi, wo, { 0, 1.f, 0 }, m_v[P_MAX]) *
            a_p_pdf[P_MAX];

        bs.wo = dr::normalize(wo);
        bs.pdf = dr::select(dr::isnan(bs.pdf) || dr::isinf(bs.pdf), 0, bs.pdf);
        bs.eta = 1.f; // We always completely cross the hair fiber
        bs.sampled_type = +BSDFFlags::Glossy;
        bs.sampled_component = 0;

        Spectrum value = dr::select(
            bs.pdf != 0, eval(ctx, si, bs.wo, active) / bs.pdf, 0);

        return { bs, depolarizer<Spectrum>(value) & (active && bs.pdf > 0.f) };
    }

    Spectrum eval(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                  const Vector3f &wo, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        if (unlikely(dr::none_or<false>(active) ||
                     !ctx.is_enabled(BSDFFlags::Glossy)))
            return 0.f;

        // Parameterization of incident and outgoing directions
        Float gamma_i = gamma(si.wi);
        Float h = dr::sin(gamma_i);
        auto [sin_theta_i, cos_theta_i] = sincos_theta(si.wi);
        Float phi_i = azimuthal_angle(si.wi);
        auto [sin_phi_i, cos_phi_i] = dr::sincos(phi_i);
        Float phi_o = azimuthal_angle(wo);

        // Transmission angle in longitudinal plane
        Float sin_theta_t = sin_theta_i / m_eta;
        Float cos_theta_t = dr::safe_sqrt(1.f - dr::square(sin_theta_t));

        // Transmission angle in azimuthal plane
        Float eta_p =
            dr::safe_sqrt(dr::square(m_eta) - dr::square(sin_theta_i)) / cos_theta_i;
        Float sin_gamma_t = h / eta_p;
        Float cos_gamma_t = dr::safe_sqrt(1.f - dr::square(sin_gamma_t));
        Float gamma_t = dr::safe_asin(sin_gamma_t);

        // Attenuation coefficients
        UnpolarizedSpectrum sigma_a = absorption(si, active);
        Float transmitted_length = 2 * cos_gamma_t / cos_theta_t;
        UnpolarizedSpectrum transmittance = dr::exp(-sigma_a * transmitted_length);
        AttenuationCoeffs a_p =
            attenuation(cos_theta_i, m_eta, h, transmittance);

        // Contribution of first `P_MAX` terms
        Float delta_phi = phi_o - phi_i;
        UnpolarizedSpectrum value(0.0f);
        for (int p = 0; p < P_MAX; ++p) {
            // Account for scales on hair surface
            auto [sin_theta_ip, cos_theta_ip] =
                reframe_with_scales(sin_theta_i, cos_theta_i, p);
            Vector3f wi_p(cos_theta_ip * cos_phi_i, sin_theta_ip,
                          cos_theta_ip * sin_phi_i);

            value += longitudinal_scattering(wi_p, wo, { 0, 1.f, 0 }, m_v[p]) *
                     dr::TwoPi<Float> * a_p[p] *
                     azimuthal_scattering(delta_phi, p, m_s, gamma_i, gamma_t);
        }

        // Contribution of remaining terms
        value += longitudinal_scattering(si.wi, wo, { 0, 1.f, 0 }, m_v[P_MAX]) *
                a_p[P_MAX];

        value = dr::select(dr::isnan(value) || dr::isinf(value), 0, value);

        return depolarizer<Spectrum>(value) & active;
    }

    Float pdf(const BSDFContext &ctx, const SurfaceInteraction3f &si,
              const Vector3f &wo, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        if (unlikely(dr::none_or<false>(active) ||
                     !ctx.is_enabled(BSDFFlags::Glossy)))
            return 0.f;

        // Parameterization of incident and outgoing directions
        Float gamma_i = gamma(si.wi);
        Float h = dr::sin(gamma_i);
        auto [sin_theta_i, cos_theta_i] = sincos_theta(si.wi);
        Float phi_i = azimuthal_angle(si.wi);
        auto [sin_phi_i, cos_phi_i] = dr::sincos(phi_i);
        Float phi_o = azimuthal_angle(wo);

        // Transmission angle in azimuthal plane
        Float eta_p =
            dr::safe_sqrt(Float(m_eta * m_eta) - dr::square(sin_theta_i)) /
            cos_theta_i;
        Float sin_gamma_t = h / eta_p;
        Float gamma_t = dr::safe_asin(sin_gamma_t);

        // Attenuation PDF
        dr::Array<Float, P_MAX + 1> apPdf = attenuation_pdf(cos_theta_i, si);

        // Compute PDF sum for each segment length
        Float delta_phi  = phi_o - phi_i;
        Float pdf(0.0f);
        for (int p = 0; p < P_MAX; ++p) {
            // Account for scales on hair surface
            auto [sin_theta_ip, cos_theta_ip] =
                reframe_with_scales(sin_theta_i, cos_theta_i, p);
            Vector3f wi_p(cos_theta_ip * cos_phi_i, sin_theta_ip,
                          cos_theta_ip * sin_phi_i);

            pdf += longitudinal_scattering(wi_p, wo, { 0, 1.f, 0 }, m_v[p]) *
                    dr::TwoPi<Float> * apPdf[p] *
                    azimuthal_scattering(delta_phi, p, m_s, gamma_i, gamma_t);
        }
        pdf += longitudinal_scattering(si.wi, wo, { 0, 1.f, 0 }, m_v[P_MAX]) *
                apPdf[P_MAX];

        pdf = dr::select(dr::isnan(pdf) || dr::isinf(pdf), 0, pdf);
        return pdf;
    }

    std::pair<Spectrum, Float> eval_pdf(const BSDFContext &ctx,
                                        const SurfaceInteraction3f &si,
                                        const Vector3f &wo,
                                        Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        if (unlikely(dr::none_or<false>(active) ||
                     !ctx.is_enabled(BSDFFlags::Glossy)))
            return { 0.f, 0.f };

        Float gamma_i = gamma(si.wi);
        Float h = dr::sin(gamma_i);

        Float phi_o = azimuthal_angle(wo);

        // Parameterization of incident directions
        auto [sin_theta_i, cos_theta_i] = sincos_theta(si.wi);
        Float phi_i = azimuthal_angle(si.wi);
        auto [sin_phi_i, cos_phi_i] = dr::sincos(phi_i);

        // Transmission angle in longitudinal plane
        Float sin_theta_t = sin_theta_i / m_eta;
        Float cos_theta_t = dr::safe_sqrt(1.f - dr::square(sin_theta_t));

        // Transmission angle in azimuthal plane
        Float eta_p =
            dr::safe_sqrt(Float(m_eta * m_eta) - dr::square(sin_theta_i)) /
            cos_theta_i;
        Float sin_gamma_t = h / eta_p;
        Float cos_gamma_t = dr::safe_sqrt(1.f - dr::square(sin_gamma_t));
        Float gamma_t = dr::safe_asin(sin_gamma_t);

        // Attenuation coefficients
        UnpolarizedSpectrum sigma_a = absorption(si, active);
        Float transmitted_length = 2 * cos_gamma_t / cos_theta_t;
        UnpolarizedSpectrum transmittance = dr::exp(-sigma_a * transmitted_length);
        AttenuationCoeffs a_p =
            attenuation(cos_theta_i, Float(m_eta), h, transmittance);
        dr::Array<Float, P_MAX + 1> a_p_pdf = attenuation_pdf(cos_theta_i, si);

        // Accumulate PDF and contribution for each segment length
        Float delta_phi = phi_o - phi_i;
        Float pdf = Float(0.0f);
        UnpolarizedSpectrum value(0.0f);
        for (int p = 0; p < P_MAX; ++p) {
            auto [sin_theta_ip, cos_theta_ip] =
                reframe_with_scales(sin_theta_i, cos_theta_i, p);
            Vector3f wi_p(cos_theta_ip * cos_phi_i, sin_theta_ip,
                          cos_theta_ip * sin_phi_i);

            Float longitudinal =
                longitudinal_scattering(wi_p, wo, { 0, 1.f, 0 }, m_v[p]);
            Float azimuthal =
                azimuthal_scattering(delta_phi, p, m_s, gamma_i, gamma_t);

            pdf   += longitudinal * dr::TwoPi<Float> * a_p_pdf[p] * azimuthal;
            value += longitudinal * dr::TwoPi<Float> * a_p[p]     * azimuthal;
        }

        // Contribution and PDF of remaining terms
        Float longitudinal =
            longitudinal_scattering(si.wi, wo, { 0, 1.f, 0 }, m_v[P_MAX]);
        pdf += longitudinal * a_p_pdf[P_MAX];
        value += longitudinal * a_p[P_MAX];

        pdf   = dr::select(dr::isnan(pdf)   || dr::isinf(pdf),   0, pdf);
        value = dr::select(dr::isnan(value) || dr::isinf(value), 0, value);

        return { depolarizer<Spectrum>(value) & active,
                 dr::select(active, pdf, 0.f) };
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Hair["
            << std::endl
            << "]";
        return oss.str();
    }

private:
    /// Maximum depth (number of scattering events)
    constexpr static const int P_MAX = 3;
    static_assert(P_MAX >= 3, "There should be at least 3 segments!");

    /** Coefficients from "An Energy-Conserving Hair Reflectance Model" by
     *  d'Eon et al. (2011). Note: these are relative to the hair radius. */
    static inline const ScalarColor3f EUMELANIN_SIGMA_A   = { 0.419f, 0.697f,
                                                              1.37f };
    static inline const ScalarColor3f PHEOMELANIN_SIGMA_A = { 0.187f, 0.4f,
                                                              1.05f };

    /// Coefficients for spectral upsampling
    static inline const ScalarVector3f EUMELANIN_SRGB_COEFFS =
        srgb_model_fetch(EUMELANIN_SIGMA_A);
    static inline const ScalarVector3f PHEOMELANIN_SRGB_COEFFS =
        srgb_model_fetch(PHEOMELANIN_SIGMA_A);

    using AttenuationCoeffs = dr::Array<UnpolarizedSpectrum, P_MAX + 1>;

    /// Updates pre-computed internal state
    void update() {
        // Fill the `m_sin_2k_alpha` and `m_cos_2k_alpha` terms
        m_sin_2k_alpha[0] = dr::sin(dr::deg_to_rad(m_alpha));
        m_cos_2k_alpha[0] = dr::safe_sqrt(1.f - dr::square(m_sin_2k_alpha[0]));
        for (int i = 1; i < 3; ++i) {
            m_sin_2k_alpha[i] =
                2 * m_cos_2k_alpha[i - 1] * m_sin_2k_alpha[i - 1];
            m_cos_2k_alpha[i] =
                dr::square(m_cos_2k_alpha[i - 1]) - dr::square(m_sin_2k_alpha[i - 1]);
        }

        // Azimuthal logistic scale factor
        ScalarFloat sqrt_pi_over_8 = dr::sqrt(dr::Pi<ScalarFloat> / 8.f);
        m_s = sqrt_pi_over_8 * (0.265f * m_azimuthal_roughness +
                                1.194f * dr::square(m_azimuthal_roughness) +
                                5.372f * dr::pow(m_azimuthal_roughness, 22));

        // Longitudinal variance
        m_v[0] = dr::square(0.726f * m_longitudinal_roughness +
                         0.812f * dr::square(m_longitudinal_roughness) +
                         3.7f * dr::pow(m_longitudinal_roughness, 20));
        m_v[1] = .25f * m_v[0];
        m_v[2] = 4 * m_v[0];
        for (int p = 3; p <= P_MAX; ++p)
            m_v[p] = m_v[2];
    }

    /// Sine / cosine of longitudinal angle for direction `w`
    MI_INLINE std::pair<Float, Float> sincos_theta(const Vector3f &w) const {
        Float sin_theta = w.y();
        return {
            sin_theta,
            dr::safe_sqrt(dr::fnmadd(sin_theta, sin_theta, 1.f))
        };
    }

    /// Azimuthal angle for a direction in the local frame
    MI_INLINE Float azimuthal_angle(const Vector3f &w) const {
        return dr::atan2(w.z(), w.x());
    }

    /// Angle between surface normal and direction `w` in hair cross-section
    MI_INLINE Float gamma(const Vector3f &w) const {
        Float normal_plane_proj =
            dr::safe_sqrt(dr::square(w.x()) + dr::square(w.z()));
        Mask singularity = normal_plane_proj == 0;

        Float gamma = dr::safe_acos(w.z() / normal_plane_proj);
        gamma = dr::select(!singularity, gamma, 0);

        return dr::select(w.x() < 0, gamma, -gamma);
    }

    /// Modified index of refraction, considers projection in the normal plane
    MI_INLINE Float azimuthal_ior(Float sin_theta_i, Float cos_theta_i) const {
        return dr::safe_sqrt(dr::square(m_eta) - dr::square(sin_theta_i)) /
               cos_theta_i;
    }

    /// Return modified direction to account for angle of scales on hair surface
    std::pair<Float, Float>
    reframe_with_scales(Float sin_theta_i, Float cos_theta_i, size_t p) const {
        Float sin_theta_ip(0.0f), cos_theta_ip(0.0f);
        switch (p) {
            case 0:
                sin_theta_ip = sin_theta_i * m_cos_2k_alpha[1] -
                               cos_theta_i * m_sin_2k_alpha[1];
                cos_theta_ip = cos_theta_i * m_cos_2k_alpha[1] +
                               sin_theta_i * m_sin_2k_alpha[1];
                break;
            case 1:
                sin_theta_ip = sin_theta_i * m_cos_2k_alpha[0] +
                               cos_theta_i * m_sin_2k_alpha[0];
                cos_theta_ip = cos_theta_i * m_cos_2k_alpha[0] -
                               sin_theta_i * m_sin_2k_alpha[0];
                break;
            case 2:
                sin_theta_ip = sin_theta_i * m_cos_2k_alpha[2] +
                               cos_theta_i * m_sin_2k_alpha[2];
                cos_theta_ip = cos_theta_i * m_cos_2k_alpha[2] -
                               sin_theta_i * m_sin_2k_alpha[2];
                break;
            default:
                sin_theta_ip = sin_theta_i;
                cos_theta_ip = cos_theta_i;
                break;
        }
        cos_theta_ip = dr::abs(cos_theta_ip);

        return { sin_theta_ip, cos_theta_ip };
    };

    /// Returns the attenuation/absorption coefficients for each segment length
    AttenuationCoeffs
    attenuation(Float cos_theta_i, Float eta, Float h,
                const UnpolarizedSpectrum &transmittance) const {
        AttenuationCoeffs a_p = dr::zeros<AttenuationCoeffs>();

        Float cos_gamma_i = dr::safe_sqrt(1.f - dr::square(h));
        Float cos_theta = cos_theta_i * cos_gamma_i; // hair coordinate system

        Float f = std::get<0>(fresnel(cos_theta, eta));
        a_p[0] = f;
        a_p[1] = dr::square(1.f - f) * transmittance;
        for (int p = 2; p < P_MAX; ++p)
            a_p[p] = a_p[p - 1] * transmittance * f;

        // Sum of remaining possible lenghts (as `p` goes to infinity)
        a_p[P_MAX] =
            a_p[P_MAX - 1] * f * transmittance / (1.f - transmittance * f);

        return a_p;
    }

    dr::Array<Float, P_MAX + 1> attenuation_pdf(Float cos_theta_i,
                                                const SurfaceInteraction3f &si,
                                                Mask active = true) const {
        using Array_pmax_f = dr::Array<Float, P_MAX + 1>;

        // Parameterization of incident direction
        Float gamma_i = gamma(si.wi);
        Float h = dr::sin(gamma_i);
        Float sin_theta_i = dr::safe_sqrt(1.f - cos_theta_i * cos_theta_i);

        // Transmission angle in longitudinal plane
        Float sin_theta_t = sin_theta_i / m_eta;
        Float cos_theta_t = dr::safe_sqrt(1.f - dr::square(sin_theta_t));

        // Transmission angle in azimuthal plane
        Float eta_p = azimuthal_ior(sin_theta_i, cos_theta_i);
        Float sin_gamma_t = h / eta_p;
        Float cos_gamma_t = dr::safe_sqrt(1.f - dr::square(sin_gamma_t));

        // Attenuation coefficients
        UnpolarizedSpectrum sigma_a = absorption(si, active);
        Float transmitted_length = 2 * cos_gamma_t / cos_theta_t;
        UnpolarizedSpectrum transmittance = dr::exp(-sigma_a * transmitted_length);
        AttenuationCoeffs a_p = attenuation(cos_theta_i, m_eta, h, transmittance);

        Array_pmax_f a_p_pdf = dr::zeros<Array_pmax_f>();
        Array_pmax_f a_p_luminance = dr::zeros<Array_pmax_f>();
        Float sum_luminance(0.0f);
        for (int i = 0; i <= P_MAX; i++) {
            if constexpr (is_monochromatic_v<Spectrum>)
                a_p_luminance[i] = a_p[i][0];
            else if constexpr (is_spectral_v<Spectrum>)
                a_p_luminance[i] = luminance(a_p[i], si.wavelengths);
            else
                a_p_luminance[i] = luminance(a_p[i]);
            sum_luminance += a_p_luminance[i];
        }

        for (int i = 0; i <= P_MAX; ++i)
            a_p_pdf[i] = a_p_luminance[i] / sum_luminance;

        return a_p_pdf;
    }

    /// Longitudinal scattering distribution
    Float longitudinal_scattering(const Vector3f &wi, const Vector3f &wo,
                                  const Vector3f &tangent,
                                  const Float v) const {
        return warp::square_to_rough_fiber_pdf<Float>(wo, wi, tangent, 1.f / v);
    }

    MI_INLINE Float logistic(Float x, Float s) const {
        x = dr::abs(x);
        return dr::exp(-x / s) / (s * dr::square(1.f + dr::exp(-x / s)));
    }

    MI_INLINE Float logistic_cdf(Float x, Float s) const {
        return 1.f / (1.f + dr::exp(-x / s));
    }

    Float trimmed_logistic_sample(Float sample, Float s) const {
        Float k =
            logistic_cdf(dr::Pi<Float>, s) - logistic_cdf(-dr::Pi<Float>, s);
        Float x =
            -s *
            dr::log(1.f / (sample * k + logistic_cdf(-dr::Pi<Float>, s)) - 1.f);
        return dr::clip(x, -dr::Pi<Float>, dr::Pi<Float>);
    }

    /// Azimuthal scattering distribution (`s` is the logisitic scale factor)
    Float azimuthal_scattering(Float delta_phi, size_t p, Float s,
                               Float gamma_i, Float gamma_t) const {
        // Perfect specular reflection and transmission
        Float perfect_delta_phi =
            2 * p * gamma_t - 2 * gamma_i + p * dr::Pi<ScalarFloat>;
        Float phi =
            delta_phi - perfect_delta_phi; // offset w.r.t. perfect interactions

        // Map `phi` to [-pi, pi]
        phi = dr::fmod(phi, 2 * dr::Pi<Float>);
        dr::masked(phi, phi < dr::Pi<Float>) = phi + 2 * dr::Pi<Float>;
        dr::masked(phi, phi > dr::Pi<Float>) = phi - 2 * dr::Pi<Float>;

        // Model roughness with trimmed logistic distribution
        return (
            logistic(phi, s) /
            (logistic_cdf(dr::Pi<Float>, s) - logistic_cdf(-dr::Pi<Float>, s))
        );
    }

    /// Get the exctionction/absorption
    UnpolarizedSpectrum absorption(const SurfaceInteraction3f &si,
                                   Mask active) const {
        if (!m_use_pigmentation) {
            return m_scale * m_sigma_a->eval(si, active);
        } else {
            if constexpr (is_rgb_v<Spectrum>) {
                ScalarColor3f eumelanin_sigma_a(EUMELANIN_SIGMA_A);
                ScalarColor3f pheomelanin_sigma_a(PHEOMELANIN_SIGMA_A);

                return m_eumelanin * Color3f(eumelanin_sigma_a) +
                       m_pheomelanin * Color3f(pheomelanin_sigma_a);
            } else {
                // TODO: Use a measured spectral response of pigmentation(s)
                // rather than the RGB upsampling scheme.
                auto pigmentation_srf = [&](Float concentration,
                                            const ScalarVector3f &coeffs) {
                    return concentration * srgb_model_eval<UnpolarizedSpectrum>(
                                               coeffs, si.wavelengths);
                };

                return pigmentation_srf(m_eumelanin, EUMELANIN_SRGB_COEFFS) +
                       pigmentation_srf(m_pheomelanin, PHEOMELANIN_SRGB_COEFFS);
            }
        }
    }

    MI_DECLARE_CLASS()

private:
    /// Roughness
    Float m_longitudinal_roughness, m_azimuthal_roughness;

    Float m_alpha; /// Angle of scales
    Float m_eta; /// IOR

    /// Pigmentation
    bool m_use_pigmentation = true;
    Float m_eumelanin, m_pheomelanin;

    ref<Texture> m_sigma_a; /// Absorption if pigmentation is not used;
    ScalarFloat m_scale;

    Float m_v[P_MAX + 1]; /// Longitudinal variance due to roughness
    Float m_s; /// Azimuthal roughness scaling factor
    Float m_sin_2k_alpha[3], m_cos_2k_alpha[3];
};

MI_IMPLEMENT_CLASS_VARIANT(Hair, BSDF)
MI_EXPORT_PLUGIN(Hair, "Hair material")
NAMESPACE_END(mitsuba)
