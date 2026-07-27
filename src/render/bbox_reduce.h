#pragma once

#include <mitsuba/core/bbox.h>

NAMESPACE_BEGIN(mitsuba)

/// Compute the bounding box of an interleaved point buffer on the device.
///
/// \c data is interleaved with \c stride scalars per element and the position at
/// offsets 0, 1, 2. With \c radius_offset >= 0 each point is grown by the scalar
/// at that offset (curve control points). The component gathers and the radius
/// expansion are fused into a single kernel by one \ref dr::eval, so the buffer
/// is read once; the six extents are then reduced and read back with one sync.
template <typename ScalarPoint3, typename FloatStorage>
BoundingBox<ScalarPoint3> device_reduce_bbox(const FloatStorage &data,
                                             uint32_t count, uint32_t stride,
                                             int radius_offset = -1) {
    using UInt32 = dr::uint32_array_t<FloatStorage>;
    using Vec    = dr::Array<FloatStorage, 3>;

    UInt32 base = dr::arange<UInt32>(count) * stride;
    Vec pos(dr::gather<FloatStorage>(data, base + 0u),
            dr::gather<FloatStorage>(data, base + 1u),
            dr::gather<FloatStorage>(data, base + 2u));

    Vec lo = pos, hi = pos;
    if (radius_offset >= 0) {
        FloatStorage r =
            dr::gather<FloatStorage>(data, base + (uint32_t) radius_offset);
        lo = pos - r;
        hi = pos + r;
    }
    dr::eval(lo, hi);

    Vec p_min(dr::min(lo.x()), dr::min(lo.y()), dr::min(lo.z())),
        p_max(dr::max(hi.x()), dr::max(hi.y()), dr::max(hi.z()));

    Vec bmin = dr::migrate(p_min, JitBackend::None),
        bmax = dr::migrate(p_max, JitBackend::None);
    dr::sync_thread();

    return BoundingBox<ScalarPoint3>(
        ScalarPoint3(bmin.x().data()[0], bmin.y().data()[0], bmin.z().data()[0]),
        ScalarPoint3(bmax.x().data()[0], bmax.y().data()[0], bmax.z().data()[0]));
}

NAMESPACE_END(mitsuba)
