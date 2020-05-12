#pragma once

#ifndef __CUDACC__
# include <mitsuba/core/bbox.h>
#endif

namespace optix {
struct BoundingBox3f {
    BoundingBox3f() = default;
#ifndef __CUDACC__
    BoundingBox3f(const mitsuba::BoundingBox<mitsuba::Point<float, 3>> &b) {
        min[0] = b.min[0]; min[1] = b.min[1]; min[2] = b.min[2];
        max[0] = b.max[0]; max[1] = b.max[1]; max[2] = b.max[2];
    }
#endif
    float min[3];
    float max[3];
};
} // end namespace optix
