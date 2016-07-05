#include <mitsuba/core/class.h>
#include <mitsuba/core/logger.h>
#include <map>
#include <iostream>

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(xml)
void registerClass(const Class *class_);
void cleanup();
NAMESPACE_END(xml)

static std::map<std::string, Class *> *__classes;
bool Class::m_isInitialized = false;
const Class *m_class = nullptr;

Class::Class(const std::string &name, const std::string &parent, bool abstract,
             ConstructFunctor constr, UnserializeFunctor unser)
    : m_name(name), m_parentName(parent), m_abstract(abstract),
      m_constr(constr), m_unser(unser) {

    if (!__classes)
        __classes = new std::map<std::string, Class *>();

    (*__classes)[name] = this;

    /* Also register new abstract classes with the XML parser */
    if (abstract || name == "Scene" /* Special case for 'Scene' */)
        xml::registerClass(this);
}

const Class *Class::forName(const std::string &name) {
    auto it = __classes->find(name);
    if (it != __classes->end())
        return it->second;

    return nullptr;
}

bool Class::derivesFrom(const Class *class_) const {
    const Class *mClass = this;

    while (mClass) {
        if (class_ == mClass)
            return true;
        mClass = mClass->parent();
    }

    return false;
}

void Class::initializeOnce(Class *class_) {
    const std::string &base = class_->m_parentName;

    if (!base.empty()) {
        if (__classes->find(base) != __classes->end()) {
            class_->m_parent = (*__classes)[base];
        } else {
            std::cerr << "Critical error during the static RTTI initialization: " << std::endl
                << "Could not locate the base class '" << base << "' while initializing '"
                << class_->name() << "'!" << std::endl;
            abort();
        }
    }
}

ref<Object> Class::construct(const Properties &props) const {
    if (!m_constr)
        Throw("RTTI error: Attempted to construct a "
              "non-constructible class (%s)!", name());
    return m_constr(props);
}

ref<Object> Class::unserialize(Stream *stream) const {
    if (!m_unser)
        Throw("RTTI error: Attempted to construct a "
              "class lacking a unserialization constructor (%s)!", name());
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
    xml::cleanup();
}

NAMESPACE_END(mitsuba)
