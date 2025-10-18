#include <iostream>
#include <variant>
#include <memory>
#include <cstring>
#include <tsl/robin_map.h>
#include <drjit/tensor.h>
#include <typeinfo>

#include <mitsuba/core/logger.h>
#include <mitsuba/core/object.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/hash.h>
#include <mitsuba/render/texture.h>

#if defined(__GNUG__)
#  include <cxxabi.h>
#endif

NAMESPACE_BEGIN(mitsuba)

// Map property types to human-readable type names
std::string_view property_type_name(Properties::Type type) {
    switch (type) {
        case Properties::Type::Unknown:           return "unknown";
        case Properties::Type::Bool:              return "boolean";
        case Properties::Type::Integer:           return "integer";
        case Properties::Type::Float:             return "float";
        case Properties::Type::String:            return "string";
        case Properties::Type::Vector:            return "vector";
        case Properties::Type::Color:             return "rgb";
        case Properties::Type::Spectrum:          return "spectrum";
        case Properties::Type::Transform:         return "transform";
        case Properties::Type::Reference:         return "ref";
        case Properties::Type::ResolvedReference: return "resolved_ref";
        case Properties::Type::Object:            return "object";
        case Properties::Type::Any:               return "any";
    }
    return "invalid"; // (to avoid a compiler warning; this should never happen)
}

using Float             = double;
using Array3f           = dr::Array<Float, 3>;
using Color3f           = Color<Float, 3>;
using AffineTransform3f = AffineTransform<Point<double, 3>>;
using AffineTransform4f = AffineTransform<Point<double, 4>>;
using Reference         = Properties::Reference;
using ResolvedReference = Properties::ResolvedReference;
using Type              = Properties::Type;

template <typename T> struct variant_type;
template<> struct variant_type<bool> { static constexpr auto value = Type::Bool; };
template<> struct variant_type<Float> { static constexpr auto value = Type::Float; };
template<> struct variant_type<int64_t> { static constexpr auto value = Type::Integer; };
template<> struct variant_type<std::string> { static constexpr auto value = Type::String; };
template<> struct variant_type<Array3f> { static constexpr auto value = Type::Vector; };
template<> struct variant_type<Color3f> { static constexpr auto value = Type::Color; };
template<> struct variant_type<AffineTransform4f> { static constexpr auto value = Type::Transform; };
template<> struct variant_type<Reference> { static constexpr auto value = Type::Reference; };
template<> struct variant_type<ResolvedReference> { static constexpr auto value = Type::ResolvedReference; };
template<> struct variant_type<ref<Object>> { static constexpr auto value = Type::Object; };
template<> struct variant_type<Any> { static constexpr auto value = Type::Any; };
template<> struct variant_type<Properties::Spectrum> { static constexpr auto value = Type::Spectrum; };

using Variant = std::variant<std::monostate, bool, int64_t, Float, std::string,
                             Array3f, Color3f, Properties::Spectrum, AffineTransform4f, Reference,
                             ResolvedReference, ref<Object>, Any>;

/// Minimal heap-allocated string for efficient storage with string_view compatibility
/// Unlike std::string which uses small string optimization, heap_string is ALWAYS
/// heap-allocated, ensuring the string data remains stable when the object is moved
/// around (e.g., when std::vector<Entry> reallocates). This stability allows us to
/// safely store string_views pointing to this data in key_to_index.
struct heap_string {
    std::unique_ptr<char[]> data;
    size_t length = 0;

    // Initialization from std::string_view
    heap_string(std::string_view str) : length(str.length()) {
        if (length > 0) {
            data = std::make_unique<char[]>(length + 1);
            std::memcpy(data.get(), str.data(), length);
            data[length] = '\0';
        }
    }

    heap_string() = default;
    heap_string(heap_string &&) = default;
    ~heap_string() = default;

    heap_string(const heap_string &other) : length(other.length) {
        if (length > 0) {
            data = std::make_unique<char[]>(length + 1);
            std::memcpy(data.get(), other.data.get(), length);
            data[length] = '\0';
        }
    }

    heap_string& operator=(const heap_string &other) {
        if (this != &other)
            *this = heap_string(other);
        return *this;
    }

    heap_string& operator=(heap_string &&) = default;

    operator std::string_view() const {
        return std::string_view(data.get(), length);
    }

