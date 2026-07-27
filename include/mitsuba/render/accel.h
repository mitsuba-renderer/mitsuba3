/*
    accel.h -- Retrieve the backend-specific acceleration-structure type
*/

#pragma once

#include <mitsuba/render/fwd.h>
#include <type_traits>

NAMESPACE_BEGIN(mitsuba)

// Forward declarations so the non-selected branches of the trait are valid
template <typename Float, typename Spectrum> struct OptixAccel;
template <typename Float, typename Spectrum> struct MetalAccel;
template <typename Float, typename Spectrum> struct EmbreeAccel;
template <typename Float, typename Spectrum> struct NativeAccel;

NAMESPACE_END(mitsuba)

#if defined(MI_ENABLE_CUDA)
#  include <mitsuba/render/accel_optix.h>
#endif
#if defined(MI_ENABLE_METAL)
#  include <mitsuba/render/accel_metal.h>
#endif
#if defined(MI_ENABLE_EMBREE)
#  include <mitsuba/render/accel_embree.h>
#else
#  include <mitsuba/render/accel_native.h>
#endif

NAMESPACE_BEGIN(mitsuba)

/// Concrete acceleration-structure type for the current variant
template <typename Float, typename Spectrum>
using SceneAccel = std::conditional_t<
    drjit::is_cuda_v<Float>, OptixAccel<Float, Spectrum>,
    std::conditional_t<
        drjit::is_metal_v<Float>, MetalAccel<Float, Spectrum>,
#if defined(MI_ENABLE_EMBREE)
        EmbreeAccel<Float, Spectrum>
#else
        NativeAccel<Float, Spectrum>
#endif
        >>;

NAMESPACE_END(mitsuba)
