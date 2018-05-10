#pragma once

#define MTS_VERSION_MAJOR 2
#define MTS_VERSION_MINOR 0
#define MTS_VERSION_PATCH 0

#define MTS_STRINGIFY(x) #x
#define MTS_TOSTRING(x)  MTS_STRINGIFY(x)

/// Current release of Mitsuba
#define MTS_VERSION                                                       \
    MTS_TOSTRING(MTS_VERSION_MAJOR) "."                                   \
    MTS_TOSTRING(MTS_VERSION_MINOR) "."                                   \
    MTS_TOSTRING(MTS_VERSION_PATCH)

/// Year of the current release
#define MTS_YEAR "2018"

/// Authors list
#define MTS_AUTHORS "Wenzel Jakob and others (see README.md)"

#include <mitsuba/core/platform.h>
#include <mitsuba/core/fwd.h>
#include <mitsuba/core/essentials.h>
