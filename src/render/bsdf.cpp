#include <mitsuba/render/bsdf.h>
#include <mitsuba/core/properties.h>

NAMESPACE_BEGIN(mitsuba)

MI_VARIANT BSDF<Float, Spectrum>::BSDF(const Properties &props)
    : m_flags(+BSDFFlags::Empty), m_id(props.id()) { }

MI_VARIANT BSDF<Float, Spectrum>::~BSDF() { }

MI_VARIANT std::pair<Spectrum, Float>
BSDF<Float, Spectrum>::eval_pdf(const BSDFContext &ctx,
                                const SurfaceInteraction3f &si,
                                const Vector3f &wo,
                                Mask active) const {
    return { eval(ctx, si, wo, active), pdf(ctx, si, wo, active) };
}

MI_VARIANT std::tuple<Spectrum, Float, typename BSDF<Float, Spectrum>::BSDFSample3f, Spectrum>
BSDF<Float, Spectrum>::eval_pdf_sample(const BSDFContext &ctx,
                                       const SurfaceInteraction3f &si,
                                       const Vector3f &wo,
                                       Float sample1,
                                       const Point2f &sample2,
                                       Mask active) const {
        auto [e_val, pdf_val] = eval_pdf(ctx, si, wo, active);
        auto [bs, bsdf_weight] = sample(ctx, si, sample1, sample2, active);
        return { e_val, pdf_val, bs, bsdf_weight };
}

MI_VARIANT Spectrum BSDF<Float, Spectrum>::eval_null_transmission(
    const SurfaceInteraction3f & /* si */, Mask /* active */) const {
    return 0.f;
}

MI_VARIANT Spectrum BSDF<Float, Spectrum>::eval_diffuse_reflectance(
    const SurfaceInteraction3f &si, Mask active) const {
    Vector3f wo = Vector3f(0.0f, 0.0f, 1.0f);
    BSDFContext ctx;
    return eval(ctx, si, wo, active) * dr::Pi<Float>;
}

std::ostream &operator<<(std::ostream &os, const TransportMode &mode) {
    switch (mode) {
        case TransportMode::Radiance:   os << "radiance"; break;
        case TransportMode::Importance: os << "importance"; break;
        default:                        os << "invalid"; break;
    }
    return os;
}

MI_IMPLEMENT_CLASS_VARIANT(BSDF, Object, "bsdf")
MI_INSTANTIATE_CLASS(BSDF)
NAMESPACE_END(mitsuba)
