#include <mitsuba/core/object.h>
#include <mitsuba/core/properties.h>
#include <sstream>
#include <nanobind/intrusive/counter.inl>

NAMESPACE_BEGIN(mitsuba)

std::vector<ref<Object>> Object::expand() const { return { }; }

void Object::traverse(TraversalCallback * /*callback*/) { }

void Object::parameters_changed(const std::vector<std::string> &/*keys*/) { }

std::string Object::to_string() const {
    std::ostringstream oss;
    oss << class_name() << "[" << (void *) this << "]";
    return oss.str();
}

std::ostream& operator<<(std::ostream &os, const Object *object) {
    os << ((object != nullptr) ? object->to_string() : "nullptr");
    return os;
}

ObjectType Object::type() const {
    return ObjectType::Unknown;
}

std::string_view Object::variant_name() const {
    return "";
}

std::string_view Object::id() const {
    return "";
}

void Object::set_id(std::string_view /*id*/) {
    // The base Object class does not hold a unique ID - do nothing.
}

std::string_view Object::class_name() const {
    return "Object";
}
NAMESPACE_END(mitsuba)
