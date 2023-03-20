#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/distr_2d.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/render/srgb.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/core/bitmap.h>
#include <drjit/tensor.h>
#include <drjit/texture.h>
#include <drjit/struct.h>
#include <mutex>

NAMESPACE_BEGIN(mitsuba)

// Texture interpolation methods: dr::FilterMode
// Texture wrapping methods: dr::WrapMode

#define MI_MIPMAP_LUT_SIZE 128

/// Specifies the desired antialiasing filter
enum MIPFilterType {
    /// No filtering, nearest neighbor lookups
    Nearest = 0,
    /// No filtering, only bilinear interpolation
    Bilinear = 1,
    /// Basic trilinear filtering
    Trilinear = 2,
    /// Elliptically weighted average
    EWA = 3
};







NAMESPACE_END(mitsuba)