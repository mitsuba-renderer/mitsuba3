#include <mitsuba/core/logger.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/variant.h>
#include <mitsuba/core/vector.h>
#include <iostream>

#include <map>
#include <sstream>

NAMESPACE_BEGIN(mitsuba)

using VariantType = variant<
    bool,
    int64_t,
    Float,
    Vector3f,
    std::string,
    ref<Object>
>;

struct Entry {
    VariantType data;
    bool queried;
};

struct Properties::PropertiesPrivate {
    std::map<std::string, Entry> entries;
    std::string id, pluginName;
};

#define DEFINE_PROPERTY_ACCESSOR(Type, TypeName, ReadableName) \
    void Properties::set##TypeName(const std::string &name, const Type &value, bool warnDuplicates) { \
        if (hasProperty(name) && warnDuplicates) \
            Log(EWarn, "Property \"%s\" was specified multiple times!", name.c_str()); \
        d->entries[name].data = (Type) value; \
        d->entries[name].queried = false; \
    } \
    \
    const Type& Properties::get##TypeName(const std::string &name) const { \
        const auto it = d->entries.find(name); \
        if (it == d->entries.end()) \
            Log(EError, "Property \"%s\" has not been specified!", name.c_str()); \
        if (!it->second.data.is<Type>()) \
            Log(EError, "The property \"%s\" has the wrong type (expected <" #ReadableName ">).", name.c_str()); \
        it->second.queried = true; \
        return (const Type &) it->second.data; \
    } \
    \
    const Type &Properties::get##TypeName(const std::string &name, const Type &defVal) const { \
        const auto it = d->entries.find(name); \
        if (it == d->entries.end()) \
            return defVal; \
        if (!it->second.data.is<Type>()) \
            Log(EError, "The property \"%s\" has the wrong type (expected <" #ReadableName ">).", name.c_str()); \
        it->second.queried = true; \
        return (const Type &) it->second.data; \
    }

DEFINE_PROPERTY_ACCESSOR(bool, Boolean, boolean)
DEFINE_PROPERTY_ACCESSOR(int64_t, Long, integer)
DEFINE_PROPERTY_ACCESSOR(Float, Float, float)
DEFINE_PROPERTY_ACCESSOR(std::string, String, string)
DEFINE_PROPERTY_ACCESSOR(Vector3f, Vector3f, vector)
DEFINE_PROPERTY_ACCESSOR(ref<Object>, Object, object)

Properties::Properties()
    : d(new PropertiesPrivate()) { }

Properties::Properties(const std::string &pluginName)
    : d(new PropertiesPrivate()) {
    d->pluginName = pluginName;
}

Properties::Properties(const Properties &props)
    : d(new PropertiesPrivate(*props.d)) { }

Properties::~Properties() { }

void Properties::operator=(const Properties &props) {
    (*d) = *props.d;
}

bool Properties::hasProperty(const std::string &name) const {
    return d->entries.find(name) != d->entries.end();
}

namespace {
    struct PropertyTypeVisitor {
        typedef Properties::EPropertyType Result;
        Result operator()(const std::nullptr_t &) { throw std::runtime_error("Internal error"); }
        Result operator()(const int64_t &) { return Properties::EInteger; }
        Result operator()(const bool &) { return Properties::EBoolean; }
        Result operator()(const Float &) { return Properties::EFloat; }
        Result operator()(const Vector3f &) { return Properties::EVector3f; }
        Result operator()(const std::string &) { return Properties::EString; }
        Result operator()(const ref<Object> &) { return Properties::EObject; }
    };

    struct StreamVisitor {
        std::ostream &os;
        StreamVisitor(std::ostream &os) : os(os) { }
        void operator()(const std::nullptr_t &) { throw std::runtime_error("Internal error"); }
        void operator()(const int64_t &i) { os << i; }
        void operator()(const bool &b) { os << b; }
        void operator()(const Float &f) { os << f; }
        void operator()(const Vector3f &v) { os << v; }
        void operator()(const std::string &s) { os << "\"" << s << "\""; }
        void operator()(const ref<Object> &o) { os << o->toString(); }
    };
}

Properties::EPropertyType Properties::getPropertyType(const std::string &name) const {
    const auto it = d->entries.find(name);
    if (it == d->entries.end())
        Log(EError, "getPropertyType(): Could not find property named \"%s\"!", name.c_str());

    return it->second.data.visit(PropertyTypeVisitor());
}

bool Properties::markQueried(const std::string &name) const {
    auto it = d->entries.find(name);
    if (it == d->entries.end())
        return false;
    it->second.queried = true;
    return true;
}

bool Properties::wasQueried(const std::string &name) const {
    const auto it = d->entries.find(name);
    if (it == d->entries.end())
        Log(EError, "Could not find property named \"%s\"!", name.c_str());
    return it->second.queried;
}

bool Properties::removeProperty(const std::string &name) {
    const auto it = d->entries.find(name);
    if (it == d->entries.end())
        return false;
    d->entries.erase(it);
    return true;
}

const std::string &Properties::getPluginName() const {
    return d->pluginName;
}

void Properties::setPluginName(const std::string &name) {
    d->pluginName = name;
}

const std::string &Properties::getID() const {
    return d->id;
}

void Properties::setID(const std::string &id) {
    d->id = id;
}

void Properties::copyAttribute(const Properties &properties,
                               const std::string &sourceName, const std::string &targetName) {
    const auto it = properties.d->entries.find(sourceName);
    if (it == properties.d->entries.end())
        Log(EError, "copyAttribute(): Could not find parameter \"%s\"!", sourceName.c_str());
    d->entries[targetName] = it->second;
}

std::vector<std::string> Properties::getPropertyNames() const {
    std::vector<std::string> result;
    for (const auto &e : d->entries)
        result.push_back(e.first);
    return result;
}

std::vector<std::string> Properties::getUnqueried() const {
    std::vector<std::string> result;
    for (const auto &e : d->entries) {
        if (!e.second.queried)
            result.push_back(e.first);
    }
    return result;
}

void Properties::merge(const Properties &p) {
    for (const auto &e : p.d->entries)
        d->entries[e.first] = e.second;
}

bool Properties::operator==(const Properties &p) const {
    if (d->pluginName != p.d->pluginName ||
        d->id != p.d->id ||
        d->entries.size() != p.d->entries.size())
        return false;

    for (const auto &e : d->entries) {
        auto it = p.d->entries.find(e.first);
        if (it == p.d->entries.end())
            return false;
        if (e.second.data != it->second.data)
            return false;
    }

    return true;
}

std::ostream &operator<<(std::ostream &os, const Properties &p) {
    auto it = p.d->entries.begin();

    os << "Properties[" << std::endl
       << "  pluginName = \"" << (p.d->pluginName) << "\"," << std::endl
       << "  id = \"" << p.d->id << "\"," << std::endl
       << "  elements = {" << std::endl;
    while (it != p.d->entries.end()) {
        os << "    \"" << it->first << "\" -> ";
        it->second.data.visit(StreamVisitor(os));
        if (++it != p.d->entries.end()) os << ",";
        os << std::endl;
    }
    os << "  }" << std::endl
       << "]" << std::endl;

    return os;
}

NAMESPACE_END(mitsuba)
