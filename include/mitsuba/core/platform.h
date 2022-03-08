/*
    Mitsuba 3: A Retargetable Forward and Inverse Renderer
    Copyright 2021, Realistic Graphics Lab, EPFL.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#if !defined(NAMESPACE_BEGIN)
#  define NAMESPACE_BEGIN(name) namespace name {
#endif
#if !defined(NAMESPACE_END)
#  define NAMESPACE_END(name) }
#endif

#if defined(_MSC_VER)
#  define MI_EXPORT   __declspec(dllexport)
#  define MI_IMPORT   __declspec(dllimport)
#  define MI_NOINLINE __declspec(noinline)
#  define MI_INLINE   __forceinline
#else
#  define MI_EXPORT    __attribute__ ((visibility("default")))
#  define MI_IMPORT
#  define MI_NOINLINE  __attribute__ ((noinline))
#  define MI_INLINE    __attribute__((always_inline)) inline
#endif

#define MI_MODULE_LIB    1
#define MI_MODULE_UI     2

#if MI_BUILD_MODULE == MI_MODULE_LIB
#  define MI_EXPORT_LIB MI_EXPORT
#  define MI_EXTERN_LIB extern
#else
#  define MI_EXPORT_LIB MI_IMPORT
#  if defined(_MSC_VER)
#    define MI_EXTERN_LIB
#  else
#    define MI_EXTERN_LIB extern
#  endif
#endif

#if MI_BUILD_MODULE == MI_MODULE_UI
#  define MI_EXPORT_UI MI_EXPORT
#else
#  define MI_EXPORT_UI MI_IMPORT
#endif

/* A few macro helpers to enable overloading macros based on the number of parameters */
#define MI_EXPAND(x)                              x
#define MI_COUNT(_1, _2, _3, _4, _5, COUNT, ...)  COUNT
#define MI_VA_SIZE(...)                           MI_EXPAND(MI_COUNT(__VA_ARGS__, 5, 4, 3, 2, 1))
#define MI_CAT_HELPER(a, b)                       a ## b
#define MI_CAT(a, b)                              MI_CAT_HELPER(a, b)

NAMESPACE_BEGIN(mitsuba)

// Reduce namespace pollution from windows.h
#if defined(_WIN32)
#  if !defined(WIN32_LEAN_AND_MEAN)
#    define WIN32_LEAN_AND_MEAN
#  endif
#  if !defined(_USE_MATH_DEFINES)
#    define _USE_MATH_DEFINES
#  endif
#endif

// Likely/unlikely macros (only on GCC/Clang)
#if defined(__GNUG__) || defined(__clang__)
#  define likely(x)   __builtin_expect(!!(x), 1)
#  define unlikely(x) __builtin_expect(!!(x), 0)
#else
#  define likely(x)       (x)
#  define unlikely(x)     (x)
#endif

// Processor architecture
#if defined(_MSC_VER) && defined(_M_X86)
#  error 32-bit builds are not supported. Please run cmake-gui.exe, delete the cache, and \
         regenerate a new version of the build system that uses a 64 bit version of the compiler
#endif

#if defined(_MSC_VER) // warning C4127: conditional expression is constant
#  pragma warning (disable: 4127)
#endif

NAMESPACE_END(mitsuba)
