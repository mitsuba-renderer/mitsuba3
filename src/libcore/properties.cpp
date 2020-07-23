#if defined(_MSC_VER)
#  pragma warning (disable: 4324) // warning C4324: 'std::pair<const std::string,mitsuba::Entry>': structure was padded due to alignment specifier
#  define _ENABLE_EXTENDED_ALIGNED_STORAGE
#endif

#include <cstdlib>
#include <iostream>
#include <map>
#include <sstream>

#include <mitsuba/core/logger.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/variant.h>

NAMESPACE_BEGIN(mitsuba)

using Float       = typename Properties::Float;
using Color3f     = typename Properties::Color3f;
using Array3f     = typename Properties::Array3f;
using Transform4f = typename Properties::Transform4f;

using VariantType = variant<
    bool,
    int64_t,
    Float,
    Array3f,
    std::string,
    Transform4f,
    Color3f,
    NamedReference,
    ref<Object>,
    const void *
>;

struct alignas(16) Entry {
    VariantType data;
    bool queried;
};

struct SortKey {
    bool operator()(const std::string &a, const std::string &b) const {
        size_t i = 0;
        while (i < a.size() && i < b.size() && a[i] == b[i])
            ++i;

        while (i > 0 && std::isdigit(a[i-1]))
            --i;

        const char *a_ptr = a.c_str() + i;
        const char *b_ptr = b.c_str() + i;

        if (std::isdigit(*a_ptr) && std::isdigit(*b_ptr)) {
            char *a_end, *b_end;
            long l1 = std::strtol(a_ptr, &a_end, 10);
            long l2 = std::strtol(b_ptr, &b_end, 10);
            if (a_end == (a.c_str() + a.size()) &&
                b_end == (b.c_str() + b.size()))
                return l1 < l2;
        }

        return std::strcmp(a_ptr, b_ptr) < 0;
    }
};

struct Properties::PropertiesPrivate {
    std::map<std::string, Entry, SortKey> entries;
    std::string id, plugin_name;
};

#define DEFINE_PROPERTY_ACCESSOR(Type, TagName, SetterName, GetterName) \
    void Properties::SetterName(const std::string &name, Type const &value, bool error_duplicates) { \
        if (has_property(name) && error_duplicates) \
            Log(Error, "Property \"%s\" was specified multiple times!", name); \
        d->entries[name].data = (Type) value; \
        d->entries[name].queried = false; \
    } \
    \
    Type const & Properties::GetterName(const std::string &name) const { \
        const auto it = d->entries.find(name); \
        if (it == d->entries.end()) \
            Throw("Property \"%s\" has not been specified!", name); \
        if (!it->second.data.is<Type>()) \
            Throw("The property \"%s\" has the wrong type (expected <" #TagName ">).", name); \
        it->second.queried = true; \
        return (Type const &) it->second.data; \
    } \
    \
    Type const & Properties::GetterName(const std::string &name, Type const &def_val) const { \
        const auto it = d->entries.find(name); \
        if (it == d->entries.end()) \
            return def_val; \
        if (!it->second.data.is<Type>()) \
            Throw("The property \"%s\" has the wrong type (expected <" #TagName ">).", name); \
        it->second.queried = true; \
        return (Type const &) it->second.data; \
    }

DEFINE_PROPERTY_ACCESSOR(bool,              boolean,   set_bool,              bool_)
DEFINE_PROPERTY_ACCESSOR(int64_t,           integer,   set_long,              long_)
DEFINE_PROPERTY_ACCESSOR(std::string,       string,    set_string,            string)
DEFINE_PROPERTY_ACCESSOR(NamedReference,    ref,       set_named_reference,   named_reference)
DEFINE_PROPERTY_ACCESSOR(Transform4f,       transform, set_transform,         transform)
DEFINE_PROPERTY_ACCESSOR(Color3f,           color,     set_color,             color)
DEFINE_PROPERTY_ACCESSOR(ref<Object>,       object,    set_object,            object)
DEFINE_PROPERTY_ACCESSOR(const void *,      pointer,   set_pointer,           pointer)
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
        using Type = Properties::Type;
        Type operator()(const std::nullptr_t &) { throw std::runtime_error("Internal error"); }
        Type operator()(const bool &) { return Type::Bool; }
        Type operator()(const int64_t &) { return Type::Long; }
        Type operator()(const Float &) { return Type::Float; }
        Type operator()(const Array3f &) { return Type::Array3f; }
        Type operator()(const std::string &) { return Type::String; }
        Type operator()(const Transform4f &) { return Type::Transform; }
        Type operator()(const Color3f &) { return Type::Color; }
        Type operator()(const NamedReference &) { return Type::NamedReference; }
        Type operator()(const ref<Object> &) { return Type::Object; }
        Type operator()(const void *&) { return Type::Pointer; }
    };

    struct StreamVisitor {
        std::ostream &os;
        StreamVisitor(std::ostream &os) : os(os) { }
        void operator()(const std::nullptr_t &) { throw std::runtime_error("Internal error"); }
        void operator()(const bool &b) { os << (b ? "true" : "false"); }
        void operator()(const int64_t &i) { os << i; }
        void operator()(const Float &f) { os << f; }
        void operator()(const Array3f &t) { os << t; }
        void operator()(const std::string &s) { os << "\"" << s << "\""; }
        void operator()(const Transform4f &t) { os << t; }
        void operator()(const Color3f &t) { os << t; }
        void operator()(const NamedReference &nr) { os << "\"" << (const std::string &) nr << "\""; }
        void operator()(const ref<Object> &o) { os << o->to_string(); }
        void operator()(const void *&p) { os << p; }
    };
}

