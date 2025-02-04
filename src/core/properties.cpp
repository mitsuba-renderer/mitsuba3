#include <iostream>
#include <variant>
#include <tsl/robin_map.h>
#include <drjit/tensor.h>

#include <mitsuba/core/logger.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>

NAMESPACE_BEGIN(mitsuba)

using Float         = typename Properties::Float;
using Array3f       = dr::Array<Float, 3>;
using Color3f       = typename Properties::Color3f;
using Transform4f   = typename Properties::Transform4f;

using Variant = std::variant<bool, int64_t, Float, Array3f, std::string,
                             Transform4f, Color3f, Properties::NamedReference,
                             ref<Object>, const void *>;

static const char *variant_name(const Variant &v) {
    struct Visitor {
        const char *operator()(const bool&) { return "bool"; }
        const char *operator()(const int64_t&) { return "int"; }
        const char *operator()(const Float&) { return "float"; }
        const char *operator()(const Array3f&) { return "vector"; }
        const char *operator()(const std::string&) { return "string"; }
        const char *operator()(const Transform4f&) { return "transform"; }
        const char *operator()(const ref<Object>&) { return "object"; }
        const char *operator()(const void *) { return "void *"; }
    };

    return std::visit(Visitor(), v);
}

struct Entry {
    Variant value;
    mutable bool queried = false;

    template <typename T>
    Entry(const T &value) : value(value) { }

    /// Convert the attribute or fail
    template <typename T> T &get(std::string_view name) const {
        T *result = std::get_if<T>(&value);
        if (!result) {
            Variant wanted = Variant(T());
            Throw("The property \"%s\" has the wrong type (expected <%s>, got <%s>)",
                  name, variant_name(wanted), variant_name(value));
        }
        return *result;
    }

    /// .. additionally try conversion to an alterantive type
    template <typename T, typename T2> T get(std::string_view name) const {
        T *result = std::get_if<T>(&value);
        if (!result) {
            T2 *result_2 = std::get_if<T2>(value);
            if (result_2) {
                Variant wanted = Variant(T()),
                        wanted_2 = Variant(T2());
                Throw("The property \"%s\" has the wrong type (expected <%s> or "
                      "<%s>, got <%s>)",
                      name, variant_name(wanted), variant_name(wanted_2),
                      variant_name(value));
            }
            return T(*result_2);
        }
        return *result;
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
        Type operator()(std::string_view ) { return Type::String; }
        Type operator()(const Transform3f &) { return Type::Transform3f; }
        Type operator()(const Transform4f &) { return Type::Transform4f; }
        Type operator()(const Color3f &) { return Type::Color; }
        Type operator()(const NamedReference &) { return Type::NamedReference; }
        Type operator()(const ref<Object> &) { return Type::Object; }
        Type operator()(const void *&) { return Type::Pointer; }
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

std::vector<std::pair<std::string, Properties::NamedReference>>
Properties::named_references(bool mark_queried) const {
    std::vector<std::pair<std::string, NamedReference>> result;
    result.reserve(d->entries.size());
    for (const auto &e : d->entries) {
        const NamedReference *nr = std::get_if<NamedReference>(&e.second.value);
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

        std::visit([&](const auto &value) {
            using T = std::decay_t<decltype(value)>;
            const T *p = std::get_if<T>(&e.second.value);
            if (!p)
                return false;
            return dr::all(value == *p);
        }, it->second.value);
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
        void operator()(std::string_view s) { os << "\"" << s << "\""; }
        void operator()(const Transform4f &t) { os << t; }
        void operator()(const Color3f &t) { os << t; }
        void operator()(const Properties::NamedReference &nr) { os << "\"" << (const std::string &) nr << "\""; }
        void operator()(const ref<Object> &o) { os << o->to_string(); }
        void operator()(const void *&p) { os << p; }
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
void Properties::set(std::string_view name, const T &value, bool error_duplicates) {
    auto [it, success] = d->entries.insert_or_assign(std::string(name), value);
    if (!success && error_duplicates)
        Throw("Property \"%s\" was specified multiple times!", name);
}

template <typename T> T Properties::get(std::string_view name) const {
    EntryMap::iterator it = d->entries.find(name);
    if (it == d->entries.end())
        Throw("Property \"%s\" has not been specified!", name);
    const T &result = it->second.get<const T &>(name);
    it->second.queried = true;
    return result;
}

template <typename T>
T Properties::get(std::string_view name, const T &def_val) const {
    EntryMap::iterator it = d->entries.find(name);
    if (it == d->entries.end())
        return def_val;
    const T &result = it->second.get<const T &>(name);
    it->second.queried = true;
    return result;
}

MI_EXPORT_PROP_ALL()
NAMESPACE_END(mitsuba)
