/*
    misuka: A Differentiable Room Acoustic Renderer
    Copyright 2026, the misuka maintainers.

    Based on Mitsuba 3, Copyright 2021, Realistic Graphics Lab, EPFL.

    All rights reserved. Use of this source code is governed by the
    licenses described in the LICENSE file.
*/

#pragma once

#define MI_VERSION_MAJOR 0
#define MI_VERSION_MINOR 1
#define MI_VERSION_PATCH 0

#define MI_STRINGIFY(x) #x
#define MI_TOSTRING(x)  MI_STRINGIFY(x)

/// Current release of misuka
#define MI_VERSION                                                             \
    MI_TOSTRING(MI_VERSION_MAJOR) "."                                          \
    MI_TOSTRING(MI_VERSION_MINOR) "."                                          \
    MI_TOSTRING(MI_VERSION_PATCH)

/// Year of the current release
#define MI_YEAR "2026"

/// Authors list
#define MI_AUTHORS "The misuka maintainers"

#include <mitsuba/core/config.h>
#include <mitsuba/core/platform.h>
