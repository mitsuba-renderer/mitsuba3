/*
    Mitsuba 2: A Retargetable Forward and Inverse Renderer
    Copyright 2019, Realistic Graphics Lab, EPFL.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#define MTS_VERSION_MAJOR 2
#define MTS_VERSION_MINOR 2
#define MTS_VERSION_PATCH 1

#define MTS_STRINGIFY(x) #x
#define MTS_TOSTRING(x)  MTS_STRINGIFY(x)

/// Current release of Mitsuba
#define MTS_VERSION                                                       \
    MTS_TOSTRING(MTS_VERSION_MAJOR) "."                                   \
    MTS_TOSTRING(MTS_VERSION_MINOR) "."                                   \
    MTS_TOSTRING(MTS_VERSION_PATCH)

/// Year of the current release
#define MTS_YEAR "2020"

/// Authors list
#define MTS_AUTHORS "Realistic Graphics Lab, EPFL"

#include <mitsuba/core/config.h>
#include <mitsuba/core/platform.h>
#include <mitsuba/core/fwd.h>
