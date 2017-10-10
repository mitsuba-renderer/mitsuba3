#include <mitsuba/core/bbox.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/shape.h>

NAMESPACE_BEGIN(mitsuba)

Shape::Shape(const Properties &props) {
    for (auto &kv : props.objects()) {
        auto emitter = dynamic_cast<Emitter *>(kv.second.get());

        if (!emitter)
            Throw("Tried to add an unsupported object of type %s", kv.second);
        if (m_emitter) {
            Log(EError, "Only a single Emitter child object can be specified"
                        " per shape.");
        }

        m_emitter = emitter;
        emitter->set_shape(this);
    }

}

Shape::~Shape() { }

BoundingBox3f Shape::bbox(Index) const {
    return bbox();
}

BoundingBox3f Shape::bbox(Index index, const BoundingBox3f &clip) const {
    BoundingBox3f result = bbox(index);
    result.clip(clip);
    return result;
}

Shape::Size Shape::primitive_count() const {
    return 1;
}

void Shape::adjust_time(SurfaceInteraction3f &si, const Float &time) const {
    si.time = time;
}

void Shape::adjust_time(SurfaceInteraction3fP &si, const FloatP &time,
                        const mask_t<FloatP> &active) const {
    si.time[active] = time;
}

MTS_IMPLEMENT_CLASS(Shape, Object)
NAMESPACE_END(mitsuba)
