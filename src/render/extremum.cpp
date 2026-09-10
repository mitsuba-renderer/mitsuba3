#include <mitsuba/core/properties.h>
#include <mitsuba/render/extremum.h>

NAMESPACE_BEGIN(mitsuba)

MI_VARIANT Extremum<Float, Spectrum>::Extremum()
    : JitObject<Extremum>(""), m_scale(1.f) {
}

MI_VARIANT Extremum<Float, Spectrum>::Extremum(const Properties &props)
    : JitObject<Extremum>(props.id()), m_scale(1.f) {
}

MI_VARIANT Extremum<Float, Spectrum>::~Extremum() {
}

MI_VARIANT void Extremum<Float, Spectrum>::update_extremum(
    const ScalarBoundingBox3f &bbox, const Volume *volume,
    std::optional<ScalarFloat> scale) {
    // set scale if provided
    if (scale)
        set_scale(scale.value());
    // set validity bbox
    set_bbox(bbox);

    if (!m_bbox.valid())
        Throw("Extremum::update_extremum() called with an invalid bbox.");

    // rebuild the extremum structure
    build(volume);
}

MI_VARIANT
TrackingState<Float, Spectrum>
Extremum<Float, Spectrum>::traverse_extremum(
    const Ray3f &/*ray*/,
    Float /*mint*/,
    Float /*maxt*/,
    UInt32 /*channel*/,
    TrackingStateType /*state*/,
    const TrackingFunctionType & /*func*/,
    Mask /*active*/
) const {
    NotImplementedError("traverse_extremum");
}

MI_INSTANTIATE_CLASS(Extremum)
NAMESPACE_END(mitsuba)
