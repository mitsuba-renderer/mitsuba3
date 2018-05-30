#include <mitsuba/render/bsdf.h>

NAMESPACE_BEGIN(mitsuba)

template struct MTS_EXPORT_RENDER BSDFSample<Point3f>;
template struct MTS_EXPORT_RENDER BSDFSample<Point3fP>;

BSDF::BSDF() : m_flags(0) { }
BSDF::~BSDF() { }

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

MTS_IMPLEMENT_CLASS(BSDF, Object)
NAMESPACE_END(mitsuba)
