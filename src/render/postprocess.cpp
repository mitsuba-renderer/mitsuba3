#include <mitsuba/core/properties.h>
#include <mitsuba/render/postprocess.h>

NAMESPACE_BEGIN(mitsuba)

MI_VARIANT PostProcess<Float, Spectrum>::PostProcess(const Properties &props)
    : JitObject<PostProcess>(props.id()) { }

MI_VARIANT PostProcess<Float, Spectrum>::~PostProcess() { }

MI_VARIANT std::string PostProcess<Float, Spectrum>::to_string() const {
    return "PostProcess[]";
}

MI_INSTANTIATE_CLASS(PostProcess)
NAMESPACE_END(mitsuba)
