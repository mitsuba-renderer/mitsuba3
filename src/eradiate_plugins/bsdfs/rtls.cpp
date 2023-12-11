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

.. _plugin-bsdf-rtls:

Ross-Thick Li-Sparse reflection model (:monosp:`rtls`)
--------------------------------------------------------

.. pluginparameters::

 * - f_iso
   - |spectrum| or |texture|
   - :math:`f_{iso}`. Default: 0.209741
   - |exposed| |differentiable|

 * - f_geo
   - |spectrum| or |texture|
   - :math:`f_{geo}`. Default: 0.081384
   - |exposed| |differentiable|

 * - f_vol
   - |spectrum| or |texture|
   - :math:`f_{vol}`. Default: 0.004140
   - |exposed| |differentiable|

 * - h
   - |float|
   - :math:`h`. Default: 2.f
   - |exposed|

 * - r
   - |float|
   - :math:`r`. Default: 1.f
   - |exposed|

 * - b
   - |float|
   - :math:`b`. Default: 1.f
   - |exposed|

The RTLS plugin implement the Ross-Thick, Li-Sparse model proposed by
(Strahler et al, 1999) for the MODIS operational the BRDF model Version 5.0

Default parameters for :math:`f_k` parameters are taken from the RAMI4ATM
benchmark test cases defined by the JRC, for measures done using the Sentinel-2A
MSI band 8A spectral region (centered around 865nm):
https://rami-benchmark.jrc.ec.europa.eu

*/

MI_VARIANT
class RTLSBSDF final : public BSDF<Float, Spectrum> {
public:
    MI_IMPORT_BASE(BSDF, m_flags, m_components)
    MI_IMPORT_TYPES(Texture)

    RTLSBSDF(const Properties &props) : Base(props) {
        m_f_iso = props.texture<Texture>("f_iso", 0.209741f);
        m_f_vol = props.texture<Texture>("f_vol", 0.081384f);
        m_f_geo = props.texture<Texture>("f_geo", 0.004140f);

        /*
         * Values from: "MODIS BRDF/Albedo Product:
         * Algorithm Theoretical Basis Document
         * Version 5.0"
         */
        m_h     = props.get<ScalarFloat>("h", 2.f);
        m_r     = props.get<ScalarFloat>("r", 1.f);
        m_b     = props.get<ScalarFloat>("b", 1.f);
        m_flags = BSDFFlags::GlossyReflection | BSDFFlags::FrontSide;
        dr::set_attr(this, "flags", m_flags);
        m_components.push_back(m_flags);
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_object("f_iso", m_f_iso.get(),
                             +ParamFlags::Differentiable);
        callback->put_object("f_vol", m_f_vol.get(),
                             +ParamFlags::Differentiable);
        callback->put_object("f_geo", m_f_geo.get(),
                             +ParamFlags::Differentiable);
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
            eval_rtls(si, bs.wo, active) * Frame3f::cos_theta(bs.wo) / bs.pdf;

        return { bs, depolarizer<Spectrum>(value) & (active && bs.pdf > 0.f) };
    }

    inline const UnpolarizedSpectrum eval_K_iso() const { return 1.0f; }

    inline const UnpolarizedSpectrum eval_K_vol(const Float &cos_theta_i,
                                                const Float &cos_theta_o,
                                                const Float &cos_psi,
                                                const Float &sin_psi,
                                                const Float &psi) const {
        return ((dr::Pi<Float> / 2.f - psi) * cos_psi + sin_psi) /
                   (cos_theta_i + cos_theta_o) -
               (dr::Pi<Float> / 4.f);
    }

    inline const UnpolarizedSpectrum eval_D(const Float &tan_theta_i,
                                            const Float &tan_theta_o,
                                            const Float &cos_d_phi) const {
        return dr::sqrt(dr::sqr(tan_theta_i) + dr::sqr(tan_theta_o) -
                        2.f * tan_theta_i * tan_theta_o * cos_d_phi);
    }

    inline const UnpolarizedSpectrum eval_O(const Float &tan_theta_i,
                                            const Float &tan_theta_o,
                                            const Float &sec_theta_sum,
                                            const Float &cos_d_phi,
                                            const Float &sin_d_phi) const {

        const UnpolarizedSpectrum D =
            eval_D(tan_theta_i, tan_theta_o, cos_d_phi);
        Log(Trace, "D: %s", D);

        const Float tan_sin_prod = tan_theta_i * tan_theta_o * sin_d_phi;
        UnpolarizedSpectrum cos_t =
            (m_h / m_b) * dr::sqrt(dr::sqr(D) + dr::sqr(tan_sin_prod)) /
            sec_theta_sum;

        // Clip cos(t) values outside of [-1; 1]
        cos_t = dr::minimum(cos_t, 1.f);
        cos_t = dr::maximum(cos_t, -1.f);
        Log(Trace, "cos_t: %s", cos_t);

        const UnpolarizedSpectrum t     = dr::acos(cos_t);
        const UnpolarizedSpectrum sin_t = dr::sin(t);

        return (1.f / dr::Pi<Float>) *(t - sin_t * cos_t) * sec_theta_sum;
    }

    inline const UnpolarizedSpectrum
    eval_K_geo(const Float &cos_theta_i, const Float &cos_theta_o,
               const Float &tan_theta_i, const Float &tan_theta_o,
               const Float &cos_d_phi, const Float &sin_d_phi,
               const Float &cos_psi) const {
        const Float sec_theta_i = 1.f / cos_theta_i;
        const Float sec_theta_o = 1.f / cos_theta_o;

        const Float sec_theta_sum = sec_theta_i + sec_theta_o;

        const UnpolarizedSpectrum O = eval_O(
            tan_theta_i, tan_theta_o, sec_theta_sum, cos_d_phi, sin_d_phi);

        Log(Trace, "O: %s", O);

        return O - sec_theta_sum +
               0.5 * (1.f + cos_psi) * sec_theta_i * sec_theta_o;
    }

