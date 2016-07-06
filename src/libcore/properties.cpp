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
    NamedReference,
    ref<Object>
>;

struct Entry {
    VariantType data;
    bool queried;
};

struct SortKey {
    bool operator()(std::string a, std::string b) const {
        size_t i = 0;
        while (i < a.size() && i < b.size() && a[i] == b[i])
            ++i;
        a = a.substr(i);
        b = b.substr(i);
        try {
            return std::stoi(a) < std::stoi(b);
        } catch (const std::logic_error &) {
            return a < b;
        }
    }
};

struct Properties::PropertiesPrivate {
    std::map<std::string, Entry, SortKey> entries;
    std::string id, pluginName;
};

#define DEFINE_PROPERTY_ACCESSOR(Type, TagName, SetterName, GetterName) \
    void Properties::SetterName(const std::string &name, const Type &value, bool warnDuplicates) { \
        if (hasProperty(name) && warnDuplicates) \
            Log(EWarn, "Property \"%s\" was specified multiple times!", name); \
        d->entries[name].data = (Type) value; \
        d->entries[name].queried = false; \
    } \
    \
    const Type& Properties::GetterName(const std::string &name) const { \
        const auto it = d->entries.find(name); \
        if (it == d->entries.end()) \
            Throw("Property \"%s\" has not been specified!", name); \
        if (!it->second.data.is<Type>()) \
            Throw("The property \"%s\" has the wrong type (expected <" #TagName ">).", name); \
        it->second.queried = true; \
        return (const Type &) it->second.data; \
    } \
    \
    const Type &Properties::GetterName(const std::string &name, const Type &defVal) const { \
        const auto it = d->entries.find(name); \
        if (it == d->entries.end()) \
            return defVal; \
        if (!it->second.data.is<Type>()) \
            Throw("The property \"%s\" has the wrong type (expected <" #TagName ">).", name); \
        it->second.queried = true; \
        return (const Type &) it->second.data; \
    }

DEFINE_PROPERTY_ACCESSOR(bool,           boolean,   setBool,            bool_)
DEFINE_PROPERTY_ACCESSOR(int64_t,        integer,   setLong,            long_)
DEFINE_PROPERTY_ACCESSOR(Float,          float,     setFloat,           float_)
DEFINE_PROPERTY_ACCESSOR(std::string,    string,    setString,          string)
DEFINE_PROPERTY_ACCESSOR(Vector3f,       vector,    setVector3f,        vector3f)
DEFINE_PROPERTY_ACCESSOR(Point3f,        point,     setPoint3f,         point3f)
DEFINE_PROPERTY_ACCESSOR(NamedReference, ref,       setNamedReference,  namedReference)
DEFINE_PROPERTY_ACCESSOR(ref<Object>,    object,    setObject,          object)

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
        Result operator()(const int64_t &) { return Properties::ELong; }
        Result operator()(const bool &) { return Properties::EBool; }
        Result operator()(const Float &) { return Properties::EFloat; }
        Result operator()(const Vector3f &) { return Properties::EVector3f; }
        Result operator()(const Point3f &) { return Properties::EPoint3f; }
        Result operator()(const std::string &) { return Properties::EString; }
        Result operator()(const NamedReference &) { return Properties::ENamedReference; }
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
        void operator()(const Point3f &v) { os << v; }
        void operator()(const std::string &s) { os << "\"" << s << "\""; }
        void operator()(const NamedReference &nr) { os << "\"" << (const std::string &) nr << "\""; }
        void operator()(const ref<Object> &o) { os << o->toString(); }
    };
}

Properties::EPropertyType Properties::propertyType(const std::string &name) const {
    const auto it = d->entries.find(name);
    if (it == d->entries.end())
        Throw("propertyType(): Could not find property named \"%s\"!", name);

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
        Throw("Could not find property named \"%s\"!", name);
    return it->second.queried;
}

bool Properties::removeProperty(const std::string &name) {
    const auto it = d->entries.find(name);
    if (it == d->entries.end())
        return false;
    d->entries.erase(it);
    return true;
}

const std::string &Properties::pluginName() const {
    return d->pluginName;
}

void Properties::setPluginName(const std::string &name) {
    d->pluginName = name;
}

const std::string &Properties::id() const {
    return d->id;
}

void Properties::setID(const std::string &id) {
    d->id = id;
}

void Properties::copyAttribute(const Properties &properties,
                               const std::string &sourceName,
                               const std::string &targetName) {
    const auto it = properties.d->entries.find(sourceName);
    if (it == properties.d->entries.end())
        Throw("copyAttribute(): Could not find parameter \"%s\"!", sourceName);
    d->entries[targetName] = it->second;
}

std::vector<std::string> Properties::propertyNames() const {
    std::vector<std::string> result;
    for (const auto &e : d->entries)
        result.push_back(e.first);
    return result;
}

std::vector<std::pair<std::string, NamedReference>> Properties::namedReferences() const {
    std::vector<std::pair<std::string, NamedReference>> result;
    result.reserve(d->entries.size());
    for (auto &e : d->entries) {
        auto type = e.second.data.visit(PropertyTypeVisitor());
        if (type != ENamedReference)
            continue;
        auto const &value = (const NamedReference &) e.second.data;
        result.push_back(std::make_pair(e.first, value));
        e.second.queried = true;
    }
    return result;
}

std::vector<std::pair<std::string, ref<Object>>> Properties::objects() const {
    std::vector<std::pair<std::string, ref<Object>>> result;
    result.reserve(d->entries.size());
    for (auto &e : d->entries) {
        auto type = e.second.data.visit(PropertyTypeVisitor());
        if (type != EObject)
            continue;
        result.push_back(std::make_pair(e.first, (const ref<Object> &) e.second));
        e.second.queried = true;
    }
    return result;
}

std::vector<std::string> Properties::unqueried() const {
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
