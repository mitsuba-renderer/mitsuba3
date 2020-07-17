#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/endpoint.h>
#include <mitsuba/render/medium.h>

NAMESPACE_BEGIN(mitsuba)

MTS_VARIANT Endpoint<Float, Spectrum>::Endpoint(const Properties &props) : m_id(props.id()) {
    m_world_transform = props.animated_transform("to_world", ScalarTransform4f()).get();

    for (auto &[name, obj] : props.objects(false)) {
        Medium *medium = dynamic_cast<Medium *>(obj.get());
        if (medium) {
            if (m_medium)
                Throw("Only a single medium can be specified per endpoint (e.g. per emitter or sensor)");
            m_medium = medium;
            props.mark_queried(name);
        }
    }
}

MTS_VARIANT Endpoint<Float, Spectrum>::~Endpoint() { }

MTS_VARIANT void Endpoint<Float, Spectrum>::set_scene(const Scene *) {
}

MTS_VARIANT void Endpoint<Float, Spectrum>::set_shape(Shape * shape) {
    if (m_shape)
        Throw("An endpoint can be only be attached to a single shape.");

    m_shape = shape;
}

MTS_VARIANT void Endpoint<Float, Spectrum>::set_medium(Medium *medium) {
    if (medium)
        Throw("An endpoint can be only be attached to a single medium.");

    m_medium = medium;
}

MTS_VARIANT std::pair<typename Endpoint<Float, Spectrum>::Ray3f, Spectrum>
Endpoint<Float, Spectrum>::sample_ray(Float /*time*/, Float /*sample1*/,
                                      const Point2f & /*sample2*/,
                                      const Point2f & /*sample3*/,
                                      Mask /*active*/) const {
    NotImplementedError("sample_ray");
}

MTS_VARIANT std::pair<typename Endpoint<Float, Spectrum>::DirectionSample3f, Spectrum>
Endpoint<Float, Spectrum>::sample_direction(const Interaction3f & /*it*/,
                                            const Point2f & /*sample*/,
                                            Mask /*active*/) const {
    NotImplementedError("sample_direction");
}

MTS_VARIANT Float Endpoint<Float, Spectrum>::pdf_direction(const Interaction3f & /*it*/,
                                                           const DirectionSample3f & /*ds*/,
                                                           Mask /*active*/) const {
    NotImplementedError("pdf_direction");
}

MTS_VARIANT Spectrum Endpoint<Float, Spectrum>::eval(const SurfaceInteraction3f & /*si*/,
                                                     Mask /*active*/) const {
    NotImplementedError("eval");
}

MTS_VARIANT std::string Endpoint<Float, Spectrum>::id() const { return m_id; }

MTS_IMPLEMENT_CLASS_VARIANT(Endpoint, Object)
MTS_INSTANTIATE_CLASS(Endpoint)
NAMESPACE_END(mitsuba)
