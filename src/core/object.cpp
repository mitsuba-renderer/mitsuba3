#include <mitsuba/core/object.h>
#include <mitsuba/core/properties.h>
#include <sstream>
#include <nanobind/intrusive/counter.inl>

NAMESPACE_BEGIN(mitsuba)

// =============================================================

std::vector<ref<Object>> Object::expand() const { return { }; }

void Object::traverse(TraversalCallback * /*callback*/) { }

void Object::parameters_changed(const std::vector<std::string> &/*keys*/) { }

std::string Object::to_string() const {
    std::ostringstream oss;
    oss << "Object[" << (void *) this << "]";
    return oss.str();
}

std::ostream& operator<<(std::ostream &os, const Object *object) {
    os << ((object != nullptr) ? object->to_string() : "nullptr");
    return os;
}

ObjectType Object::type() const {
    return ObjectType::Unknown;
}

// =============================================================

static const char *variant_type_name(ObjectType ot) {
    switch (ot) {
        case ObjectType::Scene: return "Scene";
        case ObjectType::Sensor: return "Sensor";
        case ObjectType::Film: return "Film";
        case ObjectType::Emitter: return "Emitter";
        case ObjectType::Sampler: return "Sampler";
        case ObjectType::Shape: return "Shape";
        case ObjectType::Texture: return "Texture";
        case ObjectType::Volume: return "Volume";
        case ObjectType::Medium: return "Medium";
        case ObjectType::BSDF: return "BSDF";
        case ObjectType::Integrator: return "Integrator";
        case ObjectType::PhaseFunction: return "Phase";
        case ObjectType::ReconstructionFilter: return "ReconstructionFilter";
        case ObjectType::Unknown: return "unknown";
    }
}


MI_VARIANT VariantObject<Float, Spectrum>::VariantObject() : m_id(nullptr) {
    // A variant object consists of: vtable ptr, reference count, id
    static_assert(sizeof(VariantObject) == 3*sizeof(void *), "VariantObject has an unexpected size!");

    if constexpr (dr::is_jit_v<Float>)
        jit_registry_put(Variant, variant_type_name(this->type()), this);
}

MI_VARIANT VariantObject<Float, Spectrum>::VariantObject(const Properties &props) : VariantObject() {
    std::string_view id = props.id();

    if (!id.empty())
        m_id = strdup(id.data());
}

MI_VARIANT VariantObject<Float, Spectrum>::VariantObject(VariantObject &&v) : VariantObject() {
    m_id = v.m_id;
    v.m_id = nullptr;
}

MI_VARIANT VariantObject<Float, Spectrum>::VariantObject(const VariantObject &v) : VariantObject() {
    if (v.m_id)
        m_id = strdup(v.m_id);
}

MI_VARIANT VariantObject<Float, Spectrum>::~VariantObject() {
    if constexpr (dr::is_jit_v<Float>)
        jit_registry_remove(this);

    free(m_id);
}

MI_VARIANT std::string_view VariantObject<Float, Spectrum>::variant() const {
    return Variant;
}

MI_VARIANT std::string_view VariantObject<Float, Spectrum>::id() const {
    return m_id ? m_id : "";
}

MI_INSTANTIATE_CLASS(VariantObject)
NAMESPACE_END(mitsuba)
