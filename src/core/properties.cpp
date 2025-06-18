#include <iostream>
#include <variant>
#include <algorithm>
#include <tsl/robin_map.h>
#include <drjit/tensor.h>
#include <typeinfo>

#include <mitsuba/core/logger.h>
#include <mitsuba/core/object.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/render/texture.h>

#if defined(__GNUG__)
#  include <cxxabi.h>
#endif

NAMESPACE_BEGIN(mitsuba)

// Map property types to human-readable type names
std::string_view property_type_name(Properties::Type type) {
    switch (type) {
        case Properties::Type::Bool:      return "bool";
        case Properties::Type::Integer:   return "integer";
        case Properties::Type::Float:     return "float";
        case Properties::Type::Vector:    return "vector";
        case Properties::Type::Transform: return "transform";
        case Properties::Type::Color:     return "color";
        case Properties::Type::String:    return "string";
        case Properties::Type::Reference: return "reference";
        case Properties::Type::Object:    return "object";
        case Properties::Type::Any:       return "any";
    }
}

using Float         = double;
using Array3f       = dr::Array<Float, 3>;
using Color3f       = Color<Float, 3>;
using Transform4f   = Transform<Point<double, 4>>;
using Reference     = Properties::Reference;
using Type          = Properties::Type;

template <typename T> struct variant_type;
template<> struct variant_type<bool> { static constexpr auto value = Type::Bool; };
template<> struct variant_type<Float> { static constexpr auto value = Type::Float; };
template<> struct variant_type<int64_t> { static constexpr auto value = Type::Integer; };
template<> struct variant_type<std::string> { static constexpr auto value = Type::String; };
template<> struct variant_type<Array3f> { static constexpr auto value = Type::Vector; };
template<> struct variant_type<Color3f> { static constexpr auto value = Type::Color; };
template<> struct variant_type<Transform4f> { static constexpr auto value = Type::Transform; };
template<> struct variant_type<Reference> { static constexpr auto value = Type::Reference; };
template<> struct variant_type<ref<Object>> { static constexpr auto value = Type::Object; };
template<> struct variant_type<Any> { static constexpr auto value = Type::Any; };

using Variant = std::variant<bool, int64_t, Float, std::string, Array3f,
                             Color3f, Transform4f, Reference, ref<Object>, Any>;

struct Entry {
    Variant value;
    mutable uint32_t insertion_index : 31;
    mutable uint32_t queried : 1;

    Entry() : insertion_index(0), queried(0) { }

    template <typename T> Entry(T &&v, uint32_t idx = 0)
        : value(std::forward<T>(v)), insertion_index(idx), queried(0) { }
};

using EntryMap = tsl::robin_map<
    std::string,
    Entry,
    std::hash<std::string_view>,
    std::equal_to<>
>;

struct Properties::PropertiesPrivate {
    EntryMap entries;
    std::string plugin_name;
    std::string id;
    uint32_t insertion_index = 0;
};

Properties::Properties()
    : d(new PropertiesPrivate()) { }

Properties::Properties(std::string_view plugin_name)
    : d(new PropertiesPrivate()) {
    d->plugin_name = plugin_name;
}

Properties::Properties(const Properties &props)
    : d(new PropertiesPrivate(*props.d)) { }

Properties::Properties(Properties &&props)
    : d(std::move(props.d)) { }

Properties::~Properties() { }

Properties &Properties::operator=(const Properties &props) {
    *d = *props.d;
    return *this;
}

Properties &Properties::operator=(Properties &&props) {
    d = std::move(props.d);
    return *this;
}

bool Properties::has_property(std::string_view name) const {
    return d->entries.find(name) != d->entries.end();
}

Type Properties::type(std::string_view name) const {
    EntryMap::iterator it = d->entries.find(name);
    if (it == d->entries.end())
        Throw("type(): Could not find property named \"%s\"!", name);

    struct Visitor {
        Type operator()(const bool &) { return Type::Bool; }
        Type operator()(const int64_t &) { return Type::Integer; }
        Type operator()(const Float &) { return Type::Float; }
        Type operator()(const std::string&) { return Type::String; }
        Type operator()(const Reference &) { return Type::Reference; }
        Type operator()(const Array3f &) { return Type::Vector; }
        Type operator()(const Color3f &) { return Type::Color; }
        Type operator()(const Transform4f &) { return Type::Transform; }
        Type operator()(const ref<Object> &) { return Type::Object; }
        Type operator()(const Any &) { return Type::Any; }
    };

    return std::visit(Visitor(), it->second.value);
}

