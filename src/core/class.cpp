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

/// Construct a key for use in the __classes hash table
inline std::string key(const std::string &name, const std::string &variant) {
    if (variant.empty())
        return name;
    else
        return name + "." + variant;
}

Class::Class(const std::string &name, const std::string &parent, const std::string &variant,
             ConstructFunctor constr, UnserializeFunctor unser, const std::string &alias)
    : m_name(name), m_parent_name(parent), m_variant(variant), m_alias(alias),
      m_parent(nullptr), m_construct(constr), m_unserialize(unser) {

    if (m_alias.empty())
        m_alias = name;

    if (!__classes)
        __classes = new std::map<std::string, Class *>();

    (*__classes)[key(name, variant)] = this;

    // Register classes that declare an alias for use in the XML parser
    if (!alias.empty())
        xml::detail::register_class(this);
}

const Class *Class::for_name(const std::string &name, const std::string &variant) {
    auto it = __classes->find(key(name, variant));
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
    std::string key_generic = class_->m_parent_name;
    if (key_generic.empty())
        return;

    if (!class_->variant().empty()) {
        std::string key_variant = key_generic + "." + class_->variant();
        auto it = __classes->find(key_variant);
        if (it != __classes->end()) {
            class_->m_parent = it->second;
            return;
        }
    }

    auto it = __classes->find(key_generic);
    if (it != __classes->end()) {
        class_->m_parent = it->second;
        return;
    }

    std::cerr << "Critical error during the static RTTI initialization: " << std::endl
              << "Could not locate the base class '" << key_generic << "' while initializing '"
              << class_->name() << "'";

    if (!class_->variant().empty())
        std::cerr << " with variant '" << class_->variant() << "'";

    std::cerr << "!" << std::endl;
}

ref<Object> Class::construct(const Properties &props) const {
    if (!m_construct)
        Throw("RTTI error: Attempted to construct a "
              "non-constructible class (%s)!", name());
    return m_construct(props);
}

ref<Object> Class::unserialize(Stream *stream) const {
    if (!m_unserialize)
        Throw("RTTI error: Attempted to construct a "
              "class lacking a unserialization constructor (%s)!", name());
    return m_unserialize(stream);
}

void Class::static_initialization() {
    for (auto &pair : *__classes)
        initialize_once(pair.second);
    m_is_initialized = true;
}

void Class::static_shutdown() {
    if (!m_is_initialized)
        return;
    for (auto &pair : *__classes) {
        delete pair.second;
    }
    delete __classes;
    __classes = nullptr;
    m_is_initialized = false;
    xml::detail::cleanup();
}

NAMESPACE_END(mitsuba)
