#include <mitsuba/core/object.h>
#include <cstdlib>
#include <cstdio>
#include <sstream>
#include <vector>

NAMESPACE_BEGIN(mitsuba)

void Object::dec_ref(bool dealloc) const noexcept {
    --m_ref_count;
    if (m_ref_count == 0 && dealloc) {
        delete this;
    } else if (m_ref_count < 0) {
        fprintf(stderr, "Internal error: Object reference count < 0!\n");
        abort();
    }
}

std::vector<ref<Object>> Object::expand() const {
    return { };
}

void Object::traverse(TraversalCallback * /*callback*/) { }

void Object::parameters_changed(const std::vector<std::string> &/*keys*/) { }

std::string Object::id() const { return std::string(); }

std::string Object::to_string() const {
    std::ostringstream oss;
    oss << class_()->name() << "[" << (void *) this << "]";
    return oss.str();
}

Object::~Object() { }

std::ostream& operator<<(std::ostream &os, const Object *object) {
    os << ((object != nullptr) ? object->to_string() : "nullptr");
    return os;
}

MTS_IMPLEMENT_CLASS(Object,)
NAMESPACE_END(mitsuba)
