#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/endpoint.h>
#include <mitsuba/render/medium.h>

NAMESPACE_BEGIN(mitsuba)

Endpoint::Endpoint(const Properties &props) {
    m_world_transform =
        props.animated_transform("to_world", Transform4f()).get();
}

Endpoint::~Endpoint() { }

void Endpoint::set_shape(Shape *shape) {
    m_shape = shape;
}

void Endpoint::set_medium(Medium *medium) {
    m_medium = medium;
}

std::pair<Ray3f, Spectrumf>
Endpoint::sample_ray(Float /*time*/, Float /*sample1*/,
                     const Point2f &/*sample2*/,
                     const Point2f &/*sample3*/) const {
    NotImplementedError("sample_ray");
}

std::pair<Ray3fP, SpectrumfP>
Endpoint::sample_ray(FloatP /*time*/, FloatP /*sample1*/,
                     const Point2fP &/*sample2*/,
                     const Point2fP &/*sample3*/,
                     MaskP /*active*/) const {
    NotImplementedError("sample_ray_p");
}

std::pair<DirectionSample3f, Spectrumf>
Endpoint::sample_direction(const Interaction3f &/*it*/,
                           const Point2f &/*sample*/) const {
    NotImplementedError("sample_direction");
}

std::pair<DirectionSample3fP, SpectrumfP>
Endpoint::sample_direction(const Interaction3fP &/*it*/,
                           const Point2fP &/*sample*/,
                           MaskP /*active*/) const {
    NotImplementedError("sample_direction_p");
}

Float Endpoint::pdf_direction(const Interaction3f &/*it*/,
                           const DirectionSample3f &/*ds*/) const {
    NotImplementedError("pdf_direction");
}

FloatP Endpoint::pdf_direction(const Interaction3fP &/*it*/,
                            const DirectionSample3fP &/*ds*/,
                            MaskP /*active*/) const {
    NotImplementedError("pdf_direction_p");
}

Spectrumf Endpoint::eval(const SurfaceInteraction3f &/*si*/) const {
    NotImplementedError("eval");
}

SpectrumfP Endpoint::eval(const SurfaceInteraction3fP &/*si*/,
                          MaskP /*active*/) const {
    NotImplementedError("eval_p");
}

MTS_IMPLEMENT_CLASS(Endpoint, Object)

NAMESPACE_END(mitsuba)
