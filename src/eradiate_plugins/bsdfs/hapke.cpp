#include <mitsuba/core/frame.h>
#include <mitsuba/core/fwd.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _plugin-bsdf-hapke:

Hapke surface model (:monosp:`hapke`)
-------------------------------------

The model uses 6 parameters:

 * - w
   - |spectrum| or |texture|
   - :math:`0 \le w \le 1`.
   - |exposed| |differentiable|

 * - b
   - |spectrum| or |texture|
   - :math:`0 \le w \le 1`.
   - |exposed| |differentiable|

 * - c
   - |spectrum| or |texture|
   - :math:`0 \le w \le 1`.
   - |exposed| |differentiable|

 * - theta (degree)
   - |spectrum| or |texture|
   - :math:`0 \le w \le 90`.
   - |exposed| |differentiable|

 * - B_0
   - |spectrum| or |texture|
   - :math:`0 \le w \le 1`.
   - |exposed| |differentiable|

 * - h
   - |spectrum| or |texture|
   - :math:`0 \le w \le 1`.
   - |exposed| |differentiable|

Implement the Hapke BSDF model as proposed by Bruce Hapke in 1984
(https://doi.org/10.1016/0019-1035(84)90054-X).

All parameters are required.

*/

// In particular, and for quick reference, equations 1 to 3, 14 to 18, 31 to 36,
// 45 to 58 from the reference paper are directly implemented in this module.

MI_VARIANT
class HapkeBSDF final : public BSDF<Float, Spectrum> {
public:
    MI_IMPORT_BASE(BSDF, m_flags, m_components)
    MI_IMPORT_TYPES(Texture)

    HapkeBSDF(const Properties &props) : Base(props) {

        m_w     = props.texture<Texture>("w");
        m_b     = props.texture<Texture>("b");
        m_c     = props.texture<Texture>("c");
        m_theta = props.texture<Texture>("theta");
        m_B_0   = props.texture<Texture>("B_0");
        m_h     = props.texture<Texture>("h");

        if (dr::any((m_w < 0.f) | (m_w > 1.f))) {
            throw("The single scattering albedo 'w' must be in [0; 1]");
        }

        if (dr::any((m_b < 0.f) | (m_b > 1.f))) {
            throw("The anisotropy parameter 'b' must be in [0; 1]");
        }

        if (dr::any((m_c < 0.f) | (m_c > 1.f))) {
            throw("The scattering coefficient 'c' must be in [0; 1]");
        }

        if (dr::any((m_theta < 0.f) | (m_theta > 90.f))) {
            throw("The photometric roughness 'theta' must be in [0; 90]°");
        }

        if (dr::any((m_B_0 < 0.f) | (m_B_0 > 1.f))) {
            throw("The shadow hiding opposition effect amplitude 'B_0' must be "
                  "in [0; 1]");
        }

        if (dr::any((m_h < 0.f) | (m_h > 1.f))) {
            throw("The shadow hiding opposition effect width 'h' must be in "
                  "[0; 1]");
        }

        m_flags = BSDFFlags::GlossyReflection | BSDFFlags::FrontSide;
        dr::set_attr(this, "flags", m_flags);
        m_components.push_back(m_flags);
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_object("w", m_w.get(), +ParamFlags::Differentiable);
        callback->put_object("b", m_b.get(), +ParamFlags::Differentiable);
        callback->put_object("c", m_c.get(), +ParamFlags::Differentiable);
        callback->put_object("theta", m_theta.get(),
                             +ParamFlags::Differentiable);
        callback->put_object("B_0", m_B_0.get(), +ParamFlags::Differentiable);
        callback->put_object("h", m_h.get(), +ParamFlags::Differentiable);
    }

    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx,
                                             const SurfaceInteraction3f &si,
                                             Float /* position_sample */,
                                             const Point2f &direction_sample,
                                             Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFSample, active);

        Float cos_theta_i = Frame3f::cos_theta(si.wi);
        BSDFSample3f bs   = dr::zeros<BSDFSample3f>();

        active &= cos_theta_i > 0.f;
        if (unlikely(dr::none_or<false>(active) ||
                     !ctx.is_enabled(BSDFFlags::GlossyReflection)))
            return { bs, 0.f };

        bs.wo           = warp::square_to_cosine_hemisphere(direction_sample);
        bs.pdf          = warp::square_to_cosine_hemisphere_pdf(bs.wo);
        bs.eta          = 1.f;
        bs.sampled_type = +BSDFFlags::GlossyReflection;
        bs.sampled_component = 0;

        UnpolarizedSpectrum value =
            eval_hapke(si, bs.wo, active) * Frame3f::cos_theta(bs.wo) / bs.pdf;

        return { bs, depolarizer<Spectrum>(value) & (active && bs.pdf > 0.f) };
    }

    inline const Spectrum eval_H(const Spectrum &w, const Spectrum &x) const {
        const Spectrum gamma = dr::sqrt(1.f - w);
        const Spectrum ro    = (1.f - gamma) / (1.f + gamma);
        return 1.f / (1.f - w * x *
                                (ro + (1.f - 2.f * ro * x) * 0.5f *
                                          dr::log((1.f + x) / x)));
    }

    inline const Spectrum eval_chi(const Spectrum &tan_theta) const {
        return 1 / dr::sqrt(1.f + dr::Pi<ScalarFloat> * dr::sqr(tan_theta));
    }

    inline const Float eval_f(const Float &phi) const {
        const Float clamped_phi_div2 = dr::clamp(phi / 2.f, 0.f, dr::Pi<Float> / 2.f - dr::Epsilon<Float>);
        return dr::exp(-2.f * dr::tan(clamped_phi_div2));
    }

    inline const Spectrum eval_E1(const Spectrum &tan_theta,
                                  const Float &x) const {
        return dr::exp(-2.f * dr::InvPi<Float> / tan_theta / dr::tan(x));
    }

    inline const Spectrum eval_E2(const Spectrum &tan_theta,
                                  const Float &x) const {
        return dr::exp(-dr::InvPi<Float> / dr::sqr(tan_theta) /
                       dr::sqr(dr::tan(x)));
    }

    inline const Spectrum eval_eta_0e(const Spectrum &chi, const Float &cos_i,
                                      const Float &sin_i,
                                      const Spectrum &tan_theta,
                                      const Spectrum &E2_i,
                                      const Spectrum &E1_i) const {
        return chi * (cos_i + sin_i * tan_theta * E2_i / (2.f - E1_i));
    }

    inline const Spectrum eval_eta_e(const Spectrum &chi, const Float &cos_e,
                                     const Float &sin_e,
                                     const Spectrum &tan_theta,
                                     const Spectrum &E2_e,
                                     const Spectrum &E1_e) const {
        return chi * (cos_e + sin_e * tan_theta * E2_e / (2.f - E1_e));
    }

    inline const Spectrum eval_mu(const Spectrum &tan_theta, const Float &e,
                                  const Float &i, const Float &cos_x,
                                  const Float &sin_x, const Float &phi,
                                  const Float &opt_cos_phi,
                                  const Float &sign) const {

        const Spectrum chi = eval_chi(tan_theta);

        const Spectrum E1_e = eval_E1(tan_theta, e);
        const Spectrum E1_i = eval_E1(tan_theta, i);
        const Spectrum E2_e = eval_E2(tan_theta, e);
        const Spectrum E2_i = eval_E2(tan_theta, i);

        const Float sin_phi_div2 = dr::sin(phi * 0.5f);
        const Float phi_div_pi   = phi * dr::InvPi<ScalarFloat>;

        return chi * (cos_x + sin_x * tan_theta *
                                  (opt_cos_phi * E2_e +
                                   sign * dr::sqr(sin_phi_div2) * E2_i) /
                                  (2.f - E1_e - phi_div_pi * E1_i));
    }

    inline const Spectrum eval_mu_eG(const Spectrum &tan_theta, const Float &e,
                                     const Float &i, const Float &phi,
                                     const Float &cos_phi) const {

        const Float opt_cos_phi = dr::select(e <= i, cos_phi, 1.f);
        const Float sign        = dr::select(e <= i, 1.f, -1.f);
        const Float cos_e       = dr::cos(e);
        const Float sin_e       = dr::sin(e);

        const Float a = dr::select(e <= i, i, e);
        const Float b = dr::select(e <= i, e, i);

        return eval_mu(tan_theta, a, b, cos_e, sin_e, phi, opt_cos_phi, sign);
    }

    inline const Spectrum eval_mu_0eG(const Spectrum &tan_theta, const Float &e,
                                      const Float &i, const Float &phi,
                                      const Float &cos_phi) const {

        const Float opt_cos_phi = dr::select(e <= i, 1.f, cos_phi);
        const Float sign        = dr::select(e <= i, -1.f, 1.f);
        const Float cos_i       = dr::cos(i);
        const Float sin_i       = dr::sin(i);

        const Float a = dr::select(e <= i, i, e);
        const Float b = dr::select(e <= i, e, i);

        return eval_mu(tan_theta, a, b, cos_i, sin_i, phi, opt_cos_phi, sign);
    }

    inline const Spectrum eval_M(const Spectrum &w, const Spectrum &mu_0eG,
                                 const Spectrum &mu_eG) const {
        // Multiple scattering function
        return eval_H(w, mu_0eG) * eval_H(w, mu_eG) - 1.f;
    }

    inline const Float eval_cos_g(const Float &mu_0, const Float &mu,
                                  const Float &sin_i, const Float &sin_e,
                                  const Float &cos_phi) const {
        return mu_0 * mu + sin_i * sin_e * cos_phi;
    }

    inline const Spectrum eval_P(const Spectrum &b, const Spectrum &c,
                                 const Spectrum &cos_g) const {
        // Fonction de phase P
        const Spectrum numerator = 1.f - sqr(b);
        const Spectrum term1 =
            (1.f - c) * numerator /
            dr::pow(1 + 2 * b * cos_g + dr::sqr(b), 3.f / 2.f);
        const Spectrum term2 =
            c * numerator / dr::pow(1 - 2 * b * cos_g + dr::sqr(b), 3.f / 2.f);
        return term1 + term2;
    }

    inline const Spectrum eval_S(const Spectrum &eta_e, const Spectrum &eta_0e,
                                 const Spectrum &chi, const Float &e,
                                 const Float &i, const Float &mu,
                                 const Spectrum &mu_0, const Spectrum &mu_e,
                                 const Spectrum &f) const {

        const Spectrum opt_mu  = dr::select(e < i, mu, mu_0);
        const Spectrum opt_eta = dr::select(e < i, eta_e, eta_0e);

        return (mu_e * mu_0 * chi) /
               (eta_e * eta_0e * (1.f - f + f * chi * opt_mu / opt_eta));
    }

    inline const Spectrum eval_B(const Spectrum &B_0, const Spectrum &h,
                                 const Spectrum &g) const {
        // Opposition effect
        return B_0 / (1.f + 1.f / h * dr::tan(g / 2));
    }

    const Spectrum eval_hapke(const SurfaceInteraction3f &si,
                              const Vector3f &wo, Mask active) const {

        const Spectrum theta     = dr::deg_to_rad(m_theta->eval(si, active));
        const Spectrum tan_theta = dr::tan(theta);
        const Spectrum w         = m_w->eval(si, active);

        auto [sin_phi_e, cos_phi_e] = Frame3f::sincos_phi(wo);
        auto [sin_phi_i, cos_phi_i] = Frame3f::sincos_phi(si.wi);
        const Float cos_phi = cos_phi_e * cos_phi_i + sin_phi_e * sin_phi_i;
        const Float sin_e   = Frame3f::sin_theta(wo);
        const Float mu      = Frame3f::cos_theta(wo);
        const Float tan_e   = Frame3f::tan_theta(wo);
        const Float sin_i   = Frame3f::sin_theta(si.wi);
        const Float mu_0    = Frame3f::cos_theta(si.wi);
        const Float tan_i   = Frame3f::tan_theta(si.wi);

        const Float i   = dr::atan(tan_i);
        const Float e   = dr::atan(tan_e);
        Float fr_phi = dr::safe_acos(cos_phi);
        const Float phi = dr::abs(dr::select(fr_phi > dr::Pi<Float>, 2.f * dr::Pi<Float> - fr_phi, fr_phi));

        const Spectrum w_div_4 = w * 0.25f;

        const Spectrum mu_0eG = eval_mu_0eG(tan_theta, e, i, phi, cos_phi);
        const Spectrum mu_eG  = eval_mu_eG(tan_theta, e, i, phi, cos_phi);

        const Spectrum mu_ratio = mu_0eG / (mu_0eG + mu_eG) / mu_0;

        const Spectrum b  = m_b->eval(si, active);
        const Spectrum c  = m_c->eval(si, active);
        const Float cos_g = eval_cos_g(mu_0, mu, sin_i, sin_e, cos_phi);
        const Float g     = dr::safe_acos(cos_g);

        const Spectrum P = eval_P(b, c, cos_g);

        const Spectrum B_0 = m_B_0->eval(si, active);
        const Spectrum h   = m_h->eval(si, active);
        const Spectrum B   = eval_B(B_0, h, g);

        const Spectrum M = eval_M(w, mu_0eG, mu_eG);

        const Spectrum f = eval_f(phi);

        const Spectrum chi = eval_chi(tan_theta);

        const Spectrum E1_e = eval_E1(tan_theta, e);
        const Spectrum E1_i = eval_E1(tan_theta, i);
        const Spectrum E2_e = eval_E2(tan_theta, e);
        const Spectrum E2_i = eval_E2(tan_theta, i);

        const Spectrum eta_0e =
            eval_eta_0e(chi, mu_0, sin_i, tan_theta, E2_i, E1_i);
        const Spectrum eta_e =
            eval_eta_e(chi, mu, sin_e, tan_theta, E2_e, E1_e);

        const Spectrum S = eval_S(eta_e, eta_0e, chi, e, i, mu, mu_0, mu_eG, f);

        Log(Trace, "mu ratio %s", mu_ratio);
        Log(Trace, "P %s", P);
        Log(Trace, "B %s", B);
        Log(Trace, "M %s", M);
        Log(Trace, "S %s", S);

        return w_div_4 * mu_ratio * (P * (1 + B) + M) * S;
    }

    Spectrum eval(const BSDFContext & /*ctx*/, const SurfaceInteraction3f &si,
                  const Vector3f &wo, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);

        active &= cos_theta_i > 0.f && cos_theta_o > 0.f;
        const Spectrum value = eval_hapke(si, wo, active);

        return dr::select(
            active, depolarizer<Spectrum>(value) * dr::abs(cos_theta_o), 0.f);
    }

    Float pdf(const BSDFContext &, const SurfaceInteraction3f &si,
              const Vector3f &wo, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);

        Float pdf = warp::square_to_cosine_hemisphere_pdf(wo);

        return dr::select(cos_theta_i > 0.f && cos_theta_o > 0.f, pdf, 0.f);
    }

    std::pair<Spectrum, Float> eval_pdf(const BSDFContext &,
                                        const SurfaceInteraction3f &si,
                                        const Vector3f &wo,
                                        Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);

        active &= cos_theta_i > 0.f && cos_theta_o > 0.f;
        Spectrum value = eval_hapke(si, wo, active);
        Float pdf      = warp::square_to_cosine_hemisphere_pdf(wo);

        return { depolarizer<Spectrum>(value) * dr::abs(cos_theta_o) & active,
                 dr::select(active, pdf, 0.f) };
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "HapkeBSDF["
            << "  w = " << string::indent(m_w) << "," << std::endl
            << "  b = " << string::indent(m_b) << "," << std::endl
            << "  c = " << string::indent(m_c) << "," << std::endl
            << "  theta = " << string::indent(m_theta) << "," << std::endl
            << "  B_0 = " << string::indent(m_B_0) << "," << std::endl
            << "  h = " << string::indent(m_h) << "," << std::endl;
        oss << std::endl << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS();

private:
    ref<Texture> m_w;
    ref<Texture> m_b;
    ref<Texture> m_c;
    ref<Texture> m_theta;
    ref<Texture> m_B_0;
    ref<Texture> m_h;
};

MI_IMPLEMENT_CLASS_VARIANT(HapkeBSDF, BSDF)
MI_EXPORT_PLUGIN(HapkeBSDF, "Hapke BSDF")
NAMESPACE_END(mitsuba)
