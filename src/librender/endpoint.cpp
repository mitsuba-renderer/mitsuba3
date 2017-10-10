#include <mitsuba/render/endpoint.h>
#include <mitsuba/core/spectrum.h>

NAMESPACE_BEGIN(mitsuba)

Endpoint::Endpoint(const Properties &props)
    : Object(), m_properties(props), m_medium(nullptr),
      m_shape(nullptr), m_type(0) {
    set_world_transform(
        props.animated_transform("to_world", Transform4f()).get()
    );
}

Endpoint::~Endpoint() { }


Spectrumf Endpoint::sample_position(PositionSample3f &/*p_rec*/,
    const Point2f &/*sample*/, const Point2f * /*extra*/) const {
    NotImplementedError("sample_position");
    return Spectrumf(0.0f);
}
SpectrumfP Endpoint::sample_position(PositionSample3fP &/*p_rec*/,
    const Point2fP &/*sample*/, const Point2fP * /*extra*/) const {
    NotImplementedError("sample_position");
    return SpectrumfP(0.0f);
}

Spectrumf Endpoint::sample_direction(
    DirectionSample3f &/*d_rec*/, PositionSample3f &/*p_rec*/,
    const Point2f &/*sample*/, const Point2f * /*extra*/) const {
    NotImplementedError("sample_direction");
    return Spectrumf(0.0f);
}
SpectrumfP Endpoint::sample_direction(
    DirectionSample3fP &/*d_rec*/, PositionSample3fP &/*p_rec*/,
    const Point2fP &/*sample*/, const Point2fP * /*extra*/) const {
    NotImplementedError("sample_direction");
    return SpectrumfP(0.0f);
}

Spectrumf Endpoint::sample_direct(DirectSample3f &/*d_rec*/,
                                const Point2f &/*sample*/) const {
    NotImplementedError("sample_direct");
    return Spectrumf(0.0f);
}
SpectrumfP Endpoint::sample_direct(DirectSample3fP &/*d_rec*/,
                                const Point2fP &/*sample*/) const {
    NotImplementedError("sample_direct");
    return SpectrumfP(0.0f);
}

Spectrumf Endpoint::eval_position(const PositionSample3f &/*p_rec*/) const {
    NotImplementedError("eval_position");
    return Spectrumf(0.0f);
}
SpectrumfP Endpoint::eval_position(const PositionSample3fP &/*p_rec*/) const {
    NotImplementedError("eval_position");
    return SpectrumfP(0.0f);
}

Spectrumf Endpoint::eval_direction(const DirectionSample3f &/*d_rec*/,
                                 const PositionSample3f &/*p_rec*/) const {
    NotImplementedError("eval_direction");
    return Spectrumf(0.0f);
}
SpectrumfP Endpoint::eval_direction(const DirectionSample3fP &/*d_rec*/,
                                  const PositionSample3fP &/*p_rec*/) const {
    NotImplementedError("eval_direction");
    return SpectrumfP(0.0f);
}

Float Endpoint::pdf_position(const PositionSample3f &/*p_rec*/) const {
    NotImplementedError("pdf_position");
    return Float(0.0f);
}
FloatP Endpoint::pdf_position(const PositionSample3fP &/*p_rec*/) const {
    NotImplementedError("pdf_position");
    return FloatP(0.0f);
}

Float Endpoint::pdf_direction(const DirectionSample3f &/*d_rec*/,
                            const PositionSample3f &/*p_rec*/) const {
    NotImplementedError("pdf_direction");
    return Float(0.0f);
}
FloatP Endpoint::pdf_direction(const DirectionSample3fP &/*d_rec*/,
                             const PositionSample3fP &/*p_rec*/) const {
    NotImplementedError("pdf_direction");
    return FloatP(0.0f);
}

Float Endpoint::pdf_direct(const DirectSample3f &/*d_rec*/) const {
    NotImplementedError("pdf_direct");
    return Float(0.0f);
}
FloatP Endpoint::pdf_direct(const DirectSample3fP &/*d_rec*/) const {
    NotImplementedError("pdf_direct");
    return FloatP(0.0f);
}

const AnimatedTransform *Endpoint::world_transform() const {
    return m_world_transform.get();
}

void Endpoint::set_world_transform(const AnimatedTransform *trafo) {
    m_world_transform = trafo;
    ref<AnimatedTransform> atrafo(new AnimatedTransform(*trafo));
    m_properties.set_animated_transform("to_world", atrafo, false);
}

ref<Shape> Endpoint::create_shape(const Scene * /*scene*/) {
    return nullptr;
}

MTS_IMPLEMENT_CLASS(Endpoint, Object)

NAMESPACE_END(mitsuba)
