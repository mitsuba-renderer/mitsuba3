/*
    Mitsuba 3: A Retargetable Forward and Inverse Renderer
    Copyright 2021, Realistic Graphics Lab, EPFL.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#define MI_VERSION_MAJOR 3
#define MI_VERSION_MINOR 6
#define MI_VERSION_PATCH 0

#define MI_STRINGIFY(x) #x
#define MI_TOSTRING(x)  MI_STRINGIFY(x)

/// Current release of Mitsuba
#define MI_VERSION                                                             \
    MI_TOSTRING(MI_VERSION_MAJOR) "."                                          \
    MI_TOSTRING(MI_VERSION_MINOR) "."                                          \
    MI_TOSTRING(MI_VERSION_PATCH)

/// Year of the current release
#define MI_YEAR "2024"

/// Authors list
#define MI_AUTHORS "Realistic Graphics Lab, EPFL"

#include <mitsuba/core/config.h>
#include <mitsuba/core/platform.h>
