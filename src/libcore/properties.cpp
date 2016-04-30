#include <mitsuba/core/properties.h>
#include <mitsuba/core/variant.h>

NAMESPACE_BEGIN(mitsuba)

struct Entry {
    variant<bool, int64_t, Float, std::string> data;
    bool queried;
};

struct Properties::PropertiesPrivate {
    std::map<std::string, Entry> entries;
};

bool Properties::hasProperty(const std::string &name) const {
    return d->entries.find(name) != d->entries.end();
}

#define DEFINE_PROPERTY_ACCESSOR(Type, BaseType, TypeName, ReadableName) \
    void Properties::set##TypeName(const std::string &name, const Type &value, bool warnDuplicates) { \
        if (hasProperty(name) && warnDuplicates) \
            SLog(EWarn, "Property \"%s\" was specified multiple times!", name.c_str()); \
        d->entries[name].data = (BaseType) value; \
        d->entries[name].queried = false; \
    } \
    \
    Type Properties::get##TypeName(const std::string &name) const { \
        std::map<std::string, PropertyElement>::const_iterator it = d->elements.find(name); \
        if (it == d->elements.end()) \
            SLog(EError, "Property \"%s\" has not been specified!", name.c_str()); \
        try { \
            auto &result = (const BaseType &) it->second.data; \
            it->second.queried = true; \
            return result; \
        } catch (const std::bad_cast &) { \
            SLog(EError, "The property \"%s\" has the wrong type (expected <" #ReadableName ">). The " \
                    "complete property record is :\n%s", name.c_str(), toString().c_str()); \
        } \
    } \
    \
    Type Properties::get##TypeName(const std::string &name, const Type &defVal) const { \
        std::map<std::string, PropertyElement>::const_iterator it = d->elements.find(name); \
        if (it == d->elements.end()) \
            return defVal; \
        try { \
            auto &result = (const BaseType &) it->second.data; \
            it->second.queried = true; \
            return result; \
        } catch (const std::bad_cast &) { \
            SLog(EError, "The property \"%s\" has the wrong type (expected <" #ReadableName ">). The " \
                    "complete property record is :\n%s", name.c_str(), toString().c_str()); \
        } \
    }

NAMESPACE_END(mitsuba)
