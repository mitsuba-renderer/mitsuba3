#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/spectrum.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class SmoothDiffuse final : public BSDF<Float, Spectrum> {
public:
    MTS_IMPORT_TYPES();
    using ContinuousSpectrum = ContinuousSpectrum<Float, Spectrum>;
    using Base = BSDF<Float, Spectrum>;
    using Base::Base;
    using Base::m_flags;
    using Base::m_components;

    explicit SmoothDiffuse(const Properties &props) : Base(props) {
        // TODO
        m_reflectance = props.spectrum<Float, Spectrum>("reflectance", .5f);
        m_flags = BSDFFlags::DiffuseReflection | BSDFFlags::FrontSide;
        m_components.push_back(m_flags);
    }

    MTS_INLINE
    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                                             Float /* sample1 */, const Point2f &sample2,
                                             Mask active) const override {
        Float cos_theta_i = Frame3f::cos_theta(si.wi);
        BSDFSample3f bs = zero<BSDFSample3f>();

        active &= cos_theta_i > 0.f;
        if (unlikely(none_or<false>(active) ||
                     !ctx.is_enabled(BSDFFlags::DiffuseReflection)))
            return { bs, 0.f };

        bs.wo = warp::square_to_cosine_hemisphere(sample2);
        bs.pdf = warp::square_to_cosine_hemisphere_pdf(bs.wo);
        bs.eta = 1.f;
        bs.sampled_type = (uint32_t) BSDFFlags::DiffuseReflection;
        bs.sampled_component = 0;

        Spectrum value = m_reflectance->eval(si, active);

        return { bs, select(active && bs.pdf > 0, value, 0.f) };
    }

    MTS_INLINE
    Spectrum eval(const BSDFContext &ctx, const SurfaceInteraction3f &si, const Vector3f &wo,
                  Mask active) const override {

        if (!ctx.is_enabled(BSDFFlags::DiffuseReflection))
            return 0.f;

        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);

        Spectrum value = m_reflectance->eval(si, active) *
                         math::InvPi<Float> * cos_theta_o;

        return select(cos_theta_i > 0.f && cos_theta_o > 0.f, value, 0.f);
    }

    MTS_INLINE
    Float pdf(const BSDFContext &ctx, const SurfaceInteraction3f &si, const Vector3f &wo,
              Mask /* active */) const override {
        if (!ctx.is_enabled(BSDFFlags::DiffuseReflection))
            return 0.f;

        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);

        Float pdf = warp::square_to_cosine_hemisphere_pdf(wo);

        return select(cos_theta_i > 0.f && cos_theta_o > 0.f, pdf, 0.f);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "SmoothDiffuse[" << std::endl
            << "  reflectance = " << string::indent(m_reflectance) << std::endl
            << "]";
        return oss.str();
    }

    std::vector<ref<Object>> children() override { return { m_reflectance.get() }; }

    // MTS_IMPLEMENT_BSDF_ALL()
    MTS_DECLARE_CLASS()
private:
    ref<ContinuousSpectrum> m_reflectance;
};

// MTS_IMPLEMENT_CLASS(SmoothDiffuse, BSDF)
// MTS_EXPORT_PLUGIN(SmoothDiffuse, "Smooth diffuse material")

NAMESPACE_END(mitsuba)
