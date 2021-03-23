#pragma once

#ifndef __CUDACC__
# include <mitsuba/core/bbox.h>
#endif

namespace optix {
struct BoundingBox3f {
    BoundingBox3f() = default;
#ifndef __CUDACC__
    template <typename T>
    BoundingBox3f(const mitsuba::BoundingBox<mitsuba::Point<T, 3>> &b) {
        min[0] = (float) b.min[0];
        min[1] = (float) b.min[1];
        min[2] = (float) b.min[2];
        max[0] = (float) b.max[0];
        max[1] = (float) b.max[1];
        max[2] = (float) b.max[2];
    }
#endif
    float min[3];
    float max[3];
};
} // end namespace optix
