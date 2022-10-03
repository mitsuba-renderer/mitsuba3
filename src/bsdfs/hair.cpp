#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class Hair final : public BSDF<Float, Spectrum> {
public:
    MI_IMPORT_BASE(BSDF, m_flags, m_components)
    MI_IMPORT_TYPES(Texture)

    Hair(const Properties &props) : Base(props) {
        sigma_a = props.texture<Texture>("sigma_a", .5f);
        beta_m = props.get<Float>("beta_m", 0.3f);
        beta_n = props.get<Float>("beta_n", 0.3f);
        alpha = props.get<Float>("alpha", 2.f);
        eta = props.get<Float>("eta", 1.55f);

        // I still put it as diffuse because in the formula there is no delta related
        // m_flags = BSDFFlags::GlossyTransmission | BSDFFlags::GlossyReflection | BSDFFlags::FrontSide | BSDFFlags::BackSide | BSDFFlags::NonSymmetric;

        m_components.push_back(BSDFFlags::GlossyReflection | BSDFFlags::FrontSide | BSDFFlags::BackSide);
        m_components.push_back(BSDFFlags::GlossyTransmission | BSDFFlags::FrontSide | BSDFFlags::BackSide |BSDFFlags::NonSymmetric);

        m_flags = m_components[0] | m_components[1];
        dr::set_attr(this, "flags", m_flags);
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_object("sigma_a", sigma_a.get(), +ParamFlags::NonDifferentiable);
        callback->put_parameter("beta_m", beta_m, +ParamFlags::NonDifferentiable);
        callback->put_parameter("beta_n", beta_n, +ParamFlags::NonDifferentiable);
        callback->put_parameter("alpha", alpha, +ParamFlags::NonDifferentiable);
        callback->put_parameter("eta", eta, +ParamFlags::NonDifferentiable);
    }
    // for python test // differentiable -> empty


    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx,
                                             const SurfaceInteraction3f &si,
                                             Float /* sample1 */,
                                             const Point2f &sample2,
                                             Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFSample, active);

        Float cos_theta_i = Frame3f::cos_theta(si.wi);
        BSDFSample3f bs = dr::zeros<BSDFSample3f>();

        active &= cos_theta_i > 0.f;
        if (unlikely(dr::none_or<false>(active) ||
                     !ctx.is_enabled(BSDFFlags::DiffuseReflection)))
            return { bs, 0.f };

        bs.wo = warp::square_to_cosine_hemisphere(sample2);
        bs.pdf = warp::square_to_cosine_hemisphere_pdf(bs.wo);
        bs.eta = 1.f;
        bs.sampled_type = +BSDFFlags::DiffuseReflection;
        bs.sampled_component = 0;

        UnpolarizedSpectrum value = m_reflectance->eval(si, active);

        return { bs, depolarizer<Spectrum>(value) & (active && bs.pdf > 0.f) };
    }

    Spectrum eval(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                  const Vector3f &wo, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        if (!ctx.is_enabled(BSDFFlags::GlossyReflection) && !ctx.is_enabled(BSDFFlags::GlossyReflection))
            return 0.f;

        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);
        Float h = -1 + 2 * si.uv[1];
        Float gammaO = dr::safe_asin(h);

        // CHECK(h >= -1 && h <= 1);
        // CHECK(beta_m >= 0 && beta_m <= 1);
        // CHECK(beta_n >= 0 && beta_n <= 1);
        // CHECK(pMax >= 3);

        Float v[pMax+1];
        Float s;
        Float sin2kAlpha[3], cos2kAlpha[3];

        active &= cos_theta_i > 0.f && cos_theta_o > 0.f;

        v[0] = dr::sqr(0.726f * beta_m + 0.812f * dr::sqr(beta_m) + 3.7f * dr::pow(beta_m, 20));
        v[1] = .25f * v[0];
        v[2] = 4 * v[0];
        for (int p = 3; p <= pMax; ++p)
            v[p] = v[2];

        // Compute azimuthal logistic scale factor from $\beta_n$
        s = SqrtPiOver8 *
            (0.265f * beta_n + 1.194f * dr::sqr(beta_n) + 5.372f * dr::pow(beta_n, 22));

        // CHECK(!std::isnan(s));

        // Compute $\alpha$ terms for hair scales
        sin2kAlpha[0] = dr::sin(dr::deg_to_rad(alpha));
        cos2kAlpha[0] = dr::safe_sqrt(1 - dr::sqr(sin2kAlpha[0]));
        for (int i = 1; i < 3; ++i) {
            sin2kAlpha[i] = 2 * cos2kAlpha[i - 1] * sin2kAlpha[i - 1];
            cos2kAlpha[i] = dr::sqr(cos2kAlpha[i - 1]) - dr::sqr(sin2kAlpha[i - 1]);
        }

        // Compute the BSDF
        // Compute hair coordinate system terms related to _wo_
        Float sinThetaO = wo.x();
        Float cosThetaO = dr::safe_sqrt(1 - dr::sqr(sinThetaO));
        Float phiO = dr::atan2(wo.z(), wo.y());

        // Compute hair coordinate system terms related to _wi_
        Float sinThetaI = si.wi.x();
        Float cosThetaI = dr::safe_sqrt(1 - dr::sqr(sinThetaI));
        Float phiI = dr::atan2(si.wi.z(), si.wi.y());

        // Compute $\cos \thetat$ for refracted ray
        Float sinThetaT = sinThetaO / eta;
        Float cosThetaT = dr::safe_sqrt(1 - dr::sqr(sinThetaT));

        // Compute $\gammat$ for refracted ray
        Float etap = dr::sqrt(eta * eta - dr::sqr(sinThetaO)) / cosThetaO;
        Float sinGammaT = h / etap;
        Float cosGammaT = dr::safe_sqrt(1 - dr::sqr(sinGammaT));
        Float gammaT = dr::safe_asin(sinGammaT);

        // Compute the transmittance _T_ of a single path through the cylinder
        Spectrum T = dr::exp(-sigma_a * (2 * cosGammaT / cosThetaT));

        // Calculate Ap
        Spectrum ap[pMax + 1];
        Float cosGammaO = dr::safe_sqrt(1 - h * h);
        Float cosTheta = cosThetaO * cosGammaO;
        // TODO: ?
        Float f = get<0>(fresnel(cosTheta, eta)); 
        ap[0] = f;
        // Compute $p=1$ attenuation term
        ap[1] = dr::sqr(1 - f) * T;
        // Compute attenuation terms up to $p=_pMax_$
        for (int p = 2; p < pMax; ++p) ap[p] = ap[p - 1] * T * f;
        // Compute attenuation term accounting for remaining orders of scattering
        ap[pMax] = ap[pMax - 1] * f * T / (Spectrum(1.f) - T * f);

        // Calculate Mp



        // Evaluate hair BSDF
        Float phi = phiI - phiO;
        Spectrum fsum(0.);
        for (int p = 0; p < pMax; ++p) {
            // Compute $\sin \thetao$ and $\cos \thetao$ terms accounting for scales
            Float sinThetaOp, cosThetaOp;
            if (p == 0) {
                sinThetaOp = sinThetaO * cos2kAlpha[1] - cosThetaO * sin2kAlpha[1];
                cosThetaOp = cosThetaO * cos2kAlpha[1] + sinThetaO * sin2kAlpha[1];
            }

            // Handle remainder of $p$ values for hair scale tilt
            else if (p == 1) {
                sinThetaOp = sinThetaO * cos2kAlpha[0] + cosThetaO * sin2kAlpha[0];
                cosThetaOp = cosThetaO * cos2kAlpha[0] - sinThetaO * sin2kAlpha[0];
            } else if (p == 2) {
                sinThetaOp = sinThetaO * cos2kAlpha[2] + cosThetaO * sin2kAlpha[2];
                cosThetaOp = cosThetaO * cos2kAlpha[2] - sinThetaO * sin2kAlpha[2];
            } else {
                sinThetaOp = sinThetaO;
                cosThetaOp = cosThetaO;
            }

            // Handle out-of-range $\cos \thetao$ from scale adjustment
            cosThetaOp = dr::abs(cosThetaOp);
            fsum += 
                Mp(cosThetaI, cosThetaOp, sinThetaI, sinThetaOp, v[p]) * ap[p] *
                Np(phi, p, s, gammaO, gammaT);
        }

        // Compute contribution of remaining terms after _pMax_
        fsum += Mp(cosThetaI, cosThetaO, sinThetaI, sinThetaO, v[pMax]) * ap[pMax] /
                (2.f * dr::Pi<ScalarFloat>);
        // CHECK(!std::isinf(fsum.y()) && !std::isnan(fsum.y()));   

        return depolarizer<Spectrum>(fsum) & active;
    }

    Float pdf(const BSDFContext &ctx, const SurfaceInteraction3f &si,
              const Vector3f &wo, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        if (!ctx.is_enabled(BSDFFlags::DiffuseReflection))
            return 0.f;

        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);

        Float pdf = warp::square_to_cosine_hemisphere_pdf(wo);

        return dr::select(cos_theta_i > 0.f && cos_theta_o > 0.f, pdf, 0.f);
    }

    std::pair<Spectrum, Float> eval_pdf(const BSDFContext &ctx,
                                        const SurfaceInteraction3f &si,
                                        const Vector3f &wo,
                                        Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        if (!ctx.is_enabled(BSDFFlags::DiffuseReflection))
            return { 0.f, 0.f };

        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);

        active &= cos_theta_i > 0.f && cos_theta_o > 0.f;

        UnpolarizedSpectrum value =
            m_reflectance->eval(si, active) * dr::InvPi<Float> * cos_theta_o;

        Float pdf = warp::square_to_cosine_hemisphere_pdf(wo);

        return { depolarizer<Spectrum>(value) & active, dr::select(active, pdf, 0.f) };
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Hair[" << std::endl
            // << "  reflectance = " << string::indent(m_reflectance) << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()

private:
    static const int pMax = 3;
    constexpr static const ScalarFloat SqrtPiOver8 = 0.626657069;
    ref<Texture> sigma_a; 
    ref<Texture> m_reflectance;
    Float beta_m, beta_n, alpha;
    Float eta;

    // Float or ScalarFloat?
    // Float enumlanin, pheomelanin; // color

    // Helper function
    static Float I0(Float x) {
        Float val = 0;
        Float x2i = 1;
        int64_t ifact = 1;
        int i4 = 1;
        // I0(x) \approx Sum_i x^(2i) / (4^i (i!)^2)
        for (int i = 0; i < 10; ++i) {
            if (i > 1) ifact *= i;
            val += x2i / (i4 * dr::sqr(ifact));
            x2i *= x * x;
            i4 *= 4;
        }
        return val;
    }

    static Float LogI0(Float x) {
        return dr::select(
            x > 12,
            x + 0.5f * (-dr::log(2 * dr::Pi<ScalarFloat>) + dr::log(1 / x) + 1 / (8 * x)),
            dr::log(I0(x))
        );
    }

    static Float Mp(Float cosThetaI, Float cosThetaO, Float sinThetaI,
                    Float sinThetaO, Float v) {
        Float a = cosThetaI * cosThetaO / v;
        Float b = sinThetaI * sinThetaO / v;
        Float mp =
            dr::select(v <= .1f, 
                dr::exp(LogI0(a) - b - 1 / v + 0.6931f + dr::log(1 / (2 * v))),
                dr::exp(-b) * I0(a)) / (dr::sinh(1 / v) * 2 * v
            );
        // CHECK(!std::isinf(mp) && !std::isnan(mp));
        return mp;
    }

    static Float Np(Float phi, int p, Float s, Float gammaO, Float gammaT) {
        Float Phi = 2 * p * gammaT - 2 * gammaO + p * dr::Pi<ScalarFloat>;
        Float dphi = phi - Phi;

        // Remap _dphi_ to $[-\pi,\pi]$
        // while (dphi > dr::Pi<Float>) dphi -= 2 * dr::Pi<Float>;
        // while (dphi < -dr::Pi<Float>) dphi += 2 * dr::Pi<Float>;
        dphi = angleMap(dphi);

        return TrimmedLogistic(dphi, s, -dr::Pi<Float>, dr::Pi<Float>);
    }

    static inline Float Logistic(Float x, Float s) {
        x = dr::abs(x);
        return dr::exp(-x / s) / (s * dr::sqr(1 + dr::exp(-x / s)));
    }

    static inline Float LogisticCDF(Float x, Float s) {
        return 1 / (1 + dr::exp(-x / s));
    }

    static inline Float TrimmedLogistic(Float x, Float s, Float a, Float b) {
        // CHECK_LT(a, b);
        return Logistic(x, s) / (LogisticCDF(b, s) - LogisticCDF(a, s));
    }

    static inline Float angleMap(Float dphi){
        Float pi = dr::Pi<Float>;
        Float angle = dr::fmod(dphi, 2 * pi);
        angle = dr::select(angle < -pi, angle + 2 * pi, angle);
        angle = dr::select(angle > pi, angle - 2 * pi, angle);
        return angle;
    }
};

MI_IMPLEMENT_CLASS_VARIANT(Hair, BSDF)
MI_EXPORT_PLUGIN(Hair, "Hair material")
NAMESPACE_END(mitsuba)
