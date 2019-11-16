from mitsuba.scalar_rgb.core import Thread, FileResolver, Struct, float_dtype, Properties
from mitsuba.scalar_rgb.render import ShapeKDTree, Mesh

import numpy as np

# Some helper functions to generate simple meshes
vertex_struct = Struct() \
    .append("x", float_dtype) \
    .append("y", float_dtype) \
    .append("z", float_dtype)
vdt = vertex_struct.dtype()

index_struct = Struct() \
    .append("i0", FieldType.UInt32) \
    .append("i1", FieldType.UInt32) \
    .append("i2", FieldType.UInt32)
idt = vertex_struct.dtype()


def create_single_triangle():
    m = Mesh("tri", vertex_struct, 3, index_struct, 1)
    v = m.vertices()
    f = m.faces()
    v[0] = (0, 0, 0)
    v[1] = (1, 0.2, 0)
    v[2] = (0.2, 1, 0)
    f[0] = (0, 1, 2)
    m.recompute_bbox()
    return m


def create_regular_tetrahedron():
    m = Mesh("tetrahedron", vertex_struct, 4, index_struct, 4)
    v = m.vertices()
    f = m.faces()

    v[0] = (0, 0, 0)
    v[1] = (0.8, 0.8, 0)
    v[2] = (0.8, 0, 0.8)
    v[3] = (0, 0.8, 0.8)

    f[0] = (0, 1, 2)
    f[1] = (2, 3, 0)
    f[2] = (2, 1, 3)
    f[3] = (3, 1, 0)

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

        v[k + 0] = (0.0, s1, h)
        v[k + 1] = (1.0, s1, h)
        v[k + 2] = (0.0, s2, h)
        v[k + 3] = (1.0, s2, h)

        f[k]   = (k, k + 1, k + 2)
        f[k + 1] = (k + 1, k + 3, k + 2)
        if i < num_steps - 1:
            f[k + 2] = (k + 2, k + 3, k + 5)
            f[k + 3] = (k + 5, k + 4, k + 2)

    m.recompute_bbox()
    return m

# -----------------------------------------------------------------------------------------
