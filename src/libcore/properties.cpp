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
            Log(EError, "The property \"%s\" has the wrong type (expected <" #ReadableName ">). The " \
                    "complete property record is :\n%s", name.c_str(), toString().c_str()); \
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
            Log(EError, "The property \"%s\" has the wrong type (expected <" #ReadableName ">). The " \
                    "complete property record is :\n%s", name.c_str(), toString().c_str()); \
        } \
    }

// TODO: what to return when the cast fails? (if we're aiming for an exception-free function)
DEFINE_PROPERTY_ACCESSOR(bool, bool, Boolean, boolean)
DEFINE_PROPERTY_ACCESSOR(int64_t, int64_t, Long, integer)
DEFINE_PROPERTY_ACCESSOR(Float, Float, Float, float)
DEFINE_PROPERTY_ACCESSOR(std::string, std::string, String, string)
// TODO: support other types, type-specific getters & setters


// TODO: supertype for visitors
//class TypeVisitor : public boost::static_visitor<Properties::EPropertyType> {
//public:
//    Properties::EPropertyType operator()(const bool &) const              { return Properties::EBoolean; }
//    Properties::EPropertyType operator()(const int64_t &) const           { return Properties::EInteger; }
//    Properties::EPropertyType operator()(const Float &) const             { return Properties::EFloat; }
//    Properties::EPropertyType operator()(const Point &) const             { return Properties::EPoint; }
//    Properties::EPropertyType operator()(const Vector &) const            { return Properties::EVector; }
//    Properties::EPropertyType operator()(const Transform &) const         { return Properties::ETransform; }
//    Properties::EPropertyType operator()(const AnimatedTransform *) const { return Properties::EAnimatedTransform; }
//    Properties::EPropertyType operator()(const Spectrum &) const          { return Properties::ESpectrum; }
//    Properties::EPropertyType operator()(const std::string &) const       { return Properties::EString; }
//    Properties::EPropertyType operator()(const Properties::Data &) const  { return Properties::EData; }
//};

class EqualityVisitor {  //  : public boost::static_visitor<bool>

public:
    EqualityVisitor(const VariantType *ref) : ref(ref) { }

#define EQUALITY_VISITOR_METHOD(Type) \
    bool operator()(const Type &v) const { \
        const Type &v2 = *ref;  \
        return (v == v2); \
    }

    EQUALITY_VISITOR_METHOD(bool)
    EQUALITY_VISITOR_METHOD(int64_t)
    EQUALITY_VISITOR_METHOD(Float)
    EQUALITY_VISITOR_METHOD(std::string)

private:
    const VariantType *ref;

};

class StringVisitor {  //  : public boost::static_visitor<void>
public:
    StringVisitor(std::ostringstream &oss, bool quote) : oss(oss), quote(quote) { }

    void operator()(const bool &v) const              { oss << (v ? "true" : "false"); }
    void operator()(const int64_t &v) const           { oss << v; }
    void operator()(const Float &v) const             { oss << v; }
    void operator()(const std::string &v) const       { oss << (quote ? "\"" : "") << v << (quote ? "\"" : ""); }
private:
    std::ostringstream &oss;
    bool quote;
};

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
        const Entry &first = e.second;
        const Entry &second = p.d->entries[e.first];

        const EqualityVisitor visitor(&first.data);
        // TODO: need visitor concept to make sure the right overload is called
        if (visitor(second.data))
            return false;
    }

    return true;
}

std::string Properties::toString() const {
    using std::endl;
    auto it = d->entries.begin();
    std::ostringstream oss;
    StringVisitor strVisitor(oss, true);

    oss << "Properties[" << endl
        << "  pluginName = \"" << (d->pluginName) << "\"," << endl
        << "  id = \"" << d->id << "\"," << endl
        << "  elements = {" << endl;
    while (it != d->entries.end()) {
        oss << "    \"" << it->first << "\" -> ";
        const auto &data = it->second.data;
        // TODO: need visitor concept to make sure the right overload is called
        strVisitor(data);
        if (++it != d->entries.end())
            oss << ",";
        oss << endl;
    }
    oss << "  }" << endl
    << "]" << endl;
    return oss.str();
}

// TODO: serialization capabilities (?)
// TODO: "networked object" capabilities (?)

NAMESPACE_END(mitsuba)
