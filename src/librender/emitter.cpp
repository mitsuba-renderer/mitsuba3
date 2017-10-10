#include <mitsuba/core/spectrum.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/endpoint.h>

NAMESPACE_BEGIN(mitsuba)

Emitter::Emitter(const Properties &props)
    : Endpoint(props) { }

Emitter::~Emitter() { }


Spectrumf Emitter::eval(const SurfaceInteraction3f &/*its*/,
                        const Vector3f &/*d*/) const {
    NotImplementedError("eval");
    return Spectrumf(0.0f);
}
SpectrumfP Emitter::eval(const SurfaceInteraction3fP &/*its*/,
                         const Vector3fP &/*d*/) const {
    NotImplementedError("eval");
    return SpectrumfP(0.0f);
}

std::pair<Ray3f, Spectrumf> Emitter::sample_ray(
    const Point2f &/*spatial_sample*/,
    const Point2f &/*directional_sample*/,
    Float /*time*/) const {
    NotImplementedError("sample_ray");
    return { Ray3f(), Spectrumf(0.0f), };
}
std::pair<Ray3fP, SpectrumfP> Emitter::sample_ray(
    const Point2fP &/*spatial_sample*/,
    const Point2fP &/*directional_sample*/,
    FloatP /*time*/) const {
    NotImplementedError("sample_ray");
    return { Ray3fP(), SpectrumfP(0.0f) };
}

ref<Bitmap> Emitter::bitmap(const Vector2i &/*size_hint*/) const {
    NotImplementedError("bitmap");
    return nullptr;
}


Spectrumf Emitter::eval_environment(const RayDifferential3f &/*ray*/) const {
    NotImplementedError("eval_environment");
    return Spectrumf(0.0f);
}
SpectrumfP Emitter::eval_environment(const RayDifferential3fP &/*ray*/) const {
    NotImplementedError("eval_environment");
    return SpectrumfP(0.0f);
}

bool Emitter::fill_direct_sample(
        DirectSample3f &/*d_rec*/, const Ray3f &/*ray*/) const {
    NotImplementedError("fill_direct_sample");
    return false;
}
bool Emitter::fill_direct_sample(
        DirectSample3fP &/*d_rec*/, const Ray3fP &/*ray*/) const {
    NotImplementedError("fill_direct_sample");
    return false;
}

MTS_IMPLEMENT_CLASS(Emitter, Endpoint)
NAMESPACE_END(mitsuba)
