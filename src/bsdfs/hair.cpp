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

        Vector3f wi = dr::normalize(si.wi);
        Vector3f wo;
        Float gamma_i = gamma(wi);
        Float h = dr::sin(gamma_i);

        // Compute hair coordinate system terms related to _wo_
        Float sinThetaI = wi.y();
        Float cosThetaI = dr::safe_sqrt(1 - dr::sqr(sinThetaI));
        Float phi_i      = azimuthal_angle(wi);
        BSDFSample3f bs = dr::zeros<BSDFSample3f>();

        Point2f u[2] = { {sample1, 0}, sample2 };

        // Determine which term $p$ to sample for hair scattering
        dr::Array<Float, p_max + 1> apPdf = attenuation_eval_pdf(cosThetaI, si);

        Int32 p       = Int32(-1);
        // u[0][1] is the rescaled random number after using u[0][0]
        u[0][1] = u[0][0] / apPdf[0];
        ScalarInt32 i = 0;

        while (i < p_max) {
            dr::masked(p, u[0][0] >= apPdf[i]) = i;
            dr::masked(u[0][1], u[0][0] >= apPdf[i]) = (u[0][0] - apPdf[i])/ apPdf[i+1];
            u[0][0] -= apPdf[i];
            i++;
        }
        p++;

        /******* for different lobe  ********/
        // p = 0;
        // apPdf[0] = 1;
        // apPdf[1] = 0;
        // apPdf[2] = 0;
        // apPdf[3] = 0;
        /******* for different lobe  ********/


        // Rotate $\sin \thetao$ and $\cos \thetao$ to account for hair scale
        // tilt
        Float sinThetaIp = sinThetaI;
        Float cosThetaIp = cosThetaI;

        dr::masked(sinThetaIp, dr::eq(p, 0)) = sinThetaI * m_cos_2k_alpha[1] - cosThetaI * m_sin_2k_alpha[1];
        dr::masked(cosThetaIp, dr::eq(p, 0)) = cosThetaI * m_cos_2k_alpha[1] + sinThetaI * m_sin_2k_alpha[1];
        dr::masked(sinThetaIp, dr::eq(p, 1)) = sinThetaI * m_cos_2k_alpha[0] + cosThetaI * m_sin_2k_alpha[0];
        dr::masked(cosThetaIp, dr::eq(p, 1)) = cosThetaI * m_cos_2k_alpha[0] - sinThetaI * m_sin_2k_alpha[0];
        dr::masked(sinThetaIp, dr::eq(p, 2)) = sinThetaI * m_cos_2k_alpha[2] + cosThetaI * m_sin_2k_alpha[2];
        dr::masked(cosThetaIp, dr::eq(p, 2)) = cosThetaI * m_cos_2k_alpha[2] - sinThetaI * m_sin_2k_alpha[2];

        // Sample $M_p$ to compute $\thetai$

        Float cosTheta =
            1 + m_v[p_max] * dr::log(u[1][0] + (1 - u[1][0]) * dr::exp(-2 / m_v[p_max]));
        for (int i = 0; i < p_max; i++) {
            dr::masked(cosTheta, dr::eq(p, i)) = 1 + m_v[i] * dr::log(u[1][0] + (1 - u[1][0]) * dr::exp(-2 / m_v[i]));
        }

        Float sinTheta = dr::safe_sqrt(1 - dr::sqr(cosTheta));
        Float cosPhi   = dr::cos(2 * dr::Pi<ScalarFloat> * u[1][1]);
        Float sinThetaO =
            -cosTheta * sinThetaIp + sinTheta * cosPhi * cosThetaIp;
        Float cosThetaO = dr::safe_sqrt(1 - dr::sqr(sinThetaO));

        // Sample $N_p$ to compute $\Delta\phi$

        // Compute $\gammat$ for refracted ray
        Float etap = dr::safe_sqrt(Float(m_eta * m_eta) - dr::sqr(sinThetaI)) / cosThetaI;
        Float sinGammaT = h / etap;
        Float gammaT    = dr::safe_asin(sinGammaT);
        Float dphi;
        Float Phi = 2 * p * gammaT - 2 * gamma_i + p * dr::Pi<ScalarFloat>;

        /******* for different dimension  ********/

        // dphi = 2 * dr::Pi<ScalarFloat> * u[0][1];

        dphi = dr::select(p < p_max, Phi + SampleTrimmedLogistic(u[0][1], m_s, -dr::Pi<ScalarFloat>, dr::Pi<ScalarFloat>), 2 * dr::Pi<ScalarFloat> * u[0][1]);

        /******* for different dimension  ********/

        // Compute _wi_ from sampled hair scattering angles
        Float phi_o = phi_i + dphi;
        wo = Vector3f(cosThetaO * dr::cos(phi_o), sinThetaO,
                      cosThetaO * dr::sin(phi_o));

        // Compute PDF for sampled hair scattering direction _wi_
        for (int i = 0; i < p_max; ++i) {
            // Compute $\sin \thetao$ and $\cos \thetao$ terms accounting for
            // scales
            Float sinThetaIp, cosThetaIp;
            if (i == 0) {
                sinThetaIp =
                    sinThetaI * m_cos_2k_alpha[1] - cosThetaI * m_sin_2k_alpha[1];
                cosThetaIp =
                    cosThetaI * m_cos_2k_alpha[1] + sinThetaI * m_sin_2k_alpha[1];
            }
            // Handle remainder of $p$ values for hair scale tilt
            else if (i == 1) {
                sinThetaIp =
                    sinThetaI * m_cos_2k_alpha[0] + cosThetaI * m_sin_2k_alpha[0];
                cosThetaIp =
                    cosThetaI * m_cos_2k_alpha[0] - sinThetaI * m_sin_2k_alpha[0];
            } else if (i == 2) {
                sinThetaIp =
                    sinThetaI * m_cos_2k_alpha[2] + cosThetaI * m_sin_2k_alpha[2];
                cosThetaIp =
                    cosThetaI * m_cos_2k_alpha[2] - sinThetaI * m_sin_2k_alpha[2];
            } else {
                sinThetaIp = sinThetaI;
                cosThetaIp = cosThetaI;
            }

            Vector3f wip = Vector3f(0, sinThetaIp, 0);
            bs.pdf += longitudinal_scattering(wip, wo, { 0, 1, 0 }, m_v[i]) *
                      dr::TwoPi<Float> * apPdf[i] *
                      azimuthal_scattering(dphi, i, m_s, gamma_i, gammaT);
        }

        bs.pdf += longitudinal_scattering(wi, wo, { 0, 1, 0 }, m_v[p_max]) *
                  apPdf[p_max];

        bs.wo  = wo;
        bs.pdf = dr::select(dr::isnan(bs.pdf) || dr::isinf(bs.pdf), 0, bs.pdf);
        bs.eta = 1.f;
        bs.sampled_type      = +BSDFFlags::Glossy;
        bs.sampled_component = 0;

        // wo = warp::square_to_uniform_sphere(sample2);
        // bs.wo                = wo;
        // bs.pdf               = warp::square_to_uniform_sphere_pdf(bs.wo);
        // bs.eta               = 1.;
        // bs.sampled_type      = +BSDFFlags::Glossy;
        // bs.sampled_component = 0;

        UnpolarizedSpectrum value =
            dr::select(dr::neq(bs.pdf, 0), eval(ctx, si, bs.wo, active) / bs.pdf, 0);

        return { bs, value & (active && bs.pdf > 0.f) };
    }

    Spectrum eval(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                  const Vector3f &wo, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        // TODO
        if (!ctx.is_enabled(BSDFFlags::GlossyTransmission) &&
            !ctx.is_enabled(BSDFFlags::GlossyReflection)) {
            return 0.f;
        }

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
            Vector3f wi_p = reframe_with_scales(sin_theta_i, cos_theta_i, phi_i, p);
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
            !ctx.is_enabled(BSDFFlags::GlossyReflection)) {
            return 0.f;
        }

        Vector3f wi = dr::normalize(si.wi);
        Float gamma_i = gamma(wi);
        Float h = dr::sin(gamma_i);

        Float sinThetaO = wo.y();
        Float cosThetaO = dr::safe_sqrt(1 - dr::sqr(sinThetaO));
        Float phi_o      = azimuthal_angle(wo);

        // Compute hair coordinate system terms related to _wi_
        Float sinThetaI = wi.y();
        Float cosThetaI = dr::safe_sqrt(1 - dr::sqr(sinThetaI));
        Float phi_i      = azimuthal_angle(wi);
        // Compute $\gammat$ for refracted ray
        Float etap = dr::safe_sqrt(Float(m_eta * m_eta) - dr::sqr(sinThetaI)) / cosThetaI;
        Float sinGammaT = h / etap;
        Float gammaT    = dr::safe_asin(sinGammaT);

        // Compute PDF for $A_p$ terms
        dr::Array<Float, p_max + 1> apPdf = attenuation_eval_pdf(cosThetaI, si);

        /******* for different lobe  ********/
        // apPdf[0] = 1;
        // apPdf[1] = 0;
        // apPdf[2] = 0;
        // apPdf[3] = 0;
        /******* for different lobe  ********/
        // Compute PDF sum for hair scattering events
        Float phi  = phi_o - phi_i;
        Float _pdf = Float(0);

        for (int p = 0; p < p_max; ++p) {
            // Compute $\sin \thetao$ and $\cos \thetao$ terms accounting for
            // scales
            Float sinThetaIp, cosThetaIp;
            if (p == 0) {
                sinThetaIp = sinThetaI * m_cos_2k_alpha[1] - cosThetaI * m_sin_2k_alpha[1];
                cosThetaIp = cosThetaI * m_cos_2k_alpha[1] + sinThetaI * m_sin_2k_alpha[1];
            }
            else if (p == 1) {
                sinThetaIp = sinThetaI * m_cos_2k_alpha[0] + cosThetaI * m_sin_2k_alpha[0];
                cosThetaIp = cosThetaI * m_cos_2k_alpha[0] - sinThetaI * m_sin_2k_alpha[0];
            } else if (p == 2) {
                sinThetaIp = sinThetaI * m_cos_2k_alpha[2] + cosThetaI * m_sin_2k_alpha[2];
                cosThetaIp = cosThetaI * m_cos_2k_alpha[2] - sinThetaI * m_sin_2k_alpha[2];
            } else {
                sinThetaIp = sinThetaI;
                cosThetaIp = cosThetaI;
            }

            // Handle out-of-range $\cos \thetao$ from scale adjustment
            cosThetaIp = dr::abs(cosThetaIp);

            /******* for different dimension  ********/

            // importance sampling
            Vector3f wi_p = Vector3f(0, sinThetaIp, 0);
            _pdf += longitudinal_scattering(wi_p, wo, { 0, 1, 0 }, m_v[p]) *
                    dr::TwoPi<Float> * apPdf[p] *
                    azimuthal_scattering(phi, p, m_s, gamma_i, gammaT);

            /******* for different dimension  ********/
        }
        _pdf += longitudinal_scattering(wi, wo, { 0, 1, 0 }, m_v[p_max]) *
                apPdf[p_max];

        _pdf = dr::select(dr::isnan(_pdf) || dr::isinf(_pdf), 0, _pdf);
        return _pdf ;
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

        Float sinThetaO = wo.y();
        Float cosThetaO = dr::safe_sqrt(1 - dr::sqr(sinThetaO));
        Float phi_o      = azimuthal_angle(wo);

        // Compute hair coordinate system terms related to _wi_
        Float sinThetaI = wi.y();
        Float cosThetaI = dr::safe_sqrt(1 - dr::sqr(sinThetaI));
        Float phi_i      = azimuthal_angle(wi);

        // Compute $\gammat$ for refracted ray
        Float etap = dr::safe_sqrt(Float(m_eta * m_eta) - dr::sqr(sinThetaI)) / cosThetaI;
        Float sinGammaT = h / etap;
        Float cosGammaT = dr::safe_sqrt(1 - dr::sqr(sinGammaT));
        Float gammaT    = dr::safe_asin(sinGammaT);

        // Compute $\cos \thetat$ for refracted ray
        Float sinThetaT = sinThetaI / m_eta;
        Float cosThetaT = dr::safe_sqrt(1 - dr::sqr(sinThetaT));

        // Compute the transmittance _T_ of a single path through the cylinder
        Spectrum wavelengths = get_spectrum(si);
        Spectrum sigma_a = m_sigma_a;
        if (!isColor){
            sigma_a = dr::fmadd(m_pheomelanin, pheomelanin(wavelengths),
                    m_eumelanin * eumelanin(wavelengths));
        }
        Spectrum T =
            dr::exp(-sigma_a * (2 * cosGammaT / cosThetaT));

        // Compute PDF for $A_p$ terms
        dr::Array<Float, p_max + 1> apPdf = attenuation_eval_pdf(cosThetaI, si);

        dr::Array<Spectrum, p_max + 1> ap = attenuation(cosThetaI, Float(m_eta), h, T);

        // Compute PDF sum for hair scattering events
        Float phi = phi_o - phi_i;
        Float _pdf = Float(0);
        Spectrum fsum(0.);

        for (int p = 0; p < p_max; ++p) {
            // Compute $\sin \thetao$ and $\cos \thetao$ terms accounting for
            // scales
            Float sinThetaIp, cosThetaIp;
            if (p == 0) {
                sinThetaIp = sinThetaI * m_cos_2k_alpha[1] - cosThetaI * m_sin_2k_alpha[1];
                cosThetaIp = cosThetaI * m_cos_2k_alpha[1] + sinThetaI * m_sin_2k_alpha[1];
            }
            // Handle remainder of $p$ values for hair scale tilt
            else if (p == 1) {
                sinThetaIp = sinThetaI * m_cos_2k_alpha[0] + cosThetaI * m_sin_2k_alpha[0];
                cosThetaIp = cosThetaI * m_cos_2k_alpha[0] - sinThetaI * m_sin_2k_alpha[0];
            } else if (p == 2) {
                sinThetaIp = sinThetaI * m_cos_2k_alpha[2] + cosThetaI * m_sin_2k_alpha[2];
                cosThetaIp = cosThetaI * m_cos_2k_alpha[2] - sinThetaI * m_sin_2k_alpha[2];
            } else {
                sinThetaIp = sinThetaI;
                cosThetaIp = cosThetaI;
            }

            // Handle out-of-range $\cos \thetao$ from scale adjustment
            cosThetaIp = dr::abs(cosThetaIp);

            Vector3f wip = Vector3f(0, sinThetaIp, 0);
            Float M_p = longitudinal_scattering(wip, wo, { 0, 1, 0 }, m_v[p]) *
                        dr::TwoPi<Float>;
            Float N_p = azimuthal_scattering(phi, p, m_s, gamma_i, gammaT);

            _pdf += M_p * apPdf[p] * N_p;

            fsum += M_p * ap[p] * N_p;
        }

        _pdf += longitudinal_scattering(wi, wo, { 0, 1, 0 }, m_v[p_max]) *
                apPdf[p_max];

        fsum += longitudinal_scattering(wi, wo, { 0, 1, 0 }, m_v[p_max]) *
                ap[p_max];

        fsum = dr::select(dr::isnan(fsum) || dr::isinf(fsum), 0, fsum);
        _pdf = dr::select(dr::isnan(_pdf) || dr::isinf(_pdf), 0, _pdf);
        return { fsum & active, _pdf};
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

    Float SampleTrimmedLogistic(Float u, Float s, Float a, Float b) const {
        // a should be smaller than b
        Float k = logistic_cdf(b, s) - logistic_cdf(a, s);
        Float x = -s * dr::log(1 / (u * k + logistic_cdf(a, s)) - 1);
        return dr::clamp(x, a, b);
    }

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

    /// Return modified direction to account for angle of scales on hair surface
    Vector3f reframe_with_scales(Float sin_theta_i, Float cos_theta_i,
                                 Float phi_i, size_t p) const {
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

        return Vector3f(cos_theta_ip * dr::cos(phi_i), sin_theta_ip,
                        cos_theta_ip * dr::sin(phi_i));
    };

    dr::Array<Float, p_max + 1>
    attenuation_eval_pdf(Float cosThetaI,
                         const SurfaceInteraction3f &si) const {

        Vector3f wi = dr::normalize(si.wi);
        Float gamma_i = gamma(wi);
        Float h = dr::sin(gamma_i);

        // Compute array of A_p values for cosThetaI
        Float sinThetaI = dr::safe_sqrt(1 - cosThetaI * cosThetaI);

        // Compute $\cos \thetat$ for refracted ray
        Float sinThetaT = sinThetaI / m_eta;
        Float cosThetaT = dr::safe_sqrt(1 - dr::sqr(sinThetaT));

        // Compute $\gammat$ for refracted ray
        Float eta_p = dr::safe_sqrt(m_eta * m_eta - dr::sqr(sinThetaI)) / cosThetaI;
        Float sinGammaT = h / eta_p;
        Float cosGammaT = dr::safe_sqrt(1 - dr::sqr(sinGammaT));

        // Compute the transmittance _T_ of a single path through the cylinder
        Spectrum wavelengths = get_spectrum(si);
        Spectrum sigma_a = m_sigma_a;
        if (!isColor){
            sigma_a = dr::fmadd(m_pheomelanin, pheomelanin(wavelengths),
                    m_eumelanin * eumelanin(wavelengths));
        }
        Spectrum T =
            dr::exp(-sigma_a * (2 * cosGammaT / cosThetaT));

        // Calculate Ap
        Spectrum ap[p_max + 1];
        Float cosGammaI = dr::safe_sqrt(1 - h * h);
        Float cosTheta  = cosThetaI * cosGammaI;

        Float f = std::get<0>(fresnel(cosTheta, Float(m_eta)));
        ap[0] = f;
        // Compute $p=1$ attenuation termap.y()
        ap[1] = dr::sqr(1 - f) * T;
        // Compute attenuation terms up to $p=_pMax_$
        for (int p = 2; p < p_max; ++p)
            ap[p] = ap[p - 1] * T * f;
        // Compute attenuation term accounting for remaining orders of
        // scattering
        ap[p_max] = ap[p_max - 1] * f * T / (Spectrum(1.f) - T * f);

        // Compute $A_p$ PDF from individual $A_p$ terms
        dr::Array<Float, p_max + 1> apPdf;
        Float sumY = Float(0);

        for (int i = 0; i <= p_max; i++) {
            sumY = sumY + ap[i].y();
        }

        for (int i = 0; i <= p_max; ++i) {
            apPdf[i] = ap[i].y() / sumY;
        }
        return apPdf;
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
