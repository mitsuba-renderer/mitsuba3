import drjit as dr
import mitsuba as mi


def create_single_triangle():
    m = mi.Mesh("tri", 3, 1)
    m.vertex_positions_buffer()[:] = [0.0, 0.0, 0.0, 1.0, 0.2, 0.0, 0.2, 1.0, 0.0]
    m.faces_buffer()[:] = [0, 1, 2]
    m.recompute_bbox()
    return m


def create_regular_tetrahedron():
    m = mi.Mesh("tetrahedron", 4, 4)
    params = mi.traverse(m)
    m.vertex_positions_buffer()[:] = [0, 0, 0, 0.8, 0.8, 0, 0.8, 0, 0.8, 0, 0.8, 0.8]
    m.faces_buffer()[:] = [0, 1, 2, 1, 2, 3, 0, 2, 2, 1, 3, 3, 3, 1, 0]
    m.recompute_bbox()
    return m


# Generate stairs in a 1x1x1 bbox, going up the Z axis along the X axis
def create_stairs(num_steps):
    size_step = 1.0 / num_steps

    m = mi.Mesh("stairs", 4 * num_steps, 4 * num_steps - 2)
    params = mi.traverse(m)

    v = dr.zero(mi.TensorXf, [4 * num_steps, 3])
    f = dr.zero(mi.TensorXf, [4 * num_steps - 2, 3])

    for i in range(num_steps):
        h  = i * size_step
        s1 = i * size_step
        s2 = (i + 1) * size_step
        k = 4 * i

        v[k + 0] = [0.0, s1, h]
        v[k + 1] = [1.0, s1, h]
        v[k + 2] = [0.0, s2, h]
        v[k + 3] = [1.0, s2, h]

        f[k]   = [k, k + 1, k + 2]
        f[k + 1] = [k + 1, k + 3, k + 2]
        if i < num_steps - 1:
            f[k + 2] = [k + 2, k + 3, k + 5]
            f[k + 3] = [k + 5, k + 4, k + 2]

    m.vertex_positions_buffer()[:] = v.array
    m.faces_buffer()[:] = f.array
    m.recompute_bbox()
    return m