bool Properties::mark_queried(std::string_view name) const {
    EntryMap::iterator it = d->entries.find(name);
    if (it == d->entries.end())
        return false;
    it->second.queried = 1;
    return true;
}

bool Properties::was_queried(std::string_view name) const {
    EntryMap::iterator it = d->entries.find(name);
    if (it == d->entries.end())
        Throw("Could not find property named \"%s\"!", name);
    return it->second.queried;
}

bool Properties::remove_property(std::string_view name) {
    EntryMap::iterator it = d->entries.find(name);
    if (it == d->entries.end())
        return false;
    d->entries.erase_fast(it);
    return true;
}

std::string_view Properties::plugin_name() const {
    return d->plugin_name;
}

void Properties::set_plugin_name(std::string_view name) {
    d->plugin_name = name;
}

std::string_view Properties::id() const { return d->id; }

void Properties::set_id(std::string_view id) {
    d->id = id;
}

void Properties::raise_object_type_error(std::string_view name,
                                         ObjectType expected_type,
                                         const ref<Object> &value) const {
    Throw("Property \"%s\": has an incompatible object type! (expected %s, got %s)",
          name, object_type_name(expected_type), object_type_name(value->type()));
}

namespace {
    /// Return a readable string representation of a C++ type
    std::string demangle_type_name(const std::type_info &t) {
        const char *name_in = t.name();

#if defined(__GNUG__)
        int status = 0;
        char *name = abi::__cxa_demangle(name_in, nullptr, nullptr, &status);
        if (!name)
            return std::string(name_in);
        std::string result(name);
        free(name);
        return result;
#else
        return std::string(name_in);
#endif
    }
}

void Properties::raise_any_type_error(std::string_view name,
                                      const std::type_info &requested_type) const {
    // Get the Any value to determine its actual type
    const Any *any_value = get_impl<Any>(name, false);
    if (!any_value)
        Throw("Property \"%s\" has not been specified!", name);

    std::string requested_type_name = demangle_type_name(requested_type),
                actual_type_name    = demangle_type_name(any_value->type());

    Throw("Property \"%s\" cannot be cast to the requested type! "
          "(requested: %s, actual: %s)",
          name, requested_type_name.c_str(), actual_type_name.c_str());
}

std::vector<std::string> Properties::property_names() const {
    std::vector<std::string> result;
    result.reserve(d->entries.size());

    for (const auto &e : d->entries)
        result.push_back(e.first);

    // Sort by insertion order
    std::sort(result.begin(), result.end(),
              [this](const std::string &a, const std::string &b) {
                  auto it_a = d->entries.find(a), it_b = d->entries.find(b);
                  return it_a->second.insertion_index < it_b->second.insertion_index;
              });

    return result;
}

std::vector<std::pair<std::string, Reference>>
Properties::references(bool mark_queried) const {
    std::vector<std::pair<std::string, Reference>> result;

    // Iterate through properties in insertion order
    for (const auto &name : property_names()) {
        auto it = d->entries.find(name);
        const Reference *nr = std::get_if<Reference>(&it->second.value);
        if (!nr)
            continue;

        result.emplace_back(name, *nr);
        it->second.queried |= mark_queried;
    }

    return result;
}

std::vector<std::pair<std::string, ref<Object>>>
Properties::objects(bool mark_queried) const {
    std::vector<std::pair<std::string, ref<Object>>> result;

    // Iterate through properties in insertion order
    for (const auto &name : property_names()) {
        auto it = d->entries.find(name);
        const ref<Object> *o = std::get_if<ref<Object>>(&it->second.value);
        if (!o)
            continue;

        result.emplace_back(name, *o);
        it->second.queried |= mark_queried;
    }

    return result;
}

std::vector<std::string> Properties::unqueried() const {
    std::vector<std::string> result;

    // Iterate through properties in insertion order
    for (const auto &name : property_names()) {
        auto it = d->entries.find(name);
        if (!it->second.queried)
            result.push_back(name);
    }

    return result;
}

void Properties::merge(const Properties &p) {
    for (const auto &e : p.d->entries) {
        auto [it, success] = d->entries.insert_or_assign(e.first, e.second);
        if (success)
            it->second.insertion_index = d->insertion_index++;
    }
}