    const UnpolarizedSpectrum eval_rtls(const SurfaceInteraction3f &si,
                                        const Vector3f &wo, Mask active) const {

        const UnpolarizedSpectrum f_iso = m_f_iso->eval(si, active);
        const UnpolarizedSpectrum f_vol = m_f_vol->eval(si, active);
        const UnpolarizedSpectrum f_geo = m_f_geo->eval(si, active);

        auto [sin_phi_i, cos_phi_i] = Frame3f::sincos_phi(si.wi);
        auto [sin_phi_o, cos_phi_o] = Frame3f::sincos_phi(wo);

        const Float sin_theta_i = Frame3f::sin_theta(si.wi);
        const Float cos_theta_i = Frame3f::cos_theta(si.wi);
        const Float tan_theta_i = Frame3f::tan_theta(si.wi);
        const Float sin_theta_o = Frame3f::sin_theta(wo);
        const Float cos_theta_o = Frame3f::cos_theta(wo);
        const Float tan_theta_o = Frame3f::tan_theta(wo);

        Float cos_d_phi = cos_phi_i * cos_phi_o + sin_phi_i * sin_phi_o;
        Float sin_d_phi = sin_phi_i * cos_phi_o - cos_phi_i * sin_phi_o;

        const Float cos_psi =
            cos_theta_i * cos_theta_o + sin_theta_i * sin_theta_o * cos_d_phi;

        const Float sin_psi = dr::sqrt(1 - dr::sqr(cos_psi));
        const Float psi     = dr::acos(cos_psi);

        const UnpolarizedSpectrum K_iso = eval_K_iso();
        const UnpolarizedSpectrum K_vol =
            eval_K_vol(cos_theta_i, cos_theta_o, cos_psi, sin_psi, psi);

        UnpolarizedSpectrum K_geo;
        if (unlikely(dr::abs(m_r - m_b) > dr::Epsilon<ScalarFloat>)) {
            Log(Debug, "Using different b and r values forcing extra angles "
                       "calculations");

            const Float tan_theta_i_prim = m_b / m_r * tan_theta_i;
            const Float tan_theta_o_prim = m_b / m_r * tan_theta_o;
            const Float theta_i_prim     = dr::atan(tan_theta_i_prim);
            const Float theta_o_prim     = dr::atan(tan_theta_o_prim);
            const Float cos_theta_i_prim = dr::cos(theta_i_prim);
            const Float cos_theta_o_prim = dr::cos(theta_o_prim);
            const Float sin_theta_i_prim = dr::sin(theta_i_prim);
            const Float sin_theta_o_prim = dr::sin(theta_o_prim);
            const Float cos_psi_prim =
                cos_theta_i_prim * cos_theta_o_prim +
                sin_theta_i_prim * sin_theta_o_prim * cos_d_phi;

            K_geo = eval_K_geo(cos_theta_i_prim, cos_theta_o_prim,
                               tan_theta_i_prim, tan_theta_o_prim, cos_d_phi,
                               sin_d_phi, cos_psi_prim);
        } else {
            Log(Trace, "Using similar b and r values, skipping extra angles "
                       "calculations");
            K_geo = eval_K_geo(cos_theta_i, cos_theta_o, tan_theta_i,
                               tan_theta_o, cos_d_phi, sin_d_phi, cos_psi);
        }

        Log(Trace,
            "Intermediate kernel outputs:\n  K_iso: %s\n  K_vol: %s\n  K_geo: "
            "%s",
            K_iso, K_vol, K_geo);

        const UnpolarizedSpectrum value =
            (f_iso * K_iso + f_vol * K_vol + f_geo * K_geo) * dr::InvPi<Float>;

        return value;
    }

    Spectrum eval(const BSDFContext & /*ctx*/, const SurfaceInteraction3f &si,
                  const Vector3f &wo, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);

        active &= cos_theta_i > 0.f && cos_theta_o > 0.f;
        const Spectrum value = eval_rtls(si, wo, active);

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
        Spectrum value = eval_rtls(si, wo, active);
        Float pdf      = warp::square_to_cosine_hemisphere_pdf(wo);

        return { depolarizer<Spectrum>(value) * dr::abs(cos_theta_o) & active,
                 dr::select(active, pdf, 0.f) };
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "RTLSBSDF[" << std::endl
            << "  f_iso = " << string::indent(m_f_iso) << "," << std::endl
            << "  f_vol = " << string::indent(m_f_vol) << "," << std::endl
            << "  f_geo = " << string::indent(m_f_geo) << "," << std::endl
            << "  h = " << string::indent(m_h) << "," << std::endl
            << "  r = " << string::indent(m_r) << "," << std::endl
            << "  b = " << string::indent(m_b) << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS();

private:
    ref<Texture> m_f_iso;
    ref<Texture> m_f_vol;
    ref<Texture> m_f_geo;

    ScalarFloat m_h;
    ScalarFloat m_r;
    ScalarFloat m_b;
};

MI_IMPLEMENT_CLASS_VARIANT(RTLSBSDF, BSDF)
MI_EXPORT_PLUGIN(RTLSBSDF, "Ross-Thick-Li-Sparse BSDF")
NAMESPACE_END(mitsuba)
