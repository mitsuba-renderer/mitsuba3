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

Class::Class(const std::string &name, const std::string &parent, const std::string &variant,
             ConstructFunctor constr, UnserializeFunctor unser, const std::string &alias)
    : m_name(name), m_parent_name(parent), m_variant(variant), m_alias(alias), m_constr(constr),
      m_unser(unser) {

    if (m_alias.empty())
        m_alias = name;

    if (!__classes)
        __classes = new std::map<std::string, Class *>();

    (*__classes)[construct_key(name, variant)] = this;
    if (!m_alias.empty())
        (*__classes)[construct_key(m_alias, variant)] = this;

    // Register classes that declare an alias for use in the XML parser
    if (!alias.empty())
        xml::detail::register_class(this);
}

const Class *Class::for_name(const std::string &name, const std::string &variant) {
    const std::string &key = construct_key(name, variant);
    auto it = __classes->find(key);
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
    const std::string &key_base = construct_key(class_->m_parent_name, class_->variant());

    if (!key_base.empty()) {
        if (__classes->find(key_base) != __classes->end()) {
            class_->m_parent = (*__classes)[key_base];
        } else {
            std::cerr << "Critical error during the static RTTI initialization: " << std::endl
                << "Could not locate the base class '" << key_base << "' while initializing '"
                << class_->name() << "' (" << class_->variant() << ")" << "'!" << std::endl;
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

std::string Class::construct_key(const std::string &name, const std::string &variant)
{
    if (variant.empty() || name == "Object")
        return name;
    else
        return name + "." + variant;
}

NAMESPACE_END(mitsuba)
