from mitsuba.core import Thread, FileResolver, Struct, float_dtype, Properties
from mitsuba.render import ShapeKDTree, Mesh

import numpy as np

# Some helper functions to generate simple meshes
vertex_struct = Struct() \
    .append("x", float_dtype) \
    .append("y", float_dtype) \
    .append("z", float_dtype)

index_struct = Struct() \
    .append("i0", Struct.EUInt32) \
    .append("i1", Struct.EUInt32) \
    .append("i2", Struct.EUInt32)


def create_single_triangle():
    m = Mesh("tri", vertex_struct, 3, index_struct, 1)
    v = m.vertices()
    f = m.faces()
    v[0] = np.array([0, 0, 0], dtype=float_dtype)
    v[1] = np.array([1, 0.2, 0], dtype=float_dtype)
    v[2] = np.array([0.2, 1, 0], dtype=float_dtype)
    f[0] = np.array([0, 1, 2], dtype=np.uint32)
    m.recompute_bbox()
    return m


def create_regular_tetrahedron():
    m = Mesh("tetrahedron", vertex_struct, 4, index_struct, 4)
    v = m.vertices()
    f = m.faces()

    v[0] = np.array([0, 0, 0], dtype=float_dtype)
    v[1] = np.array([0.8, 0.8, 0], dtype=float_dtype)
    v[2] = np.array([0.8, 0, 0.8], dtype=float_dtype)
    v[3] = np.array([0, 0.8, 0.8], dtype=float_dtype)

    f[0] = np.array([0, 1, 2], dtype=np.uint32)
    f[1] = np.array([2, 3, 0], dtype=np.uint32)
    f[2] = np.array([2, 1, 3], dtype=np.uint32)
    f[3] = np.array([3, 1, 0], dtype=np.uint32)

    m.recompute_bbox()
    return m


# Generate stairs in a 1x1x1 bbox, going up the Z axis along the X axis
def create_stairs(num_steps):
    size_step = 1.0 / num_steps

    m = Mesh("stairs", vertex_struct, 4 * num_steps,
             index_struct, 4 * num_steps - 2)
    v = m.vertices()
    f = m.faces()

    for i in range(num_steps):
        h  = i * size_step
        s1 = i * size_step
        s2 = (i + 1) * size_step
        k = 4 * i

        v[k + 0] = np.array([0.0, s1, h], dtype=float_dtype)
        v[k + 1] = np.array([1.0, s1, h], dtype=float_dtype)
        v[k + 2] = np.array([0.0, s2, h], dtype=float_dtype)
        v[k + 3] = np.array([1.0, s2, h], dtype=float_dtype)

        f[k]   = np.array([k, k + 1, k + 2], dtype=np.uint32)
        f[k + 1] = np.array([k + 1, k + 3, k + 2], dtype=np.uint32)
        if i < num_steps - 1:
            f[k + 2] = np.array([k + 2, k + 3, k + 5], dtype=np.uint32)
            f[k + 3] = np.array([k + 5, k + 4, k + 2], dtype=np.uint32)

    m.recompute_bbox()
    return m

# -----------------------------------------------------------------------------------------
