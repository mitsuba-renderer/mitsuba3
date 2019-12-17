/*
    Mitsuba 2: A Retargetable Forward and Inverse Renderer
    Copyright 2019, Realistic Graphics Lab, EPFL.

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
#  define MTS_EXPORT   __declspec(dllexport)
#  define MTS_IMPORT   __declspec(dllimport)
#  define MTS_NOINLINE __declspec(noinline)
#  define MTS_INLINE   __forceinline
#else
#  define MTS_EXPORT    __attribute__ ((visibility("default")))
#  define MTS_IMPORT
#  define MTS_NOINLINE  __attribute__ ((noinline))
#  define MTS_INLINE    __attribute__((always_inline)) inline
#endif

#define MTS_MODULE_CORE   1
#define MTS_MODULE_RENDER 2
#define MTS_MODULE_UI     3

#if MTS_BUILD_MODULE == MTS_MODULE_CORE
#  define MTS_EXPORT_CORE MTS_EXPORT
#  define MTS_EXTERN_CORE extern
#else
#  define MTS_EXPORT_CORE MTS_IMPORT
#  if defined(_MSC_VER)
#    define MTS_EXTERN_CORE
#  else
#    define MTS_EXTERN_CORE extern
#  endif
#endif

#if MTS_BUILD_MODULE == MTS_MODULE_RENDER
#  define MTS_EXPORT_RENDER MTS_EXPORT
#  define MTS_EXTERN_RENDER extern
#else
#  define MTS_EXPORT_RENDER MTS_IMPORT
#  if defined(_MSC_VER)
#    define MTS_EXTERN_RENDER
#  else
#    define MTS_EXTERN_RENDER extern
#  endif
#endif

#if MTS_BUILD_MODULE == MTS_MODULE_UI
#  define MTS_EXPORT_UI MTS_EXPORT
#else
#  define MTS_EXPORT_UI MTS_IMPORT
#endif

/* A few macro helpers to enable overloading macros based on the number of parameters */
#define MTS_EXPAND(x)                              x
#define MTS_COUNT(_1, _2, _3, _4, _5, COUNT, ...)  COUNT
#define MTS_VA_SIZE(...)                           MTS_EXPAND(MTS_COUNT(__VA_ARGS__, 5, 4, 3, 2, 1))
#define MTS_CAT_HELPER(a, b)                       a ## b
#define MTS_CAT(a, b)                              MTS_CAT_HELPER(a, b)

NAMESPACE_BEGIN(mitsuba)

/* Reduce namespace pollution from windows.h */
#if defined(__WINDOWS__)
#  if !defined(WIN32_LEAN_AND_MEAN)
#    define WIN32_LEAN_AND_MEAN
#  endif
#  if !defined(NOMINMAX)
#    define NOMINMAX
#  endif
#  if !defined(_USE_MATH_DEFINES)
#    define _USE_MATH_DEFINES
#  endif
#endif

/* Likely/unlikely macros (only on GCC/Clang) */
#if defined(__GNUG__) || defined(__clang__)
#  define likely(x)   __builtin_expect(!!(x), 1)
#  define unlikely(x) __builtin_expect(!!(x), 0)
#else
#  define likely(x)       (x)
#  define unlikely(x)     (x)
#endif

/* Processor architecture */
#if defined(_MSC_VER)
#  if defined(_M_X86) && !defined(__i386__)
#    error 32-bit builds are not supported. Please run cmake-gui.exe, delete the cache, and \
           regenerate a new version of the build system that uses a 64 bit version of the compiler
#  endif
#  if defined(_M_X64) && !defined(__x86_64__)
#    define __x86_64__
#  endif
#endif

#if defined(_MSC_VER) // warning C4127: conditional expression is constant
#  pragma warning (disable: 4127)
#endif

/* C++ version */
#if __cplusplus > 201402L
#  define MTS_CPP17
#endif

NAMESPACE_END(mitsuba)
