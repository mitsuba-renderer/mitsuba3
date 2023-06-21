#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/fresnel.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/render/ior.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class Hair final : public BSDF<Float, Spectrum> {
public:
    MI_IMPORT_BASE(BSDF, m_flags, m_components)
    MI_IMPORT_TYPES(Texture)

    Hair(const Properties &props) : Base(props) {
        // Roughness (longitudinal & azimuthal) and scale tilt
        m_longitudinal_roughness  = props.get<ScalarFloat>("beta_m", 0.3f);
        m_azimuthal_roughness  = props.get<ScalarFloat>("beta_n", 0.3f);
        m_alpha   = props.get<ScalarFloat>("alpha", 2.f);

        // Indices of refraction at the interface
        ScalarFloat ext_ior = lookup_ior(props, "ext_ior", "air");
        ScalarFloat int_ior = lookup_ior(props, "int_ior", "amber");
        m_eta = int_ior / ext_ior;

        m_eumelanin = props.get<ScalarFloat>("eumelanin", 0.f);
        m_pheomelanin = props.get<ScalarFloat>("pheomelanin", 0.f);

        if (props.has_property("sigma_a")){
            m_sigma_a = props.get<ScalarVector3f>("sigma_a");
            isColor = true;
        }

        if (int_ior < 0.f || ext_ior < 0.f || int_ior == ext_ior)
            Throw("The interior and exterior indices of refraction must be "
                  "positive and differ!");
        if (m_longitudinal_roughness < 0 || m_longitudinal_roughness > 1){
            Throw("The longitudinal roughness \"beta_m\" should be in the "
                  "range [0, 1]!");
        }
        if (m_azimuthal_roughness < 0 || m_azimuthal_roughness > 1){
            Throw("The azimuthal roughness \"beta_n\" should be in the "
                  "range [0, 1]!");
        }

        // TODO: verify this
        m_components.push_back(BSDFFlags::Glossy | BSDFFlags::FrontSide |
                               BSDFFlags::BackSide | BSDFFlags::NonSymmetric);
        m_flags = m_components[0];
        dr::set_attr(this, "flags", m_flags);

        // longitudinal variance
        m_v[0] = dr::sqr(0.726f * m_longitudinal_roughness +
                         0.812f * dr::sqr(m_longitudinal_roughness) +
                         3.7f * dr::pow(m_longitudinal_roughness, 20));
        m_v[1] = .25f * m_v[0];
        m_v[2] = 4 * m_v[0];
        for (int p = 3; p <= p_max; ++p)
            m_v[p] = m_v[2];

        // Compute azimuthal logistic scale factor from $\m_azimuthal_roughness$
        ScalarFloat sqrt_pi_over_8 = dr::sqrt(dr::Pi<ScalarFloat> / 8.f);
        m_s = sqrt_pi_over_8 * (0.265f * m_azimuthal_roughness + 1.194f * dr::sqr(m_azimuthal_roughness) +
                                5.372f * dr::pow(m_azimuthal_roughness, 22));

        // Compute $\m_alpha$ terms for hair scales
        m_sin_2k_alpha[0] = dr::sin(dr::deg_to_rad(m_alpha));
        m_cos_2k_alpha[0] = dr::safe_sqrt(1 - dr::sqr(m_sin_2k_alpha[0]));
        for (int i = 1; i < 3; ++i) {
            m_sin_2k_alpha[i] = 2 * m_cos_2k_alpha[i - 1] * m_sin_2k_alpha[i - 1];
            m_cos_2k_alpha[i] = dr::sqr(m_cos_2k_alpha[i - 1]) - dr::sqr(m_sin_2k_alpha[i - 1]);
        }
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_parameter("m_longitudinal_roughness", m_longitudinal_roughness, +ParamFlags::NonDifferentiable);
        callback->put_parameter("m_azimuthal_roughness", m_azimuthal_roughness, +ParamFlags::NonDifferentiable);
        callback->put_parameter("m_alpha", m_alpha, +ParamFlags::NonDifferentiable);
        callback->put_parameter("m_eta", m_eta, +ParamFlags::NonDifferentiable);
        callback->put_parameter("m_eumelanin", m_eumelanin, +ParamFlags::NonDifferentiable);
        callback->put_parameter("m_pheomelanin", m_pheomelanin, +ParamFlags::NonDifferentiable);
    }

    std::pair<BSDFSample3f, Spectrum>
    sample(const BSDFContext &ctx, const SurfaceInteraction3f &si,
           Float sample1, const Point2f &sample2, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFSample, active);

        BSDFSample3f bs = dr::zeros<BSDFSample3f>();

        // TODO (really needed?)
        Vector3f wi = dr::normalize(si.wi);

        // Parameterization of incident direction
        Float gamma_i = gamma(wi);
        Float h = dr::sin(gamma_i);
        auto [sin_theta_i, cos_theta_i] = sincos_theta(wi);
        Float phi_i = azimuthal_angle(wi);

        // Sample segment length `p`
        dr::Array<Float, p_max + 1> a_p_pdf = attenuation_pdf(cos_theta_i, si);

        Point2f u[2] = { { sample1, 0 }, sample2 };
        // u[0][1] is the rescaled random number after using u[0][0]
        u[0][1] = u[0][0] / a_p_pdf[0];

        UInt32 p(0);
        for (size_t i = 0; i < p_max; ++i) {
            Bool sample_p = a_p_pdf[i] < u[0][0];
            u[0][0] -= a_p_pdf[i];

            dr::masked(p, sample_p) = i + 1;
            dr::masked(u[0][1], sample_p) = u[0][0] / a_p_pdf[i + 1];
        }

        // Account for scales on hair surface
        Float sin_theta_ip(0.f);
        Float cos_theta_ip(0.f);
        for (size_t j = 0; j < p_max; j++) {
            auto [sin_theta_ij, cos_theta_ij] =
                reframe_with_scales(sin_theta_i, cos_theta_i, j);
            dr::masked(sin_theta_ip, dr::eq(p, j)) = sin_theta_ij;
            dr::masked(cos_theta_ip, dr::eq(p, j)) = cos_theta_ij;
        }

        // Sample $M_p$ to compute $\thetai$
        // TODO: Understand
        Float cos_theta =
            1 + m_v[p_max] * dr::log(u[1][0] + (1 - u[1][0]) * dr::exp(-2 / m_v[p_max]));
        for (size_t i = 0; i < p_max; i++)
            dr::masked(cos_theta, dr::eq(p, i)) = 1 + m_v[i] * dr::log(u[1][0] + (1 - u[1][0]) * dr::exp(-2 / m_v[i]));

        Float sin_theta = dr::safe_sqrt(1 - dr::sqr(cos_theta));
        Float cos_phi   = dr::cos(2 * dr::Pi<ScalarFloat> * u[1][1]);
        Float sin_theta_o = -cos_theta * sin_theta_ip + sin_theta * cos_phi * cos_theta_ip;
        Float cos_thata_o = dr::safe_sqrt(1 - dr::sqr(sin_theta_o));

        // Transmission angle in azimuthal plane
        Float eta_p = azimuthal_ior(sin_theta_i, cos_theta_i);
        Float sin_gamma_t = h / eta_p;
        Float gamma_t = dr::safe_asin(sin_gamma_t);

        // Sample azimuthal scattering
        Float perfect_delta_phi = 2 * p * gamma_t - 2 * gamma_i + p * dr::Pi<ScalarFloat>;
        Float delta_phi_first_terms = perfect_delta_phi + trimmed_logistic_sample(u[0][1], m_s);
        Float delta_phi_remainder = 2 * dr::Pi<ScalarFloat> * u[0][1];
        Float delta_phi = dr::select(p < p_max, delta_phi_first_terms, delta_phi_remainder);

        // Outgoing direction
        Float phi_o = phi_i + delta_phi;
        Vector3f wo(cos_thata_o * dr::cos(phi_o), sin_theta_o,
                    cos_thata_o * dr::sin(phi_o));

        // PDF for sampled outgoing direction
        for (size_t i = 0; i < p_max; ++i) {
            // Account for scales on hair surface
            auto [sin_theta_ip, cos_theta_ip] =
                reframe_with_scales(sin_theta_i, cos_theta_i, i);
            Vector3f wi_p(cos_theta_ip * dr::cos(phi_i), sin_theta_ip,
                          cos_theta_ip * dr::sin(phi_i));

            bs.pdf += longitudinal_scattering(wi_p, wo, { 0, 1, 0 }, m_v[i]) *
                      dr::TwoPi<Float> * a_p_pdf[i] *
                      azimuthal_scattering(delta_phi, i, m_s, gamma_i, gamma_t);
        }
        bs.pdf += longitudinal_scattering(wi, wo, { 0, 1, 0 }, m_v[p_max]) *
                  a_p_pdf[p_max];

        bs.wo = wo;
        bs.pdf = dr::select(dr::isnan(bs.pdf) || dr::isinf(bs.pdf), 0, bs.pdf);
        bs.eta = 1.f;
        bs.sampled_type = +BSDFFlags::Glossy;
        bs.sampled_component = 0;

        UnpolarizedSpectrum value =
            dr::select(dr::neq(bs.pdf, 0), eval(ctx, si, bs.wo, active) / bs.pdf, 0);

        return { bs, depolarizer<Spectrum>(value) & (active && bs.pdf > 0.f) };
    }

    Spectrum eval(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                  const Vector3f &wo, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        if (!ctx.is_enabled(BSDFFlags::GlossyTransmission) &&
            !ctx.is_enabled(BSDFFlags::GlossyReflection))
            return 0.f;

        // TODO (really needed?)
        Vector3f wi = dr::normalize(si.wi);

        // Parameterization of incident and outgoing directions
        Float gamma_i = gamma(wi);
        Float h = dr::sin(gamma_i);
        auto [sin_theta_i, cos_theta_i] = sincos_theta(wi);
        Float phi_i = azimuthal_angle(wi);
        auto [sin_theta_o, cos_theta_o] = sincos_theta(wo);
        Float phi_o = azimuthal_angle(wo);

        // Transmission angle in longitudinal plane
        Float sin_theta_t = sin_theta_i / m_eta;
        Float cos_theta_t = dr::safe_sqrt(1 - dr::sqr(sin_theta_t));

        // Transmission angle in azimuthal plane
        Float eta_p = dr::safe_sqrt(dr::sqr(m_eta) - dr::sqr(sin_theta_i)) / cos_theta_i;
        Float sin_gamma_t = h / eta_p;
        Float cos_gamma_t = dr::safe_sqrt(1 - dr::sqr(sin_gamma_t));
        Float gamma_t = dr::safe_asin(sin_gamma_t);

        // Attenuation coefficients
        Spectrum wavelengths = get_spectrum(si);
        Spectrum sigma_a = m_sigma_a;
        if (!isColor)
            sigma_a = dr::fmadd(m_pheomelanin, pheomelanin(wavelengths),
                                m_eumelanin * eumelanin(wavelengths));
        Float transmitted_length = 2 * cos_gamma_t / cos_theta_t;
        Spectrum transmittance = dr::exp(-sigma_a * transmitted_length);
        AttenuationCoeffs a_p = attenuation(cos_theta_i, m_eta, h, transmittance);

        // Contribution of first `p_max` terms
        Float delta_phi = phi_o - phi_i;
        UnpolarizedSpectrum value(0.0f);
        for (int p = 0; p < p_max; ++p) {
            // Account for scales on hair surface
            auto [sin_theta_ip, cos_theta_ip] =
                reframe_with_scales(sin_theta_i, cos_theta_i, p);
            Vector3f wi_p(cos_theta_ip * dr::cos(phi_i), sin_theta_ip,
                          cos_theta_ip * dr::sin(phi_i));

            value += longitudinal_scattering(wi_p, wo, { 0, 1, 0 }, m_v[p]) *
                     dr::TwoPi<Float> * a_p[p] *
                     azimuthal_scattering(delta_phi, p, m_s, gamma_i, gamma_t);
        }

        // Contribution of remaining terms
        value += longitudinal_scattering(wi, wo, { 0, 1, 0 }, m_v[p_max]) *
                a_p[p_max];

        // If it is nan, return 0
        value = dr::select(dr::isnan(value) || dr::isinf(value), 0, value);

        return depolarizer<Spectrum>(value) & active;
    }

    Float pdf(const BSDFContext &ctx, const SurfaceInteraction3f &si,
              const Vector3f &wo, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        if (!ctx.is_enabled(BSDFFlags::GlossyTransmission) &&
            !ctx.is_enabled(BSDFFlags::GlossyReflection))
            return 0.f;

        // TODO (really needed?)
        Vector3f wi = dr::normalize(si.wi);

        // Parameterization of incident and outgoing directions
        Float gamma_i = gamma(wi);
        Float h = dr::sin(gamma_i);
        auto [sin_theta_i, cos_theta_i] = sincos_theta(wi);
        Float phi_i = azimuthal_angle(wi);
        auto [sin_theta_o, cos_theta_o] = sincos_theta(wo);
        Float phi_o = azimuthal_angle(wo);

        // Transmission angle in azimuthal plane
        Float eta_p = dr::safe_sqrt(Float(m_eta * m_eta) - dr::sqr(sin_theta_i)) / cos_theta_i;
        Float sin_gamma_t = h / eta_p;
        Float gamma_t = dr::safe_asin(sin_gamma_t);

        // Attenuation PDF
        dr::Array<Float, p_max + 1> apPdf = attenuation_pdf(cos_theta_i, si);

        // Compute PDF sum for each segment length
        Float delta_phi  = phi_o - phi_i;
        Float pdf(0.0f);
        for (int p = 0; p < p_max; ++p) {
            // Account for scales on hair surface
            auto [sin_theta_ip, cos_theta_ip] =
                reframe_with_scales(sin_theta_i, cos_theta_i, p);
            Vector3f wi_p(cos_theta_ip * dr::cos(phi_i), sin_theta_ip,
                          cos_theta_ip * dr::sin(phi_i));

            pdf += longitudinal_scattering(wi_p, wo, { 0, 1, 0 }, m_v[p]) *
                    dr::TwoPi<Float> * apPdf[p] *
                    azimuthal_scattering(delta_phi, p, m_s, gamma_i, gamma_t);
        }
        pdf += longitudinal_scattering(wi, wo, { 0, 1, 0 }, m_v[p_max]) *
                apPdf[p_max];

        pdf = dr::select(dr::isnan(pdf) || dr::isinf(pdf), 0, pdf);
        return pdf;
    }

    std::pair<Spectrum, Float> eval_pdf(const BSDFContext &ctx,
                                        const SurfaceInteraction3f &si,
                                        const Vector3f &wo,
                                        Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        if (!ctx.is_enabled(BSDFFlags::GlossyTransmission) &&
            !ctx.is_enabled(BSDFFlags::GlossyReflection)) {
            return { 0.f, 0.f };
        }

        Vector3f wi = dr::normalize(si.wi);
        Float gamma_i = gamma(wi);
        Float h = dr::sin(gamma_i);

        Float phi_o      = azimuthal_angle(wo);

        // Compute hair coordinate system terms related to _wi_
        Float sin_theta_i = wi.y();
        Float cos_theta_i = dr::safe_sqrt(1 - dr::sqr(sin_theta_i));
        Float phi_i      = azimuthal_angle(wi);

        // Compute $\gammat$ for refracted ray
        Float eta_p = dr::safe_sqrt(Float(m_eta * m_eta) - dr::sqr(sin_theta_i)) / cos_theta_i;
        Float sin_gamma_t = h / eta_p;
        Float cos_gamma_t = dr::safe_sqrt(1 - dr::sqr(sin_gamma_t));
        Float gamma_t = dr::safe_asin(sin_gamma_t);

        // Compute $\cos \thetat$ for refracted ray
        Float sin_theta_t = sin_theta_i / m_eta;
        Float cos_theta_t = dr::safe_sqrt(1 - dr::sqr(sin_theta_t));

        // Compute the transmittance _T_ of a single path through the cylinder
        Spectrum wavelengths = get_spectrum(si);
        Spectrum sigma_a = m_sigma_a;
        if (!isColor){
            sigma_a = dr::fmadd(m_pheomelanin, pheomelanin(wavelengths),
                    m_eumelanin * eumelanin(wavelengths));
        }
        Float transmitted_length = 2 * cos_gamma_t / cos_theta_t;
        Spectrum transmittance = dr::exp(-sigma_a * transmitted_length);

        dr::Array<Spectrum, p_max + 1> a_p = attenuation(cos_theta_i, Float(m_eta), h, transmittance);
        dr::Array<Float, p_max + 1> a_p_pdf = attenuation_pdf(cos_theta_i, si);

        // Compute PDF sum for hair scattering events
        Float delta_phi = phi_o - phi_i;
        Float pdf = Float(0.0f);
        Spectrum value(0.0f);

        for (int p = 0; p < p_max; ++p) {
            auto [sin_theta_ip, cos_theta_ip] =
                reframe_with_scales(sin_theta_i, cos_theta_i, p);
            Vector3f wi_p(cos_theta_ip * dr::cos(phi_i), sin_theta_ip,
                          cos_theta_ip * dr::sin(phi_i));

            Float longitudinal = longitudinal_scattering(wi_p, wo, { 0, 1, 0 }, m_v[p]);
            Float azimuthal = azimuthal_scattering(delta_phi, p, m_s, gamma_i, gamma_t);

            pdf   += longitudinal * dr::TwoPi<Float> * a_p_pdf[p] * azimuthal;
            value += longitudinal * dr::TwoPi<Float> * a_p[p]     * azimuthal;
        }

        Float longitudinal = longitudinal_scattering(wi, wo, { 0, 1, 0 }, m_v[p_max]);
        pdf   += longitudinal * a_p_pdf[p_max];
        value += longitudinal * a_p[p_max];

        pdf   = dr::select(dr::isnan(pdf)   || dr::isinf(pdf),   0, pdf);
        value = dr::select(dr::isnan(value) || dr::isinf(value), 0, value);

        return { depolarizer<Spectrum>(value) & active, dr::select(active, pdf, 0.f) };
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
    constexpr static const int p_max = 3;
    static_assert(p_max >= 3, "There should be at least 3 segments!");

    using AttenuationCoeffs = dr::Array<Spectrum, p_max + 1>;

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

    /// Angle between surface normal and direction `w`
    MI_INLINE Float gamma(const Vector3f &w) const {
        Float normal_plane_proj = dr::safe_sqrt(dr::sqr(w.x()) + dr::sqr(w.z()));
        Float gamma = dr::safe_acos(w.z() / normal_plane_proj);
        return dr::select(w.x() < 0, gamma, -gamma);
    }

    /// Modified index of refraction, considers projection in the normal plane
    MI_INLINE Float azimuthal_ior(Float sin_theta_i, Float cos_theta_i) const {
        return dr::safe_sqrt(dr::sqr(m_eta) - dr::sqr(sin_theta_i)) / cos_theta_i;
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
    AttenuationCoeffs attenuation(Float cos_theta_i, Float eta, Float h,
                                const Spectrum &transmittance) const {
        AttenuationCoeffs a_p = dr::zeros<AttenuationCoeffs>();

        Float cos_gamma_i = dr::safe_sqrt(1 - dr::sqr(h));
        Float cos_theta = cos_theta_i * cos_gamma_i; // hair coordinate system

        Float f = std::get<0>(fresnel(cos_theta, eta));
        a_p[0] = f;
        a_p[1] = dr::sqr(1 - f) * transmittance;
        for (int p = 2; p < p_max; ++p)
            a_p[p] = a_p[p - 1] * transmittance * f;

        // Sum of remaining possible lenghts (as `p` goes to infinity)
        a_p[p_max] = a_p[p_max - 1] * f * transmittance / (1.f - transmittance * f);

        return a_p;
    }

    dr::Array<Float, p_max + 1>
    attenuation_pdf(Float cos_theta_i, const SurfaceInteraction3f &si) const {
        using Array_pmax_f = dr::Array<Float, p_max + 1>;

        Vector3f wi = dr::normalize(si.wi);
        Float gamma_i = gamma(wi);
        Float h = dr::sin(gamma_i);

        // Compute array of A_p values for cosThetaI
        Float sin_theta_i = dr::safe_sqrt(1 - cos_theta_i * cos_theta_i);

        // Compute $\cos \thetat$ for refracted ray
        Float sin_theta_t = sin_theta_i / m_eta;
        Float cos_theta_t = dr::safe_sqrt(1 - dr::sqr(sin_theta_t));

        // Compute $\gammat$ for refracted ray
        Float eta_p = azimuthal_ior(sin_theta_i, cos_theta_i);
        Float sin_gamma_t = h / eta_p;
        Float cos_gamma_t = dr::safe_sqrt(1 - dr::sqr(sin_gamma_t));

        // Compute the transmittance _T_ of a single path through the cylinder
        Spectrum wavelengths = get_spectrum(si);
        Spectrum sigma_a = m_sigma_a;
        if (!isColor)
            sigma_a = dr::fmadd(m_pheomelanin, pheomelanin(wavelengths),
                                m_eumelanin * eumelanin(wavelengths));
        Float transmitted_length = 2 * cos_gamma_t / cos_theta_t;
        Spectrum transmittance = dr::exp(-sigma_a * transmitted_length);
        AttenuationCoeffs a_p = attenuation(cos_theta_i, m_eta, h, transmittance);

        Array_pmax_f a_p_pdf = dr::zeros<Array_pmax_f>();
        Array_pmax_f a_p_luminance = dr::zeros<Array_pmax_f>();
        Float sum_luminance(0.0f);
        for (int i = 0; i <= p_max; i++) {
            if constexpr (!is_spectral_v<Spectrum>)
                a_p_luminance[i] = luminance(a_p[i]);
            else
                a_p_luminance[i] = luminance(a_p[i], si.wavelengths);
            sum_luminance += a_p_luminance[i];
        }

        for (int i = 0; i <= p_max; ++i)
            a_p_pdf[i] = a_p_luminance[i] / sum_luminance;

        return a_p_pdf;
    }

    /// Longitudinal scattering distribution
    Float longitudinal_scattering(const Vector3f &wi, const Vector3f &wo,
                                  const Vector3f &tangent,
                                  const ScalarFloat v) const {
        return warp::square_to_rough_fiber_pdf<Float>(wo, wi, tangent, 1.f / v);
    }

    MI_INLINE Float logistic(Float x, Float s) const {
        x = dr::abs(x);
        return dr::exp(-x / s) / (s * dr::sqr(1 + dr::exp(-x / s)));
    }

    MI_INLINE Float logistic_cdf(Float x, Float s) const {
        return 1 / (1 + dr::exp(-x / s));
    }

    Float trimmed_logistic_sample(Float sample, Float s) const {
        Float k = logistic_cdf(dr::Pi<Float>, s) - logistic_cdf(-dr::Pi<Float>, s);
        Float x = -s * dr::log(1 / (sample * k + logistic_cdf(-dr::Pi<Float>, s)) - 1);
        return dr::clamp(x, -dr::Pi<Float>, dr::Pi<Float>);
    }

    /// Azimuthal scattering distribution (`s` is the logisitic scale factor)
    Float azimuthal_scattering(Float delta_phi, size_t p, Float s, Float gamma_i,
                               Float gamma_t) const {
        // Perfect specular reflection and transmission
        Float perfect_delta_phi = 2 * p * gamma_t - 2 * gamma_i + p * dr::Pi<ScalarFloat>;
        Float phi = delta_phi - perfect_delta_phi; // offset w.r.t. perfect interactions

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

    // Get wavelengths of the ray
    inline Spectrum get_spectrum(const SurfaceInteraction3f &si) const {
        Spectrum wavelengths;
        if constexpr (is_spectral_v<Spectrum>) {
            wavelengths[0] = si.wavelengths[0]; wavelengths[1] = si.wavelengths[1];
            wavelengths[2] = si.wavelengths[2]; wavelengths[3] = si.wavelengths[3];
        } else {
            wavelengths[0] = 612.f; wavelengths[1] = 549.f; wavelengths[2] = 465.f;
        }

        return wavelengths;
    }

    // Pheomelanin absorption coefficient
    inline Spectrum pheomelanin(const Spectrum &lambda) const {
        return 2.9e12f * dr::pow(lambda, -4.75f); // adjusted relative to 0.1mm hair width
    }

    // Eumelanin absorption coefficient
    inline Spectrum eumelanin(const Spectrum &lambda) const {
        return 6.6e8f * dr::pow(lambda, -3.33f); // adjusted relative to 0.1mm hair width
    }

    MI_DECLARE_CLASS()

private:
    /// Roughness
    ScalarFloat m_longitudinal_roughness, m_azimuthal_roughness;

    ScalarFloat m_alpha; /// Angle of scales
    ScalarFloat m_eta; /// IOR

    /// Pigmentation
    ScalarFloat m_eumelanin, m_pheomelanin;

    ScalarFloat m_v[p_max + 1]; /// Longitudinal variance due to roughness
    Float m_s; /// Azimuthal roughness scaling factor
    Float m_sin_2k_alpha[3], m_cos_2k_alpha[3];

    ScalarVector3f m_sigma_a;
    bool isColor = false;
};

MI_IMPLEMENT_CLASS_VARIANT(Hair, BSDF)
MI_EXPORT_PLUGIN(Hair, "Hair material")
NAMESPACE_END(mitsuba)