    friend std::ostream& operator<<(std::ostream& os, const heap_string& hs) {
        return os << std::string_view(hs);
    }
};

struct Entry {
    heap_string key;
    Variant value;
    mutable bool queried;

    Entry() : queried(false) { }

    template <typename T> Entry(std::string_view k, T &&v)
        : key(k), value(std::forward<T>(v)), queried(false) { }
};

struct Properties::PropertiesPrivate {
    std::vector<Entry> entries;
    tsl::robin_map<std::string_view, size_t, std::hash<std::string_view>, std::equal_to<>> key_to_index;
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
    : d(new PropertiesPrivate()) {
    // Copy all members
    d->entries = props.d->entries;
    d->plugin_name = props.d->plugin_name;
    d->id = props.d->id;

    // Rebuild key_to_index with string_views backed by this instance
    for (size_t i = 0; i < d->entries.size(); ++i) {
        if (!std::holds_alternative<std::monostate>(d->entries[i].value))
            d->key_to_index[d->entries[i].key] = i;
    }
}

Properties::Properties(Properties &&props)
    : d(std::move(props.d)) { }

Properties::~Properties() { }

Properties &Properties::operator=(const Properties &props) {
    if (this != &props)
        *this = Properties(props);
    return *this;
}

Properties &Properties::operator=(Properties &&props) {
    d = std::move(props.d);
    return *this;
}

bool Properties::has_property(std::string_view name) const {
    return d->key_to_index.find(name) != d->key_to_index.end();
}

Type Properties::type(std::string_view name) const {
    auto it = d->key_to_index.find(name);
    if (it == d->key_to_index.end())
        Throw("type(): Could not find property named \"%s\"!", name);

    return (Type) d->entries[it->second].value.index();
}

bool Properties::mark_queried(std::string_view name, bool value) const {
    auto it = d->key_to_index.find(name);
    if (it == d->key_to_index.end())
        return false;
    d->entries[it->second].queried = value;
    return true;
}

bool Properties::was_queried(std::string_view name) const {
    auto it = d->key_to_index.find(name);
    if (it == d->key_to_index.end())
        Throw("Could not find property named \"%s\"!", name);
    return d->entries[it->second].queried;
}

bool Properties::remove_property(std::string_view name) {
    auto it = d->key_to_index.find(name);
    if (it == d->key_to_index.end())
        return false;

    // Set entry as unused ("tombstone")
    Entry &entry = d->entries[it->second];
    d->key_to_index.erase(it);
    entry.key = heap_string();  // Clear the key
    entry.value = std::monostate{};
    return true;
}

bool Properties::rename_property(std::string_view old_name, std::string_view new_name) {
    auto it = d->key_to_index.find(old_name);
    if (it == d->key_to_index.end())
        return false;

    // Check if new name already exists
    if (d->key_to_index.find(new_name) != d->key_to_index.end())
        return false;

    size_t index = it->second;
    Entry &entry = d->entries[index];

    // Remove old mapping
    d->key_to_index.erase(it);

    // Update the key in the entry
    entry.key = heap_string(new_name);

    // Add new mapping using the updated key
    d->key_to_index[entry.key] = index;
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

size_t Properties::key_index(std::string_view name) const noexcept {
    auto it = d->key_to_index.find(name);
    if (it == d->key_to_index.end())
        return size_t(-1);
    return it->second;
}

size_t Properties::key_index_checked(std::string_view name) const {
    auto it = d->key_to_index.find(name);
    if (it == d->key_to_index.end())
        Throw("Property \"%s\" cannot be found!", name);
    return it->second;
}

size_t Properties::maybe_append(std::string_view name, bool raise_if_exists) {
    size_t index = key_index(name);

    if (index != size_t(-1)) {
        // Key already exists, raise an exception if this is not OK
        if (raise_if_exists)
            Throw("Property \"%s\" was specified multiple times!", name);
    } else {
        // Create new entry with empty value
        index = d->entries.size();
        d->entries.emplace_back(name, std::monostate{});
        d->key_to_index[d->entries[index].key] = index;
    }

    return index;
}

std::string_view Properties::entry_name(size_t index) const {
    return d->entries[index].key;
}

void Properties::raise_object_type_error(size_t index,
                                         ObjectType expected_type,
                                         const ref<Object> &value) const {
    std::string_view name = d->entries[index].key;
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

void Properties::raise_any_type_error(size_t index,
                                      const std::type_info &requested_type) const {
    // Get the Any value to determine its actual type
    const Any *any_value = get_impl<Any>(index, false);
    const Entry& entry = d->entries[index];
    if (!any_value)
        Throw("Property \"%s\" has not been specified!", entry.key);

    std::string requested_type_name = demangle_type_name(requested_type),
                actual_type_name    = demangle_type_name(any_value->type());

    Throw("Property \"%s\" cannot be cast to the requested type! "
          "(requested: %s, actual: %s)",
          entry.key, requested_type_name, actual_type_name);
}

std::vector<std::string> Properties::unqueried() const {
    std::vector<std::string> result;

    for (const auto &entry : d->entries) {
        if (std::holds_alternative<std::monostate>(entry.value))
            continue;  // Skip tombstones

        if (!entry.queried)
            result.push_back(std::string(entry.key));
    }

    return result;
}

void Properties::merge(const Properties &p) {
    for (const auto &entry : p.d->entries) {
        if (std::holds_alternative<std::monostate>(entry.value))
            continue;  // Skip tombstones

        // Check if key already exists
        auto it = d->key_to_index.find(entry.key);
        if (it != d->key_to_index.end()) {
            Entry &entry2 = d->entries[it->second];
            entry2.value = entry.value;
            entry2.queried = entry.queried;
        } else {
            // Add new entry
            size_t new_index = d->entries.size();
            d->entries.push_back(entry);
            d->key_to_index[d->entries[new_index].key] = new_index;
        }
    }
}

bool Properties::operator==(const Properties &p) const {
    if (d->plugin_name != p.d->plugin_name ||
        d->key_to_index.size() != p.d->key_to_index.size())
        return false;

    for (const auto &[key, index] : d->key_to_index) {
        auto it = p.d->key_to_index.find(key);
        if (it == p.d->key_to_index.end())
            return false;

        const auto &entry1 = d->entries[index];
        const auto &entry2 = p.d->entries[it->second];

        // Compare the typeids of the two Entry variants
        if (entry1.value.index() != entry2.value.index())
            return false;

        // Use a visitor to compare values of the same type
        struct CompareVisitor {
            const Variant &other;
            bool operator()(const std::monostate &) const { return true; }
            bool operator()(const bool &v) const { return v == std::get<bool>(other); }
            bool operator()(const int64_t &v) const { return v == std::get<int64_t>(other); }
            bool operator()(const Float &v) const { return v == std::get<Float>(other); }
            bool operator()(const std::string &v) const { return v == std::get<std::string>(other); }
            bool operator()(const Array3f &v) const { return dr::all(v == std::get<Array3f>(other)); }
            bool operator()(const Color3f &v) const { return dr::all(v == std::get<Color3f>(other)); }
            bool operator()(const Properties::Spectrum &v) const { return v == std::get<Properties::Spectrum>(other); }
            bool operator()(const AffineTransform4f &v) const { return v == std::get<AffineTransform4f>(other); }
            bool operator()(const Reference &v) const { return v == std::get<Reference>(other); }
            bool operator()(const ResolvedReference &v) const { return v == std::get<ResolvedReference>(other); }
            bool operator()(const ref<Object> &v) const { return v == std::get<ref<Object>>(other); }
            bool operator()(const Any &v) const { return v == std::get<Any>(other); }
        };

        if (!std::visit(CompareVisitor{entry2.value}, entry1.value))
            return false;
    }

    return true;
}

size_t Properties::hash() const {
    // Start with plugin name hash
    size_t h = mitsuba::hash(d->plugin_name);

    // Collect and sort properties by name
    std::vector<std::pair<std::string_view, size_t>> sorted_props;
    for (const auto &[key, index] : d->key_to_index) {
        sorted_props.emplace_back(key, index);
    }
    std::sort(sorted_props.begin(), sorted_props.end());

    // Hash each property in sorted order
    for (const auto &[key, index] : sorted_props) {
        h = hash_combine(h, mitsuba::hash(key));

        const auto &entry = d->entries[index];
        // Hash the type
        h = hash_combine(h, mitsuba::hash(entry.value.index()));

        // Hash the value using a visitor
        struct HashVisitor {
            size_t operator()(const std::monostate &) const { return 0; }
            size_t operator()(const bool &v) const { return mitsuba::hash(v); }
            size_t operator()(const int64_t &v) const { return mitsuba::hash(v); }
            size_t operator()(const Float &v) const { return mitsuba::hash(v); }
            size_t operator()(const std::string &v) const { return mitsuba::hash(v); }
            size_t operator()(const Array3f &v) const {
                size_t h = mitsuba::hash(v.x());
                h = hash_combine(h, mitsuba::hash(v.y()));
                h = hash_combine(h, mitsuba::hash(v.z()));
                return h;
            }
            size_t operator()(const Color3f &v) const {
                size_t h = mitsuba::hash(v[0]);
                h = hash_combine(h, mitsuba::hash(v[1]));
                h = hash_combine(h, mitsuba::hash(v[2]));
                return h;
            }
            size_t operator()(const Properties::Spectrum &s) const {
                size_t h = mitsuba::hash(s.wavelengths);
                h = hash_combine(h, mitsuba::hash(s.values));
                return h;
            }
            size_t operator()(const AffineTransform4f &t) const {
                size_t h = 0;
                for (int i = 0; i < 4; ++i)
                    for (int j = 0; j < 4; ++j)
                        h = hash_combine(h, mitsuba::hash(t.matrix(i, j)));
                return h;
            }
            size_t operator()(const Reference &r) const {
                return mitsuba::hash(r.id());
            }
            size_t operator()(const ResolvedReference &r) const {
                return mitsuba::hash(r.index());
            }
            size_t operator()(const ref<Object> &) const {
                return 0; // Ignored in hash
            }
            size_t operator()(const Any &) const {
                return 0; // Ignored in hash
            }
        };

        h = hash_combine(h, std::visit(HashVisitor{}, entry.value));
    }

    return h;
}

namespace {
    struct StreamVisitor {
        std::ostream &os;
        StreamVisitor(std::ostream &os) : os(os) { }
        void operator()(const std::monostate &) { os << "[deleted]"; }
        void operator()(const bool &b) { os << (b ? "true" : "false"); }
        void operator()(const int64_t &i) { os << i; }
        void operator()(const Float &f) { os << f; }
        void operator()(const Array3f &t) { os << t; }
        void operator()(const std::string &s) { os << "\"" << s << "\""; }
        void operator()(const AffineTransform4f &t) { os << t.matrix; }
        void operator()(const Color3f &t) { os << t; }
        void operator()(const Properties::Spectrum &s) {
            if (s.is_uniform()) {
                os << "[spectrum: " << s.values[0] << "]";
            } else {
                os << "[spectrum: " << s.wavelengths.size() << " samples]";
            }
        }
        void operator()(const Reference &nr) { os << "[ref: \"" << nr.id() << "\"]"; }
        void operator()(const ResolvedReference &rr) { os << "[resolved ref: " << rr.index() << "]"; }
        void operator()(const ref<Object> &o) { os << o->to_string(); }
        void operator()(const Any &) { os << "[any]"; }
    };
}

std::string Properties::as_string(std::string_view name) const {
    auto it = d->key_to_index.find(name);
    if (it == d->key_to_index.end())
        Throw("Property \"%s\" not found!", name);

    std::ostringstream oss;
    std::visit(StreamVisitor(oss), d->entries[it->second].value);
    return oss.str();
}

std::string Properties::as_string(std::string_view name, std::string_view def_val) const {
    auto it = d->key_to_index.find(name);
    if (it == d->key_to_index.end())
        return std::string(def_val);

    std::ostringstream oss;
    std::visit(StreamVisitor(oss), d->entries[it->second].value);
    return oss.str();
}

std::ostream &operator<<(std::ostream &os, const Properties &p) {
    os << "Properties[" << std::endl
       << "  plugin_name = \"" << (p.d->plugin_name) << "\"," << std::endl
       << "  id = \"" << p.d->id << "\"," << std::endl
       << "  elements = {" << std::endl;

    size_t i = 0;
    for (const auto &kv : p) {
        os << "    \"" << kv.name() << "\" -> ";
        os << p.as_string(kv.name());
        if (++i < p.size())
            os << ",";
        os << std::endl;
    }

    os << "  }" << std::endl
       << "]" << std::endl;

    return os;
}

template<>
void Properties::set_impl<AffineTransform3f>(size_t index, const AffineTransform3f &value) {
    d->entries[index].value = value.expand();
    d->entries[index].queried = false;
}

template<>
void Properties::set_impl<AffineTransform3f>(size_t index, AffineTransform3f &&value) {
    d->entries[index].value = value.expand();
    d->entries[index].queried = false;
}

template <typename T>
void Properties::set_impl(size_t index, const T &value) {
    d->entries[index].value = value;
    d->entries[index].queried = false;
}

template <typename T>
void Properties::set_impl(size_t index, T &&value) {
    d->entries[index].value = std::forward<T>(value);
    d->entries[index].queried = false;
}

template <typename T>
const T* Properties::get_impl(size_t index, bool raise_if_incompatible, bool mark_queried) const {
    const Entry &entry = d->entries[index];
    const T *value = std::get_if<T>(&entry.value);
    if (!value) {
        if (raise_if_incompatible)
            Throw("The property \"%s\" has the wrong type (expected %s, got %s)", entry.key,
                  property_type_name(variant_type<T>::value),
                  property_type_name((Type) entry.value.index()));
        else
            return nullptr;
    }

    if (mark_queried)
        entry.queried = true;
    return value;
}

void Properties::mark_queried(size_t index) const {
    d->entries[index].queried = true;
}

size_t Properties::size() const {
    return d->key_to_index.size();
}

ref<Object> Properties::get_texture_impl(std::string_view name,
                                         std::string_view variant,
                                         bool emitter,
                                         bool unbounded) const {
    size_t index = key_index(name);
    const Entry &entry = d->entries[index];
    const Variant &value = entry.value;
    Type type = (Type) value.index();

    Properties props;
    std::string_view plugin_name;

    // Determine variant properties
    bool is_spectral      = variant.find("spectral") != std::string::npos,
         is_monochromatic = variant.find("mono")     != std::string::npos;

    switch (type) {
        case Type::Float:
        case Type::Integer: {
                double scalar_value = (type == Type::Float)
                    ? std::get<double>(value)
                    : (double) std::get<int64_t>(value);

                if (is_monochromatic || !emitter) {
                    // For monochromatic variants or non-emitters, create uniform texture
                    plugin_name = "uniform";
                    props.set("value", scalar_value);
                } else {
                    // For RGB/spectral emitters, create d65 texture with grayscale color
                    plugin_name = "d65";
                    plugin_name = is_spectral ? "d65" : "srgb";
                    props.set("color", Color3f(scalar_value));
                }
            }
            break;

        case Type::Color: {
                Color3f color = std::get<Color3f>(value);

                if (is_monochromatic) {
                    plugin_name = "uniform";
                    props.set("value", luminance(color));
                } else {
                    plugin_name = (emitter && is_spectral) ? "d65" : "srgb";
                    props.set("color", color);
                }
            }
            break;

        case Type::Spectrum: {
            const Spectrum &spectrum = std::get<Spectrum>(value);

            if (spectrum.is_uniform()) {
                double const_value = spectrum.values[0];

                if (!is_spectral && !is_monochromatic && emitter) {
                    /* A uniform spectrum does not produce a uniform RGB response in
                       sRGB (which has a D65 white point). The call to 'xyz_to_srgb'
                       computes this purple-ish color and uses it to initialize the
                       'srgb' plugin. This is needed so that RGB variants of Mitsuba
                       match the behavior of spectral variants */
                    plugin_name = "srgb";
                    props.set("color", const_value * xyz_to_srgb(Color3f(1.0)));
                } else {
                    plugin_name = "uniform";
                    props.set("value", const_value);
                }
            } else {
                // Wavelength-value pairs
                if (is_spectral) {
                    plugin_name = spectrum.is_regular() ? "regular" : "irregular";
                    props.set("value", spectrum);
                } else {
                    // In RGB/mono modes, pre-integrate against CIE matching curves

                    Color3f color = spectrum_list_to_srgb(
                        spectrum.wavelengths, spectrum.values,
                        /* bounded */ !(emitter || unbounded),
                        /* d65 */ !emitter);

                    if (is_monochromatic) {
                        plugin_name = "uniform";
                        props.set("value", luminance(color));
                    } else {
                        plugin_name = "srgb";
                        props.set("color", color);
                    }
                }
            }
            break;
        }

        case Type::Object: {
                const ref<Object> &obj = std::get<ref<Object>>(value);
                if (obj->type() != ObjectType::Texture)
                    raise_object_type_error(index, ObjectType::Texture, obj);
                entry.queried = true;
                return obj;
            }

        default:
            Throw("The property \"%s\" has the wrong type (expected texture/float/color/spectrum, got %s)", name,
                  property_type_name(type));
    }

    if (plugin_name == "srgb" && (unbounded || emitter))
        props.set("unbounded", true);

    entry.queried = true;

    // Set the plugin name after the switch
    props.set_plugin_name(plugin_name);

    return PluginManager::instance()->create_object(
        props, variant, ObjectType::Texture);
}

ref<Object> Properties::get_texture_impl(std::string_view name,
                                         std::string_view variant,
                                         bool emitter,
                                         bool unbounded,
                                         double def) const {
    if (has_property(name))
        return get_texture_impl(name, variant, emitter, unbounded);

    bool is_spectral = variant.find("spectral") != std::string::npos;
    Properties props;

    if (emitter && is_spectral) {
        props.set_plugin_name("d65");
        props.set("scale", def);
    } else {
        props.set_plugin_name("uniform");
        props.set("value", def);
    }

    return PluginManager::instance()->create_object(
        props, variant, ObjectType::Texture);
}

// ==========================================================================
//   Properties::key_iterator implementation
// ==========================================================================

// Iterator implementation
Properties::key_iterator Properties::begin() const {
    return key_iterator(this, 0);
}

Properties::key_iterator Properties::end() const {
    return key_iterator(nullptr, (size_t) -1);
}

void Properties::key_iterator::skip_to_next_valid() {
    if (m_index == (size_t) -1)
        return;

    while (m_index < m_props->d->entries.size()) {
        const Entry &entry = m_props->d->entries[m_index];
        Type entry_type = (Type) entry.value.index();

        // Skip tombstones and filtered types
        if (entry_type != Type::Unknown &&
            (m_filter_type == Type::Unknown || entry_type == m_filter_type)) {
            // Found a valid entry
            m_name = entry.key;
            m_type = entry_type;
            m_queried = entry.queried;
            return;
        }

        ++m_index;
    }

    // Not found, mark as invalid
    m_index = (size_t) -1;
}

Properties::key_iterator::key_iterator(const Properties *props, size_t index, Type filter_type)
    : m_props(props), m_index(index), m_filter_type(filter_type), m_queried(false) {
    skip_to_next_valid();
}

Properties::key_iterator& Properties::key_iterator::operator++() {
    ++m_index;
    skip_to_next_valid();
    return *this;
}

Properties::key_iterator Properties::key_iterator::operator++(int) {
    key_iterator tmp = *this;
    ++(*this);
    return tmp;
}

// ==========================================================================
//   Properties::Spectrum implementation
// ==========================================================================

Properties::Spectrum::Spectrum(std::vector<double> &&wavelengths_, std::vector<double> &&values_)
    : wavelengths(std::move(wavelengths_)), values(std::move(values_)) {

    if (wavelengths.size() < 2)
        Throw("Spectrum must have at least two entries");

    if (wavelengths.size() != this->values.size())
        Throw("Wavelength and value arrays must have the same size.");

    for (size_t i = 1; i < wavelengths.size(); ++i) {
        if (wavelengths[i] <= wavelengths[i-1])
            Throw("Wavelengths must be specified in increasing order.");
    }

    // Check if the wavelengths are regularly spaced
    m_regular = true;
    double interval = wavelengths[1] - wavelengths[0];

    for (size_t i = 2; i < wavelengths.size(); ++i) {
        double curr_interval = wavelengths[i] - wavelengths[i-1];
        if (std::abs(curr_interval - interval) > 1e-3 * interval) {
            m_regular = false;
            break;
        }
    }
}

Properties::Spectrum::Spectrum(std::string_view str) {
    auto tokens = string::tokenize(str);

    if (tokens.size() == 1 && tokens[0].find(':') == std::string_view::npos) {
        // Uniform spectrum (single value, no colon)
        try {
            double value = string::stof<double>(tokens[0]);
            values.push_back(value);
            m_regular = false;
        } catch (...) {
            Throw("Could not parse uniform spectrum value \"%s\"", tokens[0]);
        }
    } else {
        // Wavelength-value pairs
        std::vector<double> temp_wavelengths, temp_values;
        temp_wavelengths.reserve(tokens.size());
        temp_values.reserve(tokens.size());

        for (std::string_view token: tokens) {
            auto pair = string::tokenize(token, ":");
            if (pair.size() != 2)
                Throw("Invalid spectrum value \"%s\" (expected wavelength:value pairs)", token);

            try {
                temp_wavelengths.push_back(string::stof<double>(pair[0]));
                temp_values.push_back(string::stof<double>(pair[1]));
            } catch (...) {
                Throw("Could not parse wavelength:value pair \"%s\"", token);
            }
        }

        // Constructor will check monotonicity and regularity
        *this = Spectrum(std::move(temp_wavelengths), std::move(temp_values));
    }
}

Properties::Spectrum::Spectrum(std::string_view values_str,
                               double wavelength_min,
                               double wavelength_max) {

    auto tokens = string::tokenize(values_str, " ,");

    if (tokens.size() < 2)
        Throw("Regular spectrum requires at least 2 values");

    std::vector<double> temp_values;
    temp_values.reserve(tokens.size());

        for (std::string_view token: tokens) {
        try {
            temp_values.push_back(string::stof<double>(token));
        } catch (...) {
            Throw("Could not parse floating point value \"%s\"", token);
        }
    }

    *this = Spectrum(std::move(temp_values), wavelength_min, wavelength_max);
}

Properties::Spectrum::Spectrum(std::vector<double> &&values_,
                               double wavelength_min,
                               double wavelength_max)
    : values(std::move(values_)) {

    if (values.size() < 2)
        Throw("Regular spectrum requires at least 2 values");

    double step = (wavelength_max - wavelength_min) / (values.size() - 1);

    wavelengths.resize(values.size());
    for (size_t i = 0; i < values.size(); ++i)
        wavelengths[i] = wavelength_min + step * i;

    m_regular = true;
}

Properties::Spectrum::Spectrum(std::string_view wavelengths_str, std::string_view values_str) {
    auto wavelength_tokens = string::tokenize(wavelengths_str, " ,");
    auto value_tokens = string::tokenize(values_str, " ,");

    if (wavelength_tokens.size() != value_tokens.size())
        Throw("Spectrum: 'wavelengths' and 'values' have incompatible sizes "
              "(%zu vs %zu)", wavelength_tokens.size(), value_tokens.size());

    wavelengths.resize(wavelength_tokens.size());
    values.resize(value_tokens.size());

    for (size_t i = 0; i < wavelength_tokens.size(); ++i) {
        try {
            wavelengths[i] = string::stof<double>(wavelength_tokens[i]);
        } catch (...) {
            Throw("Could not parse wavelength value \"%s\"", wavelength_tokens[i]);
        }

        try {
            values[i] = string::stof<double>(value_tokens[i]);
        } catch (...) {
            Throw("Could not parse spectrum value \"%s\"", value_tokens[i]);
        }
    }

    // Constructor will check monotonicity and regularity
    *this = Spectrum(std::move(wavelengths), std::move(values));
}

Properties::Spectrum::Spectrum(const fs::path &file_path) {
    if (!fs::exists(file_path))
        Throw("\"%s\": file does not exist!", file_path);

    FILE *file = fopen(file_path.string().c_str(), "r");
    if (!file)
        Throw("Could not open spectrum file \"%s\"", file_path);

    char line[1024];
    size_t line_num = 0;
    while (fgets(line, sizeof(line), file)) {
        line_num++;

        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r')
            continue;

        char *p   = line,
             *end = line + strlen(line),
             *tmp;

        double wl = string::parse_float<double>(p, end, &tmp);
        if (tmp == p) {
            fclose(file);
            Throw("Could not extract wavelength in line %zu of spectrum file \"%s\": \"%s\"",
                  line_num, file_path, line);
        }
        p = tmp;

        double val = string::parse_float<double>(p, end, &tmp);
        if (tmp == p) {
            fclose(file);
            Throw("Could not extract value in line %zu of spectrum file \"%s\": \"%s\"",
                  line_num, file_path, line);
        }

        wavelengths.push_back(wl);
        values.push_back(val);
    }

    fclose(file);

    // Constructor will check monotonicity and regularity
    *this = Spectrum(std::move(wavelengths), std::move(values));
}

#if defined(_MSC_VER)
#  pragma warning(push)
#  pragma warning(disable: 4003) // not enough arguments for function-like macro invocation
#endif
MI_EXPORT_PROP_ALL( )
#if defined(_MSC_VER)
#  pragma warning(pop)
#endif
NAMESPACE_END(mitsuba)
