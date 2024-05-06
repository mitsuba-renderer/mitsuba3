#if defined(_MSC_VER)
#  pragma warning (disable: 4324) // warning C4324: 'std::pair<const std::string,mitsuba::Entry>': structure was padded due to alignment specifier
#  define _ENABLE_EXTENDED_ALIGNED_STORAGE
#endif

#include <cstdlib>
#include <iostream>
#include <map>
#include <sstream>
#include <cstring>
#include <climits>

#include <drjit/tensor.h>

#include <mitsuba/core/logger.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/variant.h>

NAMESPACE_BEGIN(mitsuba)

using Float         = typename Properties::Float;
using Color3f       = typename Properties::Color3f;
using Array3f       = typename Properties::Array3f;
using Transform3f   = typename Properties::Transform3f;
using Transform4f   = typename Properties::Transform4f;
using TensorHandle  = typename Properties::TensorHandle;

using VariantType = variant<
    bool,
    int64_t,
    Float,
    Array3f,
    std::string,
    Transform3f,
    Transform4f,
    TensorHandle,
    Color3f,
    NamedReference,
    ref<Object>,
    const void *
>;

struct alignas(32) Entry {
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
            long long l1 = std::strtoll(a_ptr, &a_end, 10);
            long long l2 = std::strtoll(b_ptr, &b_end, 10);
            if (a_end == (a.c_str() + a.size()) &&
                b_end == (b.c_str() + b.size()) &&
                l1 != LLONG_MAX && l2 != LLONG_MAX &&
                !((l1 == l2) && (a.size() != b.size()))) // catch leading zeros case (e.g. 001 vs 01))
                return l1 < l2;
        }

        return std::strcmp(a_ptr, b_ptr) < 0;
    }
};

struct Properties::PropertiesPrivate {
    std::map<std::string, Entry, SortKey> entries;
    std::string id, plugin_name;
};

using Iterator = typename std::map<std::string, Entry, SortKey>::iterator;

template <typename T, typename T2 = T>
T get_impl(const Iterator &it) {
    if (!it->second.data.template is<T>() && !it->second.data.template is<T2>())
        Throw("The property \"%s\" has the wrong type (expected <%s> or <%s>, is <%s>)",
              it->first, typeid(T).name(), typeid(T2).name(), it->second.data.type().name());
    it->second.queried = true;
    if (it->second.data.template is<T2>())
        return (T const &) (T2 const &) it->second.data;
    return (T const &) it->second.data;
}


/**
 * \brief Specialization to gracefully handle if user supplies either a 3x3 or 4x4 transform.
 * Historically, we didn't directly support Transform3 properties so want to maintain
 * backwards compatibility
 */
template<>
Transform3f get_impl<Transform3f, Transform4f>(const Iterator &it) {
    if (!it->second.data.template is<Transform3f>() && !it->second.data.template is<Transform4f>())
        Throw("The property \"%s\" has the wrong type (expected <%s> or <%s>, is <%s>)",
              it->first, typeid(Transform3f).name(), typeid(Transform4f).name(), it->second.data.type().name());
    it->second.queried = true;
    if (it->second.data.template is<Transform4f>())
        return ((Transform4f const &)it->second.data).extract();
    return (Transform3f const &) it->second.data;
}

template <typename T>
T get_routing(const Iterator &it) {
    if constexpr (dr::is_static_array_v<T>) {
        Assert(T::Size == 3);
        if constexpr (std::is_same_v<T, Color<float, 3>> ||
                      std::is_same_v<T, Color<double, 3>>)
            return (T) get_impl<Color3f, Array3f>(it);
        else
            return (T) get_impl<Array3f>(it);
    }

    if constexpr (std::is_same_v<T, TensorHandle>)
        return get_impl<TensorHandle>(it);

    if constexpr (std::is_same_v<T, Transform<Point<float, 3>>> ||
                  std::is_same_v<T, Transform<Point<double, 3>>>)
        return (T) get_impl<Transform3f, Transform4f>(it);

    if constexpr (std::is_same_v<T, Transform<Point<float, 4>>> ||
                  std::is_same_v<T, Transform<Point<double, 4>>>)
        return (T) get_impl<Transform4f>(it);

    if constexpr (std::is_floating_point_v<T>)
        return (T) get_impl<Float, int64_t>(it);

    if constexpr (std::is_same_v<T, ref<Object>>)
        return get_impl<ref<Object>>(it);

    if constexpr (std::is_same_v<T, bool>)
        return get_impl<T>(it);

    if constexpr (std::is_integral_v<T> && !std::is_pointer_v<T>) {
        int64_t v = get_impl<int64_t>(it);
        if constexpr (std::is_unsigned_v<T>) {
            if (v < 0) {
                Throw("Property \"%s\" has negative value %i, but was queried as a"
                    " size_t (unsigned).", it->first, v);
            }
        }
        return (T) v;
    }

    if constexpr (std::is_same_v<T, std::string>)
        return get_impl<T>(it);

    Throw("Unsupported type: <%s>.", typeid(T).name());
}

template <typename T>
T Properties::get(const std::string &name) const {
    const auto it = d->entries.find(name);
    if (it == d->entries.end())
        Throw("Property \"%s\" has not been specified!", name);
    return get_routing<T>(it);
}

