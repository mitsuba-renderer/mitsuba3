#include <mitsuba/render/mesh.h>

NAMESPACE_BEGIN(mitsuba)

Mesh::~Mesh() { }

BoundingBox3f Mesh::bbox() const {
    return m_bbox;
}

BoundingBox3f Mesh::bbox(Index index) const {
    using simd::min;
    using simd::max;
    Assert(index <= m_faceCount);

    auto idx = (const Index *) face(index);
    Assert(idx[0] < m_vertexCount);
    Assert(idx[1] < m_vertexCount);
    Assert(idx[2] < m_vertexCount);

    Point3f v0, v1, v2;
    v0.loadUnaligned((float *) vertex(idx[0]));
    v1.loadUnaligned((float *) vertex(idx[1]));
    v2.loadUnaligned((float *) vertex(idx[2]));

    return BoundingBox3f(
        min(min(v0, v1), v2),
        max(max(v0, v1), v2)
    );
}


namespace {
    constexpr size_t maxVertices = 8;

    size_t sutherlandHodgman(Point3d *input, size_t inCount, Point3d *output,
                                    int axis, double splitPos, bool isMinimum) {
        if (inCount < 3)
            return 0;

        Point3d cur       = input[0];
        double sign       = isMinimum ? 1.0 : -1.0;
        double distance   = sign * (cur[axis] - splitPos);
        bool  curIsInside = (distance >= 0);
        size_t outCount    = 0;

        for (size_t i=0; i<inCount; ++i) {
            size_t nextIdx = i+1;
            if (nextIdx == inCount)
                nextIdx = 0;

            Point3d next = input[nextIdx];
            distance = sign * (next[axis] - splitPos);
            bool nextIsInside = (distance >= 0);

            if (curIsInside && nextIsInside) {
                /* Both this and the next vertex are inside, add to the list */
                Assert(outCount + 1 < maxVertices);
                output[outCount++] = next;
            } else if (curIsInside && !nextIsInside) {
                /* Going outside -- add the intersection */
                double t = (splitPos - cur[axis]) / (next[axis] - cur[axis]);
                Assert(outCount + 1 < maxVertices);
                Point3d p = cur + (next - cur) * t;
                p[axis] = splitPos; // Avoid roundoff errors
                output[outCount++] = p;
            } else if (!curIsInside && nextIsInside) {
                /* Coming back inside -- add the intersection + next vertex */
                double t = (splitPos - cur[axis]) / (next[axis] - cur[axis]);
                Assert(outCount + 2 < maxVertices);
                Point3d p = cur + (next - cur) * t;
                p[axis] = splitPos; // Avoid roundoff errors
                output[outCount++] = p;
                output[outCount++] = next;
            } else {
                /* Entirely outside - do not add anything */
            }
            cur = next;
            curIsInside = nextIsInside;
        }
        return outCount;
    }
}

BoundingBox3f Mesh::bbox(Index index, const BoundingBox3f &clip) const {
	using simd::cast;
	using simd::cast_up;
	using simd::cast_down;

	/* Reserve room for some additional vertices */
	Point3d vertices1[maxVertices], vertices2[maxVertices];
	size_t nVertices = 3;

    Assert(index <= m_faceCount);

    auto idx = (const Index *) face(index);
    Assert(idx[0] < m_vertexCount);
    Assert(idx[1] < m_vertexCount);
    Assert(idx[2] < m_vertexCount);

    Point3f v0, v1, v2;
    v0.loadUnaligned((float *) vertex(idx[0]));
    v1.loadUnaligned((float *) vertex(idx[1]));
    v2.loadUnaligned((float *) vertex(idx[2]));

    /* The kd-tree code will frequently call this function with
       almost-collapsed bounding boxes. It's extremely important not to
       introduce errors in such cases, otherwise the resulting tree will
       incorrectly remove triangles from the associated nodes. Hence, do
       the following computation in double precision! */

	vertices1[0] = cast<Point3d>(v0);
	vertices1[1] = cast<Point3d>(v1);
	vertices1[2] = cast<Point3d>(v2);

	for (int axis=0; axis<3; ++axis) {
        nVertices = sutherlandHodgman(vertices1, nVertices, vertices2, axis,
                                      (double) clip.min[axis], true);
        nVertices = sutherlandHodgman(vertices2, nVertices, vertices1, axis,
                                      (double) clip.max[axis], false);
    }

    BoundingBox3f result;
    for (size_t i = 0; i < nVertices; ++i) {
        result.min = min(result.min, cast_down<Point3f>(vertices1[i]));
        result.max = max(result.max, cast_up  <Point3f>(vertices1[i]));
    }
    result.clip(clip);

    return result;
}

Shape::Size Mesh::primitiveCount() const {
    return faceCount();
}

MTS_IMPLEMENT_CLASS(Mesh, Shape)
NAMESPACE_END(mitsuba)
