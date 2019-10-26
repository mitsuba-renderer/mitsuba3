#include <mitsuba/render/bsdf.h>
#include <mitsuba/core/properties.h>

NAMESPACE_BEGIN(mitsuba)

BSDF::BSDF(const Properties &props) : m_flags(0), m_id(props.id()) { }
BSDF::~BSDF() { }

template <typename Index>
std::string type_mask_to_string(Index type_mask) {
    std::ostringstream oss;
    oss << "{ ";

#define is_set(mask) (type_mask & mask) == mask
    if (is_set(BSDFFlags::All)) {
        oss << "all ";
        type_mask &= ~BSDFFlags::All;
    }
    if (is_set(BSDFFlags::Reflection)) {
        oss << "reflection ";
        type_mask &= ~BSDFFlags::Reflection;
    }
    if (is_set(BSDFFlags::Transmission)) {
        oss << "transmission ";
        type_mask &= ~BSDFFlags::Transmission;
    }
    if (is_set(BSDFFlags::Smooth)) {
        oss << "smooth ";
        type_mask &= ~BSDFFlags::Smooth;
    }
    if (is_set(BSDF::Diffuse)) {
        oss << "diffuse ";
        type_mask &= ~BSDF::Diffuse;
    }
    if (is_set(BSDF::Glossy)) {
        oss << "glossy ";
        type_mask &= ~BSDF::Glossy;
    }
    if (is_set(BSDF::Delta)) {
        oss << "delta";
        type_mask &= ~BSDF::Delta;
    }
    if (is_set(BSDF::Delta1D)) {
        oss << "delta_1d ";
        type_mask &= ~BSDF::Delta1D;
    }
    if (is_set(BSDF::DiffuseReflection)) {
        oss << "diffuse_reflection ";
        type_mask &= ~BSDF::DiffuseReflection;
    }
    if (is_set(BSDF::DiffuseTransmission)) {
        oss << "diffuse_transmission ";
        type_mask &= ~BSDF::DiffuseTransmission;
    }
    if (is_set(BSDF::GlossyReflection)) {
        oss << "glossy_reflection ";
        type_mask &= ~BSDF::GlossyReflection;
    }
    if (is_set(BSDF::GlossyTransmission)) {
        oss << "glossy_transmission ";
        type_mask &= ~BSDF::GlossyTransmission;
    }
    if (is_set(BSDF::DeltaReflection)) {
        oss << "delta_reflection ";
        type_mask &= ~BSDF::DeltaReflection;
    }
    if (is_set(BSDF::DeltaTransmission)) {
        oss << "delta_transmission ";
        type_mask &= ~BSDF::DeltaTransmission;
    }
    if (is_set(BSDF::Delta1DReflection)) {
        oss << "delta_1d_reflection ";
        type_mask &= ~BSDF::Delta1DReflection;
    }
    if (is_set(BSDF::Delta1DTransmission)) {
        oss << "delta_1d_transmission ";
        type_mask &= ~BSDF::Delta1DTransmission;
    }
    if (is_set(BSDF::Null)) {
        oss << "null ";
        type_mask &= ~BSDF::Null;
    }
    if (is_set(BSDF::Anisotropic)) {
        oss << "anisotropic ";
        type_mask &= ~BSDF::Anisotropic;
    }
    if (is_set(BSDF::FrontSide)) {
        oss << "front_side ";
        type_mask &= ~BSDF::FrontSide;
    }
    if (is_set(BSDF::BackSide)) {
        oss << "back_side ";
        type_mask &= ~BSDF::BackSide;
    }
    if (is_set(BSDF::SpatiallyVarying)) {
        oss << "spatially_varying ";
        type_mask &= ~BSDF::SpatiallyVarying;
    }
    if (is_set(BSDF::NonSymmetric)) {
        oss << "non_symmetric ";
        type_mask &= ~BSDF::NonSymmetric;
    }
#undef is_set

    Assert(type_mask == 0);
    oss << "}";
    return oss.str();
}

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

Spectrumf
BSDF::eval_transmission(const SurfaceInteraction3f & /* unused */,
                const Vector3f & /* unused */) const {
    return 0.f;
}

SpectrumfP
BSDF::eval_transmission(const SurfaceInteraction3fP & /* unused */,
                const Vector3fP & /* unused */, MaskP /* unused */) const {
    return 0.f;
}

#if defined(MTS_ENABLE_AUTODIFF)
SpectrumfD
BSDF::eval_transmission(const SurfaceInteraction3fD & /* unused */,
                const Vector3fD & /* unused */, MaskD /* unused */) const {
    return 0.f;
}
#endif

MuellerMatrixSf
BSDF::eval_transmission_pol(const SurfaceInteraction3f &si,
                            const Vector3f &wo) const {
    return mueller::depolarizer(eval_transmission(si, wo));
}

MuellerMatrixSfP
BSDF::eval_transmission_pol(const SurfaceInteraction3fP &si,
                            const Vector3fP &wo, MaskP active) const {
    return mueller::depolarizer(eval_transmission(si, wo, active));
}

#if defined(MTS_ENABLE_AUTODIFF)
MuellerMatrixSfD
BSDF::eval_transmission_pol(const SurfaceInteraction3fD &si,
                            const Vector3fD &wo, MaskD active) const {
    return mueller::depolarizer(eval_transmission(si, wo, active));
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

std::ostream &operator<<(std::ostream &os, const TransportMode &mode) {
    switch (mode) {
        case Radiance:   os << "radiance"; break;
        case Importance: os << "importance"; break;
        default:         os << "invalid"; break;
    }
    return os;
}

NAMESPACE_END(mitsuba)
