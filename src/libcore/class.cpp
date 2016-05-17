#include <mitsuba/core/class.h>
#include <mitsuba/core/logger.h>
#include <map>
#include <iostream>

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(xml)
void registerClass(const Class *class_);
NAMESPACE_END(xml)

static std::map<std::string, Class *> *__classes;
bool Class::m_isInitialized = false;
const Class *m_theClass = nullptr;

Class::Class(const std::string &name, const std::string &parent, bool abstract,
             ConstructFunctor constr, UnserializeFunctor unser)
    : m_name(name), m_parentName(parent), m_abstract(abstract),
      m_constr(constr), m_unser(unser) {

    if (!__classes)
        __classes = new std::map<std::string, Class *>();

    (*__classes)[name] = this;

    /* Also register new abstract classes with the XML parser */
    if (abstract)
        xml::registerClass(this);
}

const Class *Class::forName(const std::string &name) {
    auto it = __classes->find(name);
    if (it != __classes->end())
        return it->second;

    return nullptr;
}

bool Class::derivesFrom(const Class *theClass) const {
    const Class *mClass = this;

    while (mClass) {
        if (theClass == mClass)
            return true;
        mClass = mClass->getParent();
    }

    return false;
}

void Class::initializeOnce(Class *theClass) {
    const std::string &base = theClass->m_parentName;

    if (!base.empty()) {
        if (__classes->find(base) != __classes->end()) {
            theClass->m_parent = (*__classes)[base];
        } else {
            std::cerr << "Critical error during the static RTTI initialization: " << std::endl
                << "Could not locate the base class '" << base << "' while initializing '"
                << theClass->getName() << "'!" << std::endl;
            abort();
        }
    }
}

Object *Class::construct() const {
    if (!m_constr) {
        Log(EError, "RTTI error: Attempted to construct a "
            "class lacking a default constructor (%s)!",
            getName());
    }
    return m_constr();
}

Object *Class::unserialize(Stream *stream) const {
    if (!m_unser) {
        Log(EError, "RTTI error: Attempted to construct a "
            "class lacking a unserialization constructor (%s)!",
            getName().c_str());
    }
    return m_unser(stream);
}

void Class::staticInitialization() {
    for (auto &pair : *__classes)
        initializeOnce(pair.second);
    m_isInitialized = true;
}

void Class::staticShutdown() {
    for (auto &pair : *__classes)
        delete pair.second;
    delete __classes;
    __classes = nullptr;
    m_isInitialized = false;
}

NAMESPACE_END(mitsuba)
