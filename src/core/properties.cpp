#include <iostream>
#include <variant>
#include <any>
#include <tsl/robin_map.h>
#include <drjit/tensor.h>

#include <mitsuba/core/logger.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/plugin.h>

NAMESPACE_BEGIN(mitsuba)

using Float         = double;
using Array3f       = dr::Array<Float, 3>;
using Color3f       = Color<Float, 3>;
using Transform4f   = Transform<Point<double, 4>>;

using Variant = std::variant<bool, int64_t, Float, std::string,
                             Array3f, Color3f, Transform4f,
                             Properties::Reference, ref<Object>,
                             std::any>;

static const char *variant_name(const Variant &v) {
    struct Visitor {
        const char *operator()(const bool&) { return "bool"; }
        const char *operator()(const int64_t&) { return "int"; }
        const char *operator()(const Float&) { return "float"; }
        const char *operator()(const std::string&) { return "string"; }
        const char *operator()(const Array3f&) { return "vector"; }
        const char *operator()(const Color3f&) { return "color"; }
        const char *operator()(const Transform4f&) { return "transform"; }
        const char *operator()(const Properties::Reference&) { return "reference"; }
        const char *operator()(const ref<Object>&) { return "object"; }
        const char *operator()(const std::any&) { return "any"; }
    };

    return std::visit(Visitor(), v);
}

struct Entry {
    Variant value;
    mutable bool queried = false;

    Entry() = default;

    template <typename T>
    Entry(const T &v) : value(v) { }

    /// Convert the attribute or fail
    template <typename T> T get(std::string_view name) const {
        using BaseType = std::decay_t<T>;
        using StorageType = prop_type_t<BaseType>;

        // Try conversion from storage type (this always works)
        const StorageType *storage_result = std::get_if<StorageType>(&value);
        if (storage_result) {
            if constexpr (std::is_same_v<BaseType, StorageType>) {
                return *storage_result;
            } else {
                // Handle integer bounds checking
                } else {
                    return BaseType(*storage_result);
                }
            }
        }

        Throw("The property \"%s\" has the wrong type", name);
    }
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

Properties::Type Properties::type(std::string_view name) const {
    EntryMap::iterator it = d->entries.find(name);
    if (it == d->entries.end())
        Throw("type(): Could not find property named \"%s\"!", name);

    struct Visitor {
        using Type = Properties::Type;
        Type operator()(const std::nullptr_t &) { throw std::runtime_error("Internal error"); }
        Type operator()(const bool &) { return Type::Bool; }
        Type operator()(const int64_t &) { return Type::Long; }
        Type operator()(const Float &) { return Type::Float; }
        Type operator()(const Array3f &) { return Type::Array3f; }
        Type operator()(const std::string&) { return Type::String; }
        Type operator()(const Transform3f &) { return Type::Transform3f; }
        Type operator()(const Transform4f &) { return Type::Transform4f; }
        Type operator()(const Color3f &) { return Type::Color; }
        Type operator()(const Reference &) { return Type::Reference; }
        Type operator()(const ref<Object> &) { return Type::Object; }
        Type operator()(const std::any &) { return Type::Any; }
    };

    return std::visit(Visitor(), it->second.value);
}

