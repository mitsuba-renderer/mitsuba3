#pragma once

#if !defined(NAMESPACE_BEGIN)
#  define NAMESPACE_BEGIN(name) namespace name {
#endif
#if !defined(NAMESPACE_END)
#  define NAMESPACE_END(name) }
#endif

#if defined(_MSC_VER)
#  define MTS_EXPORT __declspec(dllexport)
#  define MTS_IMPORT __declspec(dllimport)
#  define MTS_NOINLINE __declspec(noinline)
#  define MTS_FORCEINLINE inline __forceinline
#else
#  define MTS_EXPORT __attribute__ ((visibility("default")))
#  define MTS_IMPORT
#  define MTS_NOINLINE __attribute__ ((noinline))
#  define MTS_FORCEINLINE inline __attribute__((always_inline))
#endif

#define MTS_MODULE_CORE 1
#define MTS_MODULE_RENDER 2
#define MTS_MODULE_HW 3
#define MTS_MODULE_BIDIR 4

#if MTS_BUILD_MODULE == MTS_MODULE_CORE
#  define MTS_EXPORT_CORE MTS_EXPORT
#else
#  define MTS_EXPORT_CORE MTS_IMPORT
#endif

#if MTS_BUILD_MODULE == MTS_MODULE_RENDER
#  define MTS_EXPORT_RENDER MTS_EXPORT
#else
#  define MTS_EXPORT_RENDER MTS_IMPORT
#endif

#if MTS_BUILD_MODULE == MTS_MODULE_GUI
#  define MTS_EXPORT_GUI MTS_EXPORT
#else
#  define MTS_EXPORT_GUI MTS_IMPORT
#endif

#if MTS_BUILD_MODULE == MTS_MODULE_HW
#  define MTS_EXPORT_HW MTS_EXPORT
#else
#  define MTS_EXPORT_HW MTS_IMPORT
#endif

#if MTS_BUILD_MODULE == MTS_MODULE_BIDIR
#  define MTS_EXPORT_BIDIR MTS_EXPORT
#else
#  define MTS_EXPORT_BIDIR MTS_IMPORT
#endif

/* A few macro helpers to enable overloading macros based on the number of parameters */
#define MTS_EXPAND(x)                              x
#define MTS_COUNT(_1, _2, _3, _4, _5, COUNT, ...)  COUNT
#define MTS_VA_SIZE(...)                           MTS_EXPAND(MTS_COUNT(__VA_ARGS__, 5, 4, 3, 2, 1))
#define MTS_CAT_HELPER(a, b)                       a ## b
#define MTS_CAT(a, b)                              MTS_CAT_HELPER(a, b)

NAMESPACE_BEGIN(mitsuba)

/* Define a 'Float' data type with customizable precision */
#if defined(DOUBLE_PRECISION)
    typedef double Float;
#elif defined(SINGLE_PRECISION)
    typedef float Float;
#else
#  error No precision flag was defined!
#endif

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

/* Definitions for various compiler intrinsics */
#if defined(__GNUG__) || defined(__clang__)
#  define MTS_CLZ     __builtin_clz
#  define MTS_CLZLL   __builtin_clzll
#elif defined(_MSC_VER)
#  define MTS_CLZ     __lzcnt
#  define MTS_CLZLL   __lzcnt64
#endif

#define MTS_BLSR      _blsr_u32
#define MTS_BLSRLL    _blsr_u64

/* Likely/unlikely macros (only on GCC/Clang) */
#if defined(__GNUG__) || defined(__clang__)
#  define likely(x)   __builtin_expect(!!(x), 1)
#  define unlikely(x) __builtin_expect(!!(x), 0)
#else
#  define likely(x)       (x)
#  define unlikely(x)     (x)
#endif

NAMESPACE_END(mitsuba)
