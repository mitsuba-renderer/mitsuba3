#include <sstream>

#include <mitsuba/core/logger.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/variant.h>

NAMESPACE_BEGIN(mitsuba)

using VariantType = variant<bool, int64_t, Float, std::string>;

struct Entry {
    // TODO: support for more types (?)
    // Originally supported types: bool, int64_t, Float, Point, Vector,
    //                             Transform, AnimatedTransform *,
    //                             Spectrum, std::string, Properties::Data
    VariantType data;
    bool queried;
};

struct Properties::PropertiesPrivate {
    std::map<std::string, Entry> entries;
    std::string id, pluginName;
};

// TODO: add back useful "print full settings on error", requires Properties -> string
#define DEFINE_PROPERTY_ACCESSOR(Type, BaseType, TypeName, ReadableName) \
    void Properties::set##TypeName(const std::string &name, const Type &value, bool warnDuplicates) { \
        if (hasProperty(name) && warnDuplicates) \
            Log(EWarn, "Property \"%s\" was specified multiple times!", name.c_str()); \
        d->entries[name].data = (BaseType) value; \
        d->entries[name].queried = false; \
    } \
    \
    Type Properties::get##TypeName(const std::string &name) const { \
        const auto it = d->entries.find(name); \
        if (it == d->entries.end()) \
            Log(EError, "Property \"%s\" has not been specified!", name.c_str()); \
        try { \
            auto &result = (const BaseType &) it->second.data; \
            it->second.queried = true; \
            return result; \
        } catch (const std::bad_cast &) { \
            Log(EError, "The property \"%s\" has the wrong type (expected <" #ReadableName ">)."); \
        } \
    } \
    \
    Type Properties::get##TypeName(const std::string &name, const Type &defVal) const { \
        const auto it = d->entries.find(name); \
        if (it == d->entries.end()) \
            return defVal; \
        try { \
            auto &result = (const BaseType &) it->second.data; \
            it->second.queried = true; \
            return result; \
        } catch (const std::bad_cast &) { \
            Log(EError, "The property \"%s\" has the wrong type (expected <" #ReadableName ">)."); \
        } \
    }

// TODO: what to return when the cast fails? (if we're aiming for an exception-free function)
DEFINE_PROPERTY_ACCESSOR(bool, bool, Boolean, boolean)
DEFINE_PROPERTY_ACCESSOR(int64_t, int64_t, Long, integer)
DEFINE_PROPERTY_ACCESSOR(Float, Float, Float, float)
DEFINE_PROPERTY_ACCESSOR(std::string, std::string, String, string)
// TODO: support other types, type-specific getters & setters

Properties::Properties()
    : d(new PropertiesPrivate()) {
    d->id = "unnamed";
}

Properties::Properties(const std::string &pluginName)
    : d(new PropertiesPrivate()) {
    d->id = "unnamed";
    d->pluginName = pluginName;
}

Properties::Properties(const Properties &props)
    : d(new PropertiesPrivate()) {
    // TODO: only valid if PropertiesPrivate doesn't have pointers
    (*d) = *props.d;
    // TODO: special treatment to bump reference counts for managed types
}

Properties::~Properties() {
    // TODO: special treatment to decrease reference counts for managed types
}

void Properties::operator=(const Properties &props) {
    // TODO: only valid if PropertiesPrivate doesn't have pointers
    (*d) = *props.d;
    // TODO: special treatment to bump reference counts for managed types
}

bool Properties::hasProperty(const std::string &name) const {
    return d->entries.find(name) != d->entries.end();
}

void Properties::markQueried(const std::string &name) const {
    auto it = d->entries.find(name);
    if (it == d->entries.end())
        return;
    it->second.queried = true;
}

bool Properties::wasQueried(const std::string &name) const {
    const auto it = d->entries.find(name);
    if (it == d->entries.end())
        Log(EError, "Could not find parameter \"%s\"!", name.c_str());
    return it->second.queried;
}

bool Properties::removeProperty(const std::string &name) {
    const auto it = d->entries.find(name);
    if (it == d->entries.end())
        return false;

    // TODO: special treatment to decrease reference counts for managed types
    d->entries.erase(it);
    return true;
}

inline const std::string &Properties::getPluginName() const {
    return d->pluginName;
}
inline void Properties::setPluginName(const std::string &name) {
    d->pluginName = name;
}
inline const std::string &Properties::getID() const {
    return d->id;
}
inline void Properties::setID(const std::string &id) {
    d->id = id;
}

void Properties::copyAttribute(const Properties &properties,
                               const std::string &sourceName, const std::string &targetName) {
    const auto it = properties.d->entries.find(sourceName);
    if (it == properties.d->entries.end())
        Log(EError, "copyAttribute(): Could not find parameter \"%s\"!", sourceName.c_str());
    d->entries[targetName] = it->second;
}

void Properties::putPropertyNames(std::vector<std::string> &results) const {
    for (const auto e : d->entries) {
        results.push_back(e.first);
    }
}

std::vector<std::string> Properties::getUnqueried() const {
    std::vector<std::string> result;
    for (auto e : d->entries) {
        if (!e.second.queried)
            result.push_back(e.first);
    }
    return result;
}

void Properties::merge(const Properties &p) {
    for (const auto e : p.d->entries) {
        d->entries[e.first] = e.second;
    }
}

bool Properties::operator==(const Properties &p) const {
    if (d->pluginName != p.d->pluginName || d->id != p.d->id
        || d->entries.size() != p.d->entries.size())
        return false;

    for (const auto e : d->entries) {
        if (e.second.data != p.d->entries[e.first].data)
            return false;
    }

    return true;
}

// TODO: serialization capabilities (?)
// TODO: "networked object" capabilities (?)

NAMESPACE_END(mitsuba)
