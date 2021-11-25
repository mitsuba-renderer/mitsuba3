#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/endpoint.h>
#include <mitsuba/render/medium.h>

NAMESPACE_BEGIN(mitsuba)

MTS_VARIANT Endpoint<Float, Spectrum>::Endpoint(const Properties &props) : m_id(props.id()) {
    m_to_world = (ScalarTransform4f) props.get<ScalarTransform4f>(
        "to_world", ScalarTransform4f());

    for (auto &[name, obj] : props.objects(false)) {
        Medium *medium = dynamic_cast<Medium *>(obj.get());
        if (medium) {
            if (m_medium)
                Throw("Only a single medium can be specified per endpoint (e.g. per emitter or sensor)");
            set_medium(medium);
            props.mark_queried(name);
        }
    }

    /* For some emitters, set_shape() will never be called, so we
     * make sure that m_shape is at least initialized. */
    ek::set_attr(this, "shape", m_shape);
}

MTS_VARIANT Endpoint<Float, Spectrum>::~Endpoint() { }

MTS_VARIANT void Endpoint<Float, Spectrum>::set_scene(const Scene *) {
}

MTS_VARIANT void Endpoint<Float, Spectrum>::set_shape(Shape * shape) {
    if (m_shape)
        Throw("An endpoint can be only be attached to a single shape.");

    m_shape = shape;
    ek::set_attr(this, "shape", m_shape);
}

MTS_VARIANT void Endpoint<Float, Spectrum>::set_medium(Medium *medium) {
    if (medium)
        Throw("An endpoint can be only be attached to a single medium.");

    m_medium = medium;
    ek::set_attr(this, "medium", m_medium);
}

MTS_VARIANT std::pair<typename Endpoint<Float, Spectrum>::Ray3f, Spectrum>
Endpoint<Float, Spectrum>::sample_ray(Float /*time*/,
                                      Float /*sample1*/,
                                      const Point2f & /*sample2*/,
                                      const Point2f & /*sample3*/,
                                      const Float & /*volume_sample*/,
                                      Mask /*active*/) const {
    NotImplementedError("sample_ray");
}

MTS_VARIANT std::pair<typename Endpoint<Float, Spectrum>::DirectionSample3f, Spectrum>
Endpoint<Float, Spectrum>::sample_direction(const Interaction3f & /*it*/,
                                            const Point2f & /*sample*/,
                                            const Float & /*volume_sample*/,
                                            Mask /*active*/) const {
    NotImplementedError("sample_direction");
}

MTS_VARIANT
std::pair<typename Endpoint<Float, Spectrum>::PositionSample3f, Float>
Endpoint<Float, Spectrum>::sample_position(Float /*time*/,
                                           const Point2f &/*sample*/,
                                           const Float & /*volume_sample*/,
                                           Mask /*active*/) const {
    NotImplementedError("sample_position");
}

MTS_VARIANT std::pair<typename Endpoint<Float, Spectrum>::Wavelength, Spectrum>
Endpoint<Float, Spectrum>::sample_wavelengths(const SurfaceInteraction3f & /*si*/,
                                              Float /*sample*/,
                                              Mask /*active*/) const {
    NotImplementedError("sample_wavelengths");
}

MTS_VARIANT Float Endpoint<Float, Spectrum>::pdf_direction(const Interaction3f & /*it*/,
                                                           const DirectionSample3f & /*ds*/,
                                                           Mask /*active*/) const {
    NotImplementedError("pdf_direction");
}

MTS_VARIANT Float Endpoint<Float, Spectrum>::pdf_position(
    const PositionSample3f & /*ps*/, Mask /*active*/) const {
    NotImplementedError("pdf_position");
}

MTS_VARIANT Spectrum Endpoint<Float, Spectrum>::pdf_wavelengths(
    const Spectrum & /*wavelengths*/, Mask /*active*/) const {
    NotImplementedError("pdf_wavelengths");
}

MTS_VARIANT Spectrum Endpoint<Float, Spectrum>::eval(const SurfaceInteraction3f & /*si*/,
                                                     Mask /*active*/) const {
    NotImplementedError("eval");
}

MTS_VARIANT void Endpoint<Float, Spectrum>::traverse(TraversalCallback *callback) {
    if (m_medium)
        callback->put_object("medium", m_medium.get());
}

MTS_IMPLEMENT_CLASS_VARIANT(Endpoint, Object)
MTS_INSTANTIATE_CLASS(Endpoint)
NAMESPACE_END(mitsuba)
