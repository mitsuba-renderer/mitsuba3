#include <mitsuba/render/emitter.h>

NAMESPACE_BEGIN(mitsuba)

Emitter::Emitter(const Properties &props)
    : m_properties(props), m_type(0) {
    set_world_transform(
        props.animated_transform("to_world", Transform4f()).get()
    );
}

Emitter::~Emitter() { }

/// Return the local space to world space transformation
const AnimatedTransform *Emitter::world_transform() const {
    return m_world_transform.get();
}

/// Set the local space to world space transformation
void Emitter::set_world_transform(const AnimatedTransform *trafo) {
    m_world_transform = trafo;
    ref<AnimatedTransform> atrafo(new AnimatedTransform(*trafo));
    m_properties.set_animated_transform("to_world", atrafo, false);
}

MTS_IMPLEMENT_CLASS(Emitter, Object)
NAMESPACE_END(mitsuba)
