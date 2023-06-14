#include <mitsuba/core/object.h>
#include <cstdlib>
#include <cstdio>
#include <sstream>

NAMESPACE_BEGIN(mitsuba)

void Object::dec_ref(bool dealloc) const noexcept {
    uint32_t ref_count = m_ref_count.fetch_sub(1);

    // If the Object has a Python object attached to it, we need to clean up
    // when the reference counter is decreased below 2. This accounts for the
    // fact that there is an internal reference by the Python object itself. The
    // cleanup callback then releases the Python object, which recursively
    // de-allocates the Mitsuba Object instance.
    if (ref_count == 2 && m_py_cleanup_callback) {
        m_py_cleanup_callback();
        return;
    }
    if (ref_count <= 0) {
        fprintf(stderr, "Internal error: Object reference count < 0!\n");
        abort();
    } else if (ref_count == 1 && dealloc) {
        delete this;
    }
}

std::vector<ref<Object>> Object::expand() const {
    return { };
}

void Object::traverse(TraversalCallback * /*callback*/) { }

void Object::parameters_changed(const std::vector<std::string> &/*keys*/) { }

std::string Object::id() const { return std::string(); }

void Object::set_id(const std::string&/*id*/) { }

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

MI_IMPLEMENT_CLASS(Object,)
NAMESPACE_END(mitsuba)