Properties::Type Properties::type(const std::string &name) const {
    const auto it = d->entries.find(name);
    if (it == d->entries.end())
        Throw("type(): Could not find property named \"%s\"!", name);

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
        if (type != Type::NamedReference)
            continue;
        auto const &value = (const NamedReference &) e.second.data;
        result.push_back(std::make_pair(e.first, value));
        e.second.queried = true;
    }
    return result;
}

std::vector<std::pair<std::string, ref<Object>>> Properties::objects(bool mark_queried) const {
    std::vector<std::pair<std::string, ref<Object>>> result;
    result.reserve(d->entries.size());
    for (auto &e : d->entries) {
        auto type = e.second.data.visit(PropertyTypeVisitor());
        if (type != Type::Object)
            continue;
        result.push_back(std::make_pair(e.first, (const ref<Object> &) e.second));
        if (mark_queried)
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

// size_t getter
size_t Properties::size_(const std::string &name) const {
    const auto it = d->entries.find(name);
    if (it == d->entries.end())
        Throw("Property \"%s\" has not been specified!", name);
    if (!it->second.data.is<int64_t>())
        Throw("The property \"%s\" has the wrong type (expected <integer>).", name);

    auto v = (int64_t) it->second.data;
    if (v < 0) {
        Throw("Property \"%s\" has negative value %i, but was queried as a"
              " size_t (unsigned).", name, v);
    }
    it->second.queried = true;
    return (size_t) v;
}
// size_t getter (with default value)
size_t Properties::size_(const std::string &name, const size_t &def_val) const {
    const auto it = d->entries.find(name);
    if (it == d->entries.end())
        return def_val;

    auto v = (int64_t) it->second.data;
    if (v < 0) {
        Throw("Property \"%s\" has negative value %i, but was queried as a"
              " size_t (unsigned).", name, v);
    }
    it->second.queried = true;
    return (size_t) v;
}

/// Float setter
void Properties::set_float(const std::string &name, const Float &value, bool error_duplicates) {
    if (has_property(name) && error_duplicates)
        Log(Error, "Property \"%s\" was specified multiple times!", name);
    d->entries[name].data = (Float) value;
    d->entries[name].queried = false;
}

/// Float getter (without default)
Float Properties::float_(const std::string &name) const {
    const auto it = d->entries.find(name);
    if (it == d->entries.end())
        Throw("Property \"%s\" has not been specified!", name);
    if (!(it->second.data.is<Float>() || it->second.data.is<int64_t>()))
        Throw("The property \"%s\" has the wrong type (expected <float>).", name);
    it->second.queried = true;
    if (it->second.data.is<int64_t>())
        return (int64_t) it->second.data;
    return (Float) it->second.data;
}

/// Float getter (with default)
Float Properties::float_(const std::string &name, const Float &def_val) const {
    const auto it = d->entries.find(name);
    if (it == d->entries.end())
        return def_val;
    if (!(it->second.data.is<Float>() || it->second.data.is<int64_t>()))
        Throw("The property \"%s\" has the wrong type (expected <float>).", name);
    it->second.queried = true;
    if (it->second.data.is<int64_t>())
        return (int64_t) it->second.data;
    return (Float) it->second.data;
}

/// Array3f setter
void Properties::set_array3f(const std::string &name, const Array3f &value, bool error_duplicates) {
    if (has_property(name) && error_duplicates)
        Log(Error, "Property \"%s\" was specified multiple times!", name);
    d->entries[name].data = (Array3f) value;
    d->entries[name].queried = false;
}

/// Array3f getter (without default)
Array3f Properties::array3f(const std::string &name) const {
    const auto it = d->entries.find(name);
    if (it == d->entries.end())
        Throw("Property \"%s\" has not been specified!", name);
    if (!it->second.data.is<Array3f>())
        Throw("The property \"%s\" has the wrong type (expected <vector> or <point>).", name);
    it->second.queried = true;
    return it->second.data.operator Array3f&();
}

/// Array3f getter (with default)
Array3f Properties::array3f(const std::string &name, const Array3f &def_val) const {
    const auto it = d->entries.find(name);
    if (it == d->entries.end())
        return def_val;
    if (!it->second.data.is<Array3f>())
        Throw("The property \"%s\" has the wrong type (expected <vector> or <point>).", name);
    it->second.queried = true;
    return it->second.data.operator Array3f&();
}

/// AnimatedTransform setter.
void Properties::set_animated_transform(const std::string &name,
                                        ref<AnimatedTransform> value,
                                        bool error_duplicates) {
    if (has_property(name) && error_duplicates)
        Log(Error, "Property \"%s\" was specified multiple times!", name);
    d->entries[name].data = ref<Object>(value.get());
    d->entries[name].queried = false;
}

/// AnimatedTransform setter (from a simple Transform).
void Properties::set_animated_transform(const std::string &name,
                                        const Transform4f &value,
                                        bool error_duplicates) {
    ref<AnimatedTransform> trafo(new AnimatedTransform(value));
    return set_animated_transform(name, trafo, error_duplicates);
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
    if (!it->second.data.is<ref<Object>>()) {
        Throw("The property \"%s\" has the wrong type (expected "
              " <animated_transform> or <transform>).", name);
    }
    ref<Object> o = it->second.data;
    if (!o->class_()->derives_from(MTS_CLASS(AnimatedTransform)))
        Throw("The property \"%s\" has the wrong type (expected "
              " <animated_transform> or <transform>).", name);
    it->second.queried = true;
    return (AnimatedTransform *) o.get();
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
    if (!it->second.data.is<ref<Object>>()) {
        Throw("The property \"%s\" has the wrong type (expected "
              " <animated_transform> or <transform>).", name);
    }
    ref<Object> o = it->second.data;
    if (!o->class_()->derives_from(MTS_CLASS(AnimatedTransform)))
        Throw("The property \"%s\" has the wrong type (expected "
              " <animated_transform> or <transform>).", name);
    it->second.queried = true;
    return (AnimatedTransform *) o.get();
}

/// Retrieve an animated transformation (default value is a constant transform)
ref<AnimatedTransform> Properties::animated_transform(
        const std::string &name, const Transform4f &def_val) const {
    return animated_transform(name, new AnimatedTransform(def_val));
}

ref<Object> Properties::find_object(const std::string &name) const {
    const auto it = d->entries.find(name);
    if (it == d->entries.end())
        return ref<Object>();

    if (!it->second.data.is<ref<Object>>())
        Throw("The property \"%s\" has the wrong type.", name);

    return it->second.data;
}

NAMESPACE_END(mitsuba)
