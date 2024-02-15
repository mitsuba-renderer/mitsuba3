#include <mitsuba/core/object.h>
#include <cstdlib>
#include <cstdio>
#include <sstream>
#include <nanobind/intrusive/counter.inl>

NAMESPACE_BEGIN(mitsuba)

std::vector<ref<Object>> Object::expand() const { return { }; }

void Object::traverse(TraversalCallback * /*callback*/) { }

void Object::parameters_changed(const std::vector<std::string> &/*keys*/) { }

std::string Object::id() const { return std::string(); }

void Object::set_id(const std::string&/*id*/) { }

std::string Object::to_string() const {
    std::ostringstream oss;
    oss << class_()->name() << "[" << (void *) this << "]";
    return oss.str();
}

std::ostream& operator<<(std::ostream &os, const Object *object) {
    os << ((object != nullptr) ? object->to_string() : "nullptr");
    return os;
}

MI_IMPLEMENT_CLASS(Object,)
NAMESPACE_END(mitsuba)
