#include <mitsuba/core/logger.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/variant.h>

NAMESPACE_BEGIN(mitsuba)

struct Entry {
    // TODO: support for more types (?)
    // Originally supported types: bool, int64_t, Float, Point, Vector,
    //                             Transform, AnimatedTransform *,
    //                             Spectrum, std::string, Properties::Data
    variant<bool, int64_t, Float, std::string> data;
    bool queried;
};

struct Properties::PropertiesPrivate {
    std::map<std::string, Entry> entries;
    std::string id, pluginName;
};

bool Properties::hasProperty(const std::string &name) const {
    return d->entries.find(name) != d->entries.end();
}

#define DEFINE_PROPERTY_ACCESSOR(Type, BaseType, TypeName, ReadableName) \
    void Properties::set##TypeName(const std::string &name, const Type &value, bool warnDuplicates) { \
        if (hasProperty(name) && warnDuplicates) \
            Log(EWarn, "Property \"%s\" was specified multiple times!", name.c_str()); \
        d->entries[name].data = (BaseType) value; \
        d->entries[name].queried = false; \
    } \
    \
    Type Properties::get##TypeName(const std::string &name) const { \
        std::map<std::string, Entry>::const_iterator it = d->entries.find(name); \
        if (it == d->entries.end()) \
            Log(EError, "Property \"%s\" has not been specified!", name.c_str()); \
        try { \
            auto &result = (const BaseType &) it->second.data; \
            it->second.queried = true; \
            return result; \
        } catch (const std::bad_cast &) { \
            Log(EError, "The property \"%s\" has the wrong type (expected <" #ReadableName ">). The " \
                    "complete property record is :\n%s", name.c_str(), toString().c_str()); \
        } \
    } \
    \
    Type Properties::get##TypeName(const std::string &name, const Type &defVal) const { \
        std::map<std::string, Entry>::const_iterator it = d->entries.find(name); \
        if (it == d->entries.end()) \
            return defVal; \
        try { \
            auto &result = (const BaseType &) it->second.data; \
            it->second.queried = true; \
            return result; \
        } catch (const std::bad_cast &) { \
            Log(EError, "The property \"%s\" has the wrong type (expected <" #ReadableName ">). The " \
                    "complete property record is :\n%s", name.c_str(), toString().c_str()); \
        } \
    }

DEFINE_PROPERTY_ACCESSOR(bool, bool, Boolean, boolean)
DEFINE_PROPERTY_ACCESSOR(int64_t, int64_t, Long, integer)
DEFINE_PROPERTY_ACCESSOR(Float, Float, Float, float)
DEFINE_PROPERTY_ACCESSOR(std::string, std::string, String, string)
// TODO: support other types, type-specific getters & setters

// TODO: "visitor" helpers for getType, equality & asString

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


// TODO: other properties' management methods

// TODO: merge operator

// TODO: serialization capabilities (?)
// TODO: "networked object" capabilities (?)

NAMESPACE_END(mitsuba)
