#pragma once

#if !defined(NAMESPACE_BEGIN)
#  define NAMESPACE_BEGIN(name) namespace name {
#endif
#if !defined(NAMESPACE_END)
#  define NAMESPACE_END(name) }
#endif

#ifdef __WINDOWS__
#  define MTS_EXPORT __declspec(dllexport)
#  define MTS_IMPORT __declspec(dllimport)
#else
#  define MTS_EXPORT __attribute__ ((visibility("default")))
#  define MTS_IMPORT
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