template <typename T>
T Properties::get(const std::string &name, const T &def_val) const {
    const auto it = d->entries.find(name);
    if (it == d->entries.end())
        return def_val;
    return get_routing<T>(it);
}
#define DEFINE_PROPERTY_SETTER(Type, SetterName) \
    void Properties::SetterName(const std::string &name, Type const &value, bool error_duplicates) { \
        if (has_property(name) && error_duplicates) \
            Log(Error, "Property \"%s\" was specified multiple times!", name); \
        d->entries[name].data = (Type) value; \
        d->entries[name].queried = false; \
    }

#define DEFINE_PROPERTY_ACCESSOR(Type, TagName, SetterName, GetterName) \
    DEFINE_PROPERTY_SETTER(Type, SetterName) \
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

DEFINE_PROPERTY_SETTER(bool,         set_bool)
DEFINE_PROPERTY_SETTER(int64_t,      set_long)
DEFINE_PROPERTY_SETTER(Transform3f,  set_transform3f)
DEFINE_PROPERTY_SETTER(Transform4f,  set_transform)
DEFINE_PROPERTY_SETTER(TensorHandle, set_tensor_handle)
DEFINE_PROPERTY_SETTER(Color3f,      set_color)
DEFINE_PROPERTY_ACCESSOR(std::string,    string,  set_string,          string)
DEFINE_PROPERTY_ACCESSOR(NamedReference, ref,     set_named_reference, named_reference)
DEFINE_PROPERTY_ACCESSOR(ref<Object>,    object,  set_object,          object)
DEFINE_PROPERTY_ACCESSOR(const void *,   pointer, set_pointer,         pointer)

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

void Properties::share(const Properties &props) {
    d = props.d;
}

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
        Type operator()(const Transform3f &) { return Type::Transform3f; }
        Type operator()(const Transform4f &) { return Type::Transform4f; }
        Type operator()(const TensorHandle &) { return Type::Tensor; }
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
        void operator()(const Transform3f &t) { os << t; }
        void operator()(const Transform4f &t) { os << t; }
        void operator()(const TensorHandle &t) { os << t.get(); }
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

/// Float setter
void Properties::set_float(const std::string &name, const Float &value, bool error_duplicates) {
    if (has_property(name) && error_duplicates)
        Log(Error, "Property \"%s\" was specified multiple times!", name);
    d->entries[name].data = (Float) value;
    d->entries[name].queried = false;
}

/// Array3f setter
void Properties::set_array3f(const std::string &name, const Array3f &value, bool error_duplicates) {
    if (has_property(name) && error_duplicates)
        Log(Error, "Property \"%s\" was specified multiple times!", name);
    d->entries[name].data = (Array3f) value;
    d->entries[name].queried = false;
}

#if 0
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
    if (!o->class_()->derives_from(MI_CLASS(AnimatedTransform)))
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
    if (!o->class_()->derives_from(MI_CLASS(AnimatedTransform)))
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
#endif

ref<Object> Properties::find_object(const std::string &name) const {
    const auto it = d->entries.find(name);
    if (it == d->entries.end())
        return ref<Object>();

    if (!it->second.data.is<ref<Object>>())
        Throw("The property \"%s\" has the wrong type.", name);

    return it->second.data;
}

#define EXPORT_PROPERTY_ACCESSOR(T) \
    template MI_EXPORT_LIB T Properties::get<T>(const std::string &) const; \
    template MI_EXPORT_LIB T Properties::get<T>(const std::string &, const T&) const;

#define T(...) __VA_ARGS__
EXPORT_PROPERTY_ACCESSOR(T(bool))
EXPORT_PROPERTY_ACCESSOR(T(float))
EXPORT_PROPERTY_ACCESSOR(T(double))
EXPORT_PROPERTY_ACCESSOR(T(uint32_t))
EXPORT_PROPERTY_ACCESSOR(T(int32_t))
EXPORT_PROPERTY_ACCESSOR(T(uint64_t))
EXPORT_PROPERTY_ACCESSOR(T(int64_t))
EXPORT_PROPERTY_ACCESSOR(T(dr::Array<float, 3>))
EXPORT_PROPERTY_ACCESSOR(T(dr::Array<double, 3>))
EXPORT_PROPERTY_ACCESSOR(T(Point<float, 3>))
EXPORT_PROPERTY_ACCESSOR(T(Point<double, 3>))
EXPORT_PROPERTY_ACCESSOR(T(Vector<float, 3>))
EXPORT_PROPERTY_ACCESSOR(T(Vector<double, 3>))
EXPORT_PROPERTY_ACCESSOR(T(Color<float, 3>))
EXPORT_PROPERTY_ACCESSOR(T(Color<double, 3>))
EXPORT_PROPERTY_ACCESSOR(T(Transform<Point<float, 3>>))
EXPORT_PROPERTY_ACCESSOR(T(Transform<Point<double, 3>>))
EXPORT_PROPERTY_ACCESSOR(T(Transform<Point<float, 4>>))
EXPORT_PROPERTY_ACCESSOR(T(Transform<Point<double, 4>>))
EXPORT_PROPERTY_ACCESSOR(T(Properties::TensorHandle))
EXPORT_PROPERTY_ACCESSOR(T(std::string))
EXPORT_PROPERTY_ACCESSOR(T(ref<Object>))
#if defined(__APPLE__)
EXPORT_PROPERTY_ACCESSOR(T(size_t))
#endif
#undef T

NAMESPACE_END(mitsuba)
