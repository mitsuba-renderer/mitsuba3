#include <mitsuba/render/bsdf.h>
#include <mitsuba/core/properties.h>

NAMESPACE_BEGIN(mitsuba)

BSDF::BSDF(const Properties &props) : m_flags(0), m_id(props.id()) { }
BSDF::~BSDF() { }

std::string BSDF::id() const { return m_id; }

std::pair<BSDFSample3f, MuellerMatrixSf>
BSDF::sample_pol(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                 Float sample1, const Point2f &sample2) const {
    BSDFSample3f s;
    Spectrumf f;
    std::tie(s, f) = sample(ctx, si, sample1, sample2);
    return std::make_pair(s, mueller::depolarizer(f));
}

std::pair<BSDFSample3fP, MuellerMatrixSfP>
BSDF::sample_pol(const BSDFContext &ctx, const SurfaceInteraction3fP &si,
                 FloatP sample1, const Point2fP &sample2,
    MaskP active) const {
    BSDFSample3fP s;
    SpectrumfP f;
    std::tie(s, f) = sample(ctx, si, sample1, sample2, active);
    return std::make_pair(s, mueller::depolarizer(f));
}

#if defined(MTS_ENABLE_AUTODIFF)
std::pair<BSDFSample3fD, MuellerMatrixSfD>
BSDF::sample_pol(const BSDFContext &ctx, const SurfaceInteraction3fD &si,
                 FloatD sample1, const Point2fD &sample2,
    MaskD active) const {
    BSDFSample3fD s;
    SpectrumfD f;
    std::tie(s, f) = sample(ctx, si, sample1, sample2, active);
    return std::make_pair(s, mueller::depolarizer(f));
}
#endif

MuellerMatrixSf
BSDF::eval_pol(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                const Vector3f &wo) const {
    return mueller::depolarizer(eval(ctx, si, wo));
}

MuellerMatrixSfP
BSDF::eval_pol(const BSDFContext &ctx, const SurfaceInteraction3fP &si,
               const Vector3fP &wo, MaskP active) const {
    return mueller::depolarizer(eval(ctx, si, wo, active));
}

#if defined(MTS_ENABLE_AUTODIFF)
MuellerMatrixSfD
BSDF::eval_pol(const BSDFContext &ctx, const SurfaceInteraction3fD &si,
               const Vector3fD &wo, MaskD active) const {
    return mueller::depolarizer(eval(ctx, si, wo, active));
}
#endif

std::ostream &operator<<(std::ostream &os, const BSDFContext& ctx) {
    os << "BSDFContext[" << std::endl
        << "  mode = " << ctx.mode << "," << std::endl
        << "  type_mask = " << type_mask_to_string(ctx.type_mask) << "," << std::endl
        << "  component = ";

    if (ctx.component == (uint32_t) -1)
        os << "all";
    else
        os << ctx.component;
    os  << std::endl << "]";
    return os;
}

MTS_IMPLEMENT_CLASS(BSDF, DifferentiableObject)
NAMESPACE_END(mitsuba)
