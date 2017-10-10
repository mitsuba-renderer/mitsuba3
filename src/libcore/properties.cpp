#if defined(_MSC_VER)
#  pragma warning (disable: 4324) // warning C4324: 'std::pair<const std::string,mitsuba::Entry>': structure was padded due to alignment specifier
#endif

#include <mitsuba/core/properties.h>

#include <mitsuba/core/logger.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/variant.h>
#include <iostream>

#include <map>
#include <sstream>

NAMESPACE_BEGIN(mitsuba)

using VariantType = variant<
    bool,
    int64_t,
    Float,
    Vector3f,
    Point3f,
    std::string,
    Transform4f,
    ref<AnimatedTransform>,
    Spectrumf,
    NamedReference,
    ref<Object>
>;

struct alignas(alignof(Transform4f)) Entry {
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
    using Alloc = enoki::aligned_allocator<std::pair<const std::string, Entry>>;
    std::map<std::string, Entry, SortKey, Alloc> entries;
    std::string id, plugin_name;
};

#define DEFINE_PROPERTY_ACCESSOR(Type, TagName, SetterName, GetterName) \
    void Properties::SetterName(const std::string &name, const Type &value, bool warn_duplicates) { \
        if (has_property(name) && warn_duplicates) \
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
    const Type &Properties::GetterName(const std::string &name, const Type &def_val) const { \
        const auto it = d->entries.find(name); \
        if (it == d->entries.end()) \
            return def_val; \
        if (!it->second.data.is<Type>()) \
            Throw("The property \"%s\" has the wrong type (expected <" #TagName ">).", name); \
        it->second.queried = true; \
        return (const Type &) it->second.data; \
    }

DEFINE_PROPERTY_ACCESSOR(bool,              boolean,   set_bool,              bool_)
DEFINE_PROPERTY_ACCESSOR(int64_t,           integer,   set_long,              long_)
DEFINE_PROPERTY_ACCESSOR(Float,             float,     set_float,             float_)
DEFINE_PROPERTY_ACCESSOR(std::string,       string,    set_string,            string)
DEFINE_PROPERTY_ACCESSOR(Vector3f,          vector,    set_vector3f,          vector3f)
DEFINE_PROPERTY_ACCESSOR(Point3f,           point,     set_point3f,           point3f)
DEFINE_PROPERTY_ACCESSOR(NamedReference,    ref,       set_named_reference,   named_reference)
DEFINE_PROPERTY_ACCESSOR(Transform4f,       transform, set_transform,         transform)
DEFINE_PROPERTY_ACCESSOR(Spectrumf,         spectrumf, set_spectrumf,         spectrumf)
DEFINE_PROPERTY_ACCESSOR(ref<Object>,       object,    set_object,            object)
// See at the end of the file for custom-defined accessors.

Properties::Properties()
    : d(new PropertiesPrivate()) { }

Properties::Properties(const std::string &plugin_name)
    : d(new PropertiesPrivate()) {
    d->plugin_name = plugin_name;
}

Properties::Properties(const Properties &props)
    : d(new PropertiesPrivate(*props.d)) { }

Properties::~Properties() { }

void Properties::operator=(const Properties &props) {
    (*d) = *props.d;
}

bool Properties::has_property(const std::string &name) const {
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
        Result operator()(const Transform4f &) { return Properties::ETransform; }
        Result operator()(const ref<AnimatedTransform> &) { return Properties::EAnimatedTransform; }
        Result operator()(const Spectrumf &) { return Properties::ESpectrum; }
        Result operator()(const ref<Object> &) { return Properties::EObject; }
    };

    struct StreamVisitor {
        std::ostream &os;
        StreamVisitor(std::ostream &os) : os(os) { }
        void operator()(const std::nullptr_t &) { throw std::runtime_error("Internal error"); }
        void operator()(const int64_t &i) { os << i; }
        void operator()(const bool &b) { os << (b ? "true" : "false"); }
        void operator()(const Float &f) { os << f; }
        void operator()(const Vector3f &v) { os << v; }
        void operator()(const Point3f &v) { os << v; }
        void operator()(const Transform4f &t) { os << t; }
        void operator()(const ref<AnimatedTransform> &t) { os << *t; }
        void operator()(const Spectrumf &t) { os << t; }
        void operator()(const std::string &s) { os << "\"" << s << "\""; }
        void operator()(const NamedReference &nr) { os << "\"" << (const std::string &) nr << "\""; }
        void operator()(const ref<Object> &o) { os << o->to_string(); }
    };
}

Properties::EPropertyType Properties::property_type(const std::string &name) const {
    const auto it = d->entries.find(name);
    if (it == d->entries.end())
        Throw("property_type(): Could not find property named \"%s\"!", name);

    return it->second.data.visit(PropertyTypeVisitor());
}

bool Properties::mark_queried(const std::string &name) const {
    auto it = d->entries.find(name);
    if (it == d->entries.end())
        return false;
    it->second.queried = true;
    return true;
}

bool Properties::was_queried(const std::string &name) const {
    const auto it = d->entries.find(name);
    if (it == d->entries.end())
        Throw("Could not find property named \"%s\"!", name);
    return it->second.queried;
}

bool Properties::remove_property(const std::string &name) {
    const auto it = d->entries.find(name);
    if (it == d->entries.end())
        return false;
    d->entries.erase(it);
    return true;
}

const std::string &Properties::plugin_name() const {
    return d->plugin_name;
}

void Properties::set_plugin_name(const std::string &name) {
    d->plugin_name = name;
}

const std::string &Properties::id() const {
    return d->id;
}

void Properties::set_id(const std::string &id) {
    d->id = id;
}

void Properties::copy_attribute(const Properties &properties,
                                const std::string &source_name,
                                const std::string &target_name) {
    const auto it = properties.d->entries.find(source_name);
    if (it == properties.d->entries.end())
        Throw("copy_attribute(): Could not find parameter \"%s\"!", source_name);
    d->entries[target_name] = it->second;
}

std::vector<std::string> Properties::property_names() const {
    std::vector<std::string> result;
    for (const auto &e : d->entries)
        result.push_back(e.first);
    return result;
}

std::vector<std::pair<std::string, NamedReference>> Properties::named_references() const {
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
    if (d->plugin_name != p.d->plugin_name ||
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

std::string Properties::as_string(const std::string &name) const {
    std::ostringstream oss;
    bool found = false;
    for (auto &e : d->entries) {
        if (e.first != name)
            continue;
        e.second.data.visit(StreamVisitor(oss));
        found = true;
        break;
    }
    if (!found)
        Throw("Property \"%s\" has not been specified!", name); \
    return oss.str();
}

std::string Properties::as_string(const std::string &name, const std::string &def_val) const {
    std::ostringstream oss;
    bool found = false;
    for (auto &e : d->entries) {
        if (e.first != name)
            continue;
        e.second.data.visit(StreamVisitor(oss));
        found = true;
        break;
    }
    if (!found)
        return def_val;
    return oss.str();
}

std::ostream &operator<<(std::ostream &os, const Properties &p) {
    auto it = p.d->entries.begin();

    os << "Properties[" << std::endl
       << "  plugin_name = \"" << (p.d->plugin_name) << "\"," << std::endl
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

// =============================================================================
// === Custom accessors
// =============================================================================

/// AnimatedTransform setter.
void Properties::set_animated_transform(const std::string &name,
                                        ref<AnimatedTransform> value,
                                        bool warn_duplicates) {
    if (has_property(name) && warn_duplicates)
        Log(EWarn, "Property \"%s\" was specified multiple times!", name);
    d->entries[name].data = value;
    d->entries[name].queried = false;
}

/// AnimatedTransform setter (from a simple Transform).
void Properties::set_animated_transform(const std::string &name,
                                        const Transform4f &value,
                                        bool warn_duplicates) {
    ref<AnimatedTransform> trafo(new AnimatedTransform(value));
    return set_animated_transform(name, trafo, warn_duplicates);
}

/// AnimatedTransform getter (without default value).
ref<AnimatedTransform> Properties::animated_transform(const std::string &name) const {
    const auto it = d->entries.find(name);
    if (it == d->entries.end())
        Throw("Property \"%s\" has not been specified!", name);
    if (it->second.data.is<Transform4f>()) {
        // Also accept simple transforms, from which we can build
        // an AnimatedTransform.
        it->second.queried = true;
        return new AnimatedTransform(
            static_cast<const Transform4f &>(it->second.data));
    }
    if (!it->second.data.is<ref<AnimatedTransform>>()) {
        Throw("The property \"%s\" has the wrong type (expected "
              " <animated_transform> or <transform>).", name);
    }
    it->second.queried = true;
    return (const ref<AnimatedTransform> &) it->second.data;
}

/// AnimatedTransform getter (with default value).
ref<AnimatedTransform> Properties::animated_transform(
        const std::string &name, ref<AnimatedTransform> def_val) const {
    const auto it = d->entries.find(name);
    if (it == d->entries.end())
        return def_val;
    if (it->second.data.is<Transform4f>()) {
        // Also accept simple transforms, from which we can build
        // an AnimatedTransform.
        it->second.queried = true;
        return new AnimatedTransform(
            static_cast<const Transform4f &>(it->second.data));
    }
    if (!it->second.data.is<ref<AnimatedTransform>>()) {
        Throw("The property \"%s\" has the wrong type (expected "
              " <animated_transform> or <transform>).", name);
    }
    it->second.queried = true;
    return (const ref<AnimatedTransform> &) it->second.data;
}

/// Retrieve an animated transformation (default value is a constant transform)
ref<AnimatedTransform> Properties::animated_transform(
        const std::string &name, const Transform4f &def_val) const {
    return animated_transform(name, new AnimatedTransform(def_val));
}

NAMESPACE_END(mitsuba)
