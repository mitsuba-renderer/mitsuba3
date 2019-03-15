#include <mitsuba/core/class.h>
#include <mitsuba/core/logger.h>
#include <map>
#include <iostream>

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(xml)
NAMESPACE_BEGIN(detail)

void register_class(const Class *class_);
void cleanup();

NAMESPACE_END(detail)
NAMESPACE_END(xml)

static std::map<std::string, Class *> *__classes;
bool Class::m_is_initialized = false;
const Class *m_class = nullptr;

Class::Class(const std::string &name, const std::string &parent,
             bool abstract, ConstructFunctor constr,
             UnserializeFunctor unser)
    : Class(name, name, parent, abstract, constr, unser) { }

Class::Class(const std::string &name, const std::string &alias,
             const std::string &parent, bool abstract,
             ConstructFunctor constr, UnserializeFunctor unser)
    : m_name(name), m_alias(alias), m_parent_name(parent),
      m_abstract(abstract), m_constr(constr), m_unser(unser) {

    if (!__classes)
        __classes = new std::map<std::string, Class *>();

    (*__classes)[name] = this;

    // Also register new abstract classes with the XML parser
    if (abstract ||
        // Special cases
        name == "Scene" || name == "ContinuousSpectrum" || name == "Texture3D")
        xml::detail::register_class(this);
}

const Class *Class::for_name(const std::string &name) {
    auto it = __classes->find(name);
    if (it != __classes->end())
        return it->second;

    return nullptr;
}

bool Class::derives_from(const Class *arg) const {
    const Class *class_ = this;

    while (class_) {
        if (arg == class_)
            return true;
        class_ = class_->parent();
    }

    return false;
}

void Class::initialize_once(Class *class_) {
    const std::string &base = class_->m_parent_name;

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

void Class::static_initialization() {
    for (auto &pair : *__classes)
        initialize_once(pair.second);
    m_is_initialized = true;
}

void Class::static_shutdown() {
    for (auto &pair : *__classes)
        delete pair.second;
    delete __classes;
    __classes = nullptr;
    m_is_initialized = false;
    xml::detail::cleanup();
}

NAMESPACE_END(mitsuba)