bool Properties::mark_queried(std::string_view name) const {
    EntryMap::iterator it = d->entries.find(name);
    if (it == d->entries.end())
        return false;
    it->second.queried = true;
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

std::string_view Properties::id() const {
    return d->id.c_str();
}

void Properties::set_id(std::string_view id) {
    d->id = id;
}

std::vector<std::string> Properties::property_names() const {
    std::vector<std::string> result;
    result.reserve(d->entries.size());
    for (const auto &e : d->entries)
        result.push_back(e.first);
    return result;
}

std::vector<std::pair<std::string, Properties::Reference>>
Properties::references(bool mark_queried) const {
    std::vector<std::pair<std::string, Reference>> result;
    result.reserve(d->entries.size());
    for (const auto &e : d->entries) {
        const Reference *nr = std::get_if<Reference>(&e.second.value);
        if (!nr)
            continue;
        result.emplace_back(e.first, *nr);
        e.second.queried |= mark_queried;
    }
    return result;
}

std::vector<std::pair<std::string, ref<Object>>>
Properties::objects(bool mark_queried) const {
    std::vector<std::pair<std::string, ref<Object>>> result;
    result.reserve(d->entries.size());
    for (auto &e : d->entries) {
        const ref<Object> *o = std::get_if<ref<Object>>(&e.second.value);
        if (!o)
            continue;
        result.emplace_back(e.first, *o);
        e.second.queried |= mark_queried;
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
        EntryMap::iterator it = p.d->entries.find(e.first);
        if (it == p.d->entries.end())
            return false;

        // CLAUDE: compare the typeids of the two Entry variants and return false if they don't match.
        // If they do match, call a visitor that casts both to the same type and calls operator==.
        // It should always return false when the type is std::any, since those cannot easily be compared.
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
        void operator()(const Properties::Reference &nr) { os << "\"" << (const std::string &) nr << "\""; }
        void operator()(const ref<Object> &o) { os << o->to_string(); }
        void operator()(const std::any &) { os << "[any]"; }
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


    std::vector<std::string> names = p.property_names();
    std::sort(names.begin(), names.end());

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
void Properties::set_impl(std::string_view name, T &&value, bool raise_if_exists) {
    auto [it, success] = d->entries.insert_or_assign(std::string(name), std::forward<T>(value));
    if (!success && raise_if_exists)
        Throw("Property \"%s\" was specified multiple times!", name);
}

template <typename T>
T* Properties::get_impl(std::string_view name, bool raise_if_missing) const {
    EntryMap::iterator it = d->entries.find(name);
    if (it == d->entries.end()) {
        if (raise_if_missing)
            Throw("Property \"%s\" has not been specified!", name);
        else
            return nullptr;
    }

    T *value = std::get_if<T>(it->value);
    if (!value)
        Throw("The property \"%s\" has the wrong type", name);

    it->second.queried = true;
    return value;
}

template <typename TextureType>
ref<TextureType> Properties::texture(std::string_view name) const {
    // CLAUDE: construct a texture here using the plugin manager, see on how this was done in prior commits
    mark_queried(name);
    return nullptr;
}

template <typename TextureType>
ref<TextureType> Properties::texture(std::string_view name, Float def_val) const {
    EntryMap::iterator it = d->entries.find(name);
    if (it == d->entries.end()) {
        // Create a uniform texture with the default value
        Log(Warn, "Properties::texture() with float default not fully implemented yet. "
                  "Returning nullptr for parameter \"%s\" with default %f", name, def_val);
        mark_queried(name);
        return nullptr;
    }

    // Get the actual texture from properties
    ref<TextureType> result = it->second.get<ref<TextureType>>(name);
    it->second.queried = true;
    // CLAUDE: construct a texture here using the plugin manager, see on how this was done in prior commits. Make a default texture if none was found
    return result;
}

template <typename TextureType>
ref<TextureType> Properties::texture_d65(std::string_view name, Float def_val) const {
    EntryMap::iterator it = d->entries.find(name);
    if (it == d->entries.end()) {
        // Create a uniform D65 texture with the default value
        Log(Warn, "Properties::texture_d65() with float default not fully implemented yet. "
                  "Returning nullptr for parameter \"%s\" with default %f", name, def_val);
        mark_queried(name);
        return nullptr;
    }

    // Get the actual texture from properties
    ref<TextureType> result = it->second.get<ref<TextureType>>(name);
    it->second.queried = true;
    // CLAUDE construct a texture here using the plugin manager, see on how this was done in prior commits. Make a default texture with D65 whitepoint if none was found
    return result;
}

MI_EXPORT_PROP_ALL()
NAMESPACE_END(mitsuba)
