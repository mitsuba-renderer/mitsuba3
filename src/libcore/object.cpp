#include <mitsuba/core/object.h>
#include <cstdlib>
#include <cstdio>

NAMESPACE_BEGIN(mitsuba)

void Object::decRef(bool dealloc) const noexcept {
    --m_refCount;
    if (m_refCount == 0 && dealloc) {
        delete this;
    } else if (m_refCount < 0) {
        fprintf(stderr, "Internal error: Object reference count < 0!\n");
        abort();
    }
}

std::string Object::toString() const { 
    return "Object[unknown]";
}

Object::~Object() { }

#if 0
void Object::addChild(Object *) {
    throw InternalError(
        "Object::addChild() is not implemented for objects of type '%s'!",
        classTypeName(getClassType()));
}

void Object::activate() { /* Do nothing */ }
void Object::setParent(Object *) { /* Do nothing */ }

std::map<std::string, ObjectFactory::Constructor> *ObjectFactory::m_constructors = nullptr;

void ObjectFactory::registerClass(const std::string &name, const Constructor &constr) {
    if (!m_constructors)
        m_constructors = new std::map<std::string, ObjectFactory::Constructor>();
    (*m_constructors)[name] = constr;
}
#endif

NAMESPACE_END(mitsuba)
