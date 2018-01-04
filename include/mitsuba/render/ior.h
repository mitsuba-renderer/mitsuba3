#pragma once

#include <mitsuba/mitsuba.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/properties.h>

NAMESPACE_BEGIN(mitsuba)

struct IOREntry {
    const char *name;
    Float value;
};

/**
* Many values are taken from Hecht, Optics,
* Fourth Edition.
*
* The IOR values are from measurements between
* 0 and 20 degrees celsius at ~589 nm.
*/
static IOREntry ior_data[] = {
    { "vacuum",                1.0f },
    { "helium",                1.000036f },
    { "hydrogen",              1.000132f },
    { "air",                   1.000277f },
    { "carbon dioxide",        1.00045f },
    //////////////////////////////////////
    { "water",                 1.3330f },
    { "acetone",               1.36f },
    { "ethanol",               1.361f },
    { "carbon tetrachloride",  1.461f },
    { "glycerol",              1.4729f },
    { "benzene",               1.501f },
    { "silicone oil",          1.52045f },
    { "bromine",               1.661f },
    //////////////////////////////////////
    { "water ice",             1.31f },
    { "fused quartz",          1.458f },
    { "pyrex",                 1.470f },
    { "acrylic glass",         1.49f },
    { "polypropylene",         1.49f },
    { "bk7",                   1.5046f },
    { "sodium chloride",       1.544f },
    { "amber",                 1.55f },
    { "pet",                   1.5750f },
    { "diamond",               2.419f },

    { nullptr,                 0.0f }
};

static Float lookup_ior(const std::string &name) {
    std::string lower_case = string::to_lower(name);
    IOREntry *ior = ior_data;

    while (ior->name) {
        if (lower_case == ior->name)
            return ior->value;
        ++ior;
    }

    std::ostringstream oss;
    oss << "Unable to find an IOR value for \"" << lower_case
        << "\"! Valid choices are:";

    // Unable to find the IOR value by name -- print an error
    // message that lists all possible options
    for (ior = ior_data; ior->name != NULL; ++ior) {
        oss << ior->name;
        if ((ior + 1)->name)
            oss << ", ";
    }

    Log(EError, "%s", oss.str().c_str());
    return 0.0f;
}

inline Float lookup_ior(const Properties &props, const std::string &param_name,
                        const std::string &default_value) {
    if (props.has_property(param_name) && props.type(param_name) == Properties::EFloat)
        return props.float_(param_name);
    else
        return lookup_ior(props.string(param_name, default_value));
}

inline Float lookup_ior(const Properties &props, const std::string &param_name,
                        Float default_value) {
    if (props.has_property(param_name)) {
        if (props.type(param_name) == Properties::EFloat)
            return props.float_(param_name);
        else
            return lookup_ior(props.string(param_name));
    }
    else {
        return default_value;
    }
}

NAMESPACE_END(mitsuba)