bool Properties::operator==(const Properties &p) const {
    if (d->plugin_name != p.d->plugin_name ||
        d->id != p.d->id ||
        d->entries.size() != p.d->entries.size())
        return false;

    for (const auto &e : d->entries) {
        EntryMap::iterator it = p.d->entries.find(e.first);
        if (it == p.d->entries.end())
            return false;

        // Compare the typeids of the two Entry variants
        if (e.second.value.index() != it->second.value.index())
            return false;

        // Use a visitor to compare values of the same type
        struct CompareVisitor {
            const Variant &other;
            bool operator()(const bool &v) const { return v == std::get<bool>(other); }
            bool operator()(const int64_t &v) const { return v == std::get<int64_t>(other); }
            bool operator()(const Float &v) const { return v == std::get<Float>(other); }
            bool operator()(const std::string &v) const { return v == std::get<std::string>(other); }
            bool operator()(const Array3f &v) const { return dr::all(v == std::get<Array3f>(other)); }
            bool operator()(const Color3f &v) const { return dr::all(v == std::get<Color3f>(other)); }
            bool operator()(const Transform4f &v) const { return v == std::get<Transform4f>(other); }
            bool operator()(const Reference &v) const { return v == std::get<Reference>(other); }
            bool operator()(const ref<Object> &v) const { return v == std::get<ref<Object>>(other); }
            bool operator()(const Any &) const { return false; } // Any cannot easily be compared
        };

        if (!std::visit(CompareVisitor{it->second.value}, e.second.value))
            return false;
    }

    return true;
}

namespace {
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
        void operator()(const Reference &nr) { os << "\"" << (const std::string &) nr << "\""; }
        void operator()(const ref<Object> &o) { os << o->to_string(); }
        void operator()(const Any &) { os << "[any]"; }
    };
}

std::string Properties::as_string(std::string_view name) const {
    EntryMap::iterator it = d->entries.find(name);
    if (it == d->entries.end())
        Throw("Property \"%s\" not found!", name);

    std::ostringstream oss;
    std::visit(StreamVisitor(oss), it->second.value);
    return oss.str();
}

std::string Properties::as_string(std::string_view name, std::string_view def_val) const {
    EntryMap::iterator it = d->entries.find(name);
    if (it == d->entries.end())
        return std::string(def_val);

    std::ostringstream oss;
    std::visit(StreamVisitor(oss), it->second.value);
    return oss.str();
}

std::ostream &operator<<(std::ostream &os, const Properties &p) {
    os << "Properties[" << std::endl
       << "  plugin_name = \"" << (p.d->plugin_name) << "\"," << std::endl
       << "  id = \"" << p.d->id << "\"," << std::endl
       << "  elements = {" << std::endl;

    // Get property names in insertion order
    std::vector<std::string> names = p.property_names();

    for (size_t i = 0; i < names.size(); ++i) {
        os << "    \"" << names[i] << "\" -> ";
        os << p.as_string(names[i]);
        if (i + 1 < names.size()) os << ",";
        os << std::endl;
    }

    os << "  }" << std::endl
       << "]" << std::endl;

    return os;
}

template <typename T>
void Properties::set_impl(std::string_view name, const T &value, bool raise_if_exists) {
    auto [it, success] = d->entries.insert_or_assign(std::string(name), Entry(value, d->insertion_index));
    if (!success && raise_if_exists)
        Throw("Property \"%s\" was specified multiple times!", name);
    d->insertion_index++;
}

template <typename T>
void Properties::set_impl(std::string_view name, T &&value, bool raise_if_exists) {
    auto [it, success] = d->entries.insert_or_assign(std::string(name), Entry(std::forward<T>(value), d->insertion_index));
    if (!success && raise_if_exists)
        Throw("Property \"%s\" was specified multiple times!", name);
    d->insertion_index++;
}

template <typename T>
const T* Properties::get_impl(std::string_view name, bool raise_if_missing) const {
    EntryMap::const_iterator it = d->entries.find(name);
    if (it == d->entries.end()) {
        if (raise_if_missing)
            Throw("Property \"%s\" has not been specified!", name);
        else
            return nullptr;
    }

    const T *value = std::get_if<T>(&it->second.value);
    if (!value) {
        if (raise_if_missing)
            Throw("The property \"%s\" has the wrong type (expected %s, got %s)", name,
                  property_type_name(variant_type<T>::value),
                  property_type_name(type(name)));
        else
            return nullptr;
    }

    it->second.queried = 1;
    return value;
}


// Explicit template instantiations (space argument expands to empty mode)
MI_EXPORT_PROP_ALL( )

NAMESPACE_END(mitsuba)
