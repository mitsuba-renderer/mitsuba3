#include <mitsuba/render/bsdf.h>

NAMESPACE_BEGIN(mitsuba)

class SmoothDiffuse final : public BSDF {
public:
    SmoothDiffuse(const Properties &) {
        m_flags = EDiffuseReflection | EFrontSide;
    }


    MTS_DECLARE_CLASS()
};

MTS_IMPLEMENT_CLASS(SmoothDiffuse, BSDF)
MTS_EXPORT_PLUGIN(SmoothDiffuse, "Smooth diffuse BRDF")

NAMESPACE_END(mitsuba)
