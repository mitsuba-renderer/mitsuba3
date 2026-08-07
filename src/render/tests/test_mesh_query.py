"""
Tests of mesh silhouette sampling, and of the MeshPtr pointer type and its
vectorized calls. The half-edge adjacency itself is covered by
test_dedge.py.
"""

import numpy as np
import pytest
import drjit as dr
import mitsuba as mi

from mitsuba.scalar_rgb.test.util import fresolver_append_path, \
    vertex_positions


@fresolver_append_path
def load_triangle():
    return mi.load_dict({
        "type": "ply",
        "filename": "resources/data/tests/ply/triangle.ply",
        "face_normals": True
    })


@fresolver_append_path
def mixed_shapes_scene():
    return mi.load_dict({
        "type": "scene",
        "shape1": {
            "type": "ply",
            "filename": "resources/data/tests/ply/rectangle_uv.ply",
        },
        "shape2": {
            "type": "sphere",
        },
        "shape3": {
            "type": "ply",
            "filename": "resources/data/tests/ply/rectangle_uv.ply",
            "flip_normals": True,
        },
    }, parallel=False, optimize=False)


# -------------------------------------------------------------------
# Silhouette sampling
# -------------------------------------------------------------------

def test01_discontinuity_types(variants_vec_rgb):
    """Meshes only expose perimeter-type silhouette discontinuities."""
    mesh = load_triangle()
    types = mesh.silhouette_discontinuity_types()
    assert not mi.has_flag(types, mi.DiscontinuityFlags.InteriorType)
    assert mi.has_flag(types, mi.DiscontinuityFlags.PerimeterType)

    # Requesting the unsupported interior type yields invalid samples
    ss = mesh.sample_silhouette([0.1, 0.2, 0.3],
                                mi.DiscontinuityFlags.InteriorType)
    assert ss.discontinuity_type == mi.DiscontinuityFlags.Empty.value


@pytest.mark.parametrize("direction", ["sphere", "lune"])
def test02_sample_silhouette(variants_vec_rgb, direction):
    """Perimeter samples lie on the triangle boundary and are distributed
    with the expected density."""
    if not dr.is_diff_v(mi.Float):
        pytest.skip("Only relevant in AD-enabled variants!")

    mesh = load_triangle()
    mesh_ptr = mi.ShapePtr(mesh)

    params = mi.traverse(mesh)
    dr.enable_grad(params['positions'])
    params.update()

    x = dr.linspace(mi.Float, 1e-6, 1 - 1e-6, 10)
    samples = mi.Point3f(dr.meshgrid(x, x, x))

    flag = (mi.DiscontinuityFlags.DirectionSphere if direction == "sphere"
            else mi.DiscontinuityFlags.DirectionLune)
    flags = mi.DiscontinuityFlags.PerimeterType | flag
    ss = mesh.sample_silhouette(samples, flags)

    if direction == "lune":
        valid = ss.is_valid()
        ss = dr.gather(mi.SilhouetteSample3f, ss, dr.compress(valid))

    assert dr.all(ss.discontinuity_type ==
                  mi.DiscontinuityFlags.PerimeterType.value)
    assert dr.all(ss.p.x == 0)
    assert dr.all((ss.p.y <= 1) & (ss.p.y >= 0) &
                  (ss.p.z <= 1) & (ss.p.z >= 0))
    assert dr.all(ss.flags == flags)
    dr.assert_allclose(dr.dot(ss.n, ss.d), 0, atol=1e-6)
    perimeter = 2 + dr.sqrt(2)
    dr.assert_allclose(ss.pdf, dr.rcp(perimeter) * dr.inv_four_pi)
    assert dr.all(dr.reinterpret_array(mi.UInt32, ss.shape) ==
                  dr.reinterpret_array(mi.UInt32, mesh_ptr))


@fresolver_append_path
@pytest.mark.parametrize("direction", ["sphere", "lune"])
def test03_sample_silhouette_bijective(variants_vec_rgb, direction):
    """invert_silhouette_sample() inverts sample_silhouette() on the unit
    cube of sample values."""
    if not dr.is_diff_v(mi.Float):
        pytest.skip("Only relevant in AD-enabled variants!")

    mesh = mi.load_dict({
        "type": "ply",
        "filename": "resources/data/common/meshes/bunny_lowres.ply",
    })

    params = mi.traverse(mesh)
    dr.enable_grad(params['positions'])
    params.update()

    x = dr.linspace(mi.Float, 1e-3, 1 - 1e-3, 10)
    samples = mi.Point3f(dr.meshgrid(x, x, x))

    flag = (mi.DiscontinuityFlags.DirectionSphere if direction == "sphere"
            else mi.DiscontinuityFlags.DirectionLune)
    ss = mesh.sample_silhouette(samples,
                                mi.DiscontinuityFlags.PerimeterType | flag)
    out = mesh.invert_silhouette_sample(ss)
    valid = ss.is_valid()
    samples_valid = dr.gather(mi.Point3f, samples, dr.compress(valid))
    out_valid = dr.gather(mi.Point3f, out, dr.compress(valid))

    dr.assert_allclose(samples_valid.x, out_valid.x, atol=1e-7)
    if direction == "sphere":
        dr.assert_allclose(samples_valid.y, out_valid.y, atol=1e-7)
    else:
        # Lune sampling is not numerically robust
        dr.assert_allclose(samples_valid.y, out_valid.y, atol=1e-4)
    dr.assert_allclose(samples_valid.z, out_valid.z, atol=1e-7)


@fresolver_append_path
def test04_primitive_silhouette_projection(variants_vec_rgb):
    """primitive_silhouette_projection() moves interactions onto
    perimeter edges."""
    mesh = mi.load_dict({
        "type": "ply",
        "filename": "resources/data/tests/ply/rectangle_uv.ply",
    })

    # Only differentiated shapes have a silhouette to project onto
    params = mi.traverse(mesh)
    dr.enable_grad(params['positions'])
    params.update()

    u = dr.linspace(mi.Float, 1e-6, 1 - 1e-6, 10)
    uv = mi.Point2f(dr.meshgrid(u, u))
    si = mesh.eval_parameterization(uv)

    viewpoint = mi.Point3f(0, 0, 5)
    sample = dr.linspace(mi.Float, 1e-6, 1 - 1e-6, dr.width(uv))
    ss = mesh.primitive_silhouette_projection(
        viewpoint, si, mi.DiscontinuityFlags.PerimeterType, sample,
        si.is_valid())

    valid = ss.is_valid()
    ss = dr.gather(mi.SilhouetteSample3f, ss, dr.compress(valid))

    assert dr.all(ss.discontinuity_type ==
                  mi.DiscontinuityFlags.PerimeterType.value)
    dr.assert_allclose(dr.dot(ss.n, ss.d), 0, atol=1e-6)

    mesh_ptr = mi.ShapePtr(mesh)
    assert dr.all(dr.reinterpret_array(mi.UInt32, ss.shape) ==
                  dr.reinterpret_array(mi.UInt32, mesh_ptr))


def test05_sample_precomputed_silhouette(variants_vec_rgb):
    """sample_precomputed_silhouette() places samples along the directed
    edge that its first sample selects."""
    mesh = load_triangle()
    p = vertex_positions(mesh)

    # The triangle lies in the plane x = 0, so the viewpoint has to leave it
    # for the silhouette normal to be well defined
    viewpoint = mi.Point3f(2, 0.3, 0.4)
    dedge = dr.arange(mi.UInt32, 3)
    u = mi.Float(0.25, 0.5, 0.75)
    ss = mesh.sample_precomputed_silhouette(viewpoint, dedge, u)

    # Directed edge e of face 0 runs from vertex e to vertex (e + 1) % 3
    a, b = p[[0, 1, 2]], p[[1, 2, 0]]
    edge = b - a
    dr.assert_allclose(ss.p, mi.Point3f((a + np.array(u)[:, None] * edge).T))
    dr.assert_allclose(ss.silhouette_d,
                       mi.Vector3f((edge / np.linalg.norm(
                           edge, axis=1, keepdims=True)).T))
    dr.assert_allclose(ss.pdf, mi.Float(1 / np.linalg.norm(edge, axis=1)))
    dr.assert_allclose(dr.dot(ss.n, ss.d), 0, atol=1e-6)
    assert dr.all(ss.prim_index == 0)
    assert dr.all(ss.discontinuity_type ==
                  mi.DiscontinuityFlags.PerimeterType.value)


# -------------------------------------------------------------------
# MeshPtr and vcalls
# -------------------------------------------------------------------

def test06_mesh_ptr_and_vcalls(variants_vec_rgb):
    """MeshPtr construction zeroes out non-mesh entries, and vectorized
    Mesh method calls agree with direct calls on the individual
    instances."""
    scene = mixed_shapes_scene()
    shapes = scene.shapes_dr()

    expected_ordering = [("shape1", True), ("shape2", False),
                         ("shape3", True)]
    for i, sh in enumerate(scene.shapes()):
        assert (sh.id(), sh.is_mesh()) == expected_ordering[i]
        as_mesh = mi.MeshPtr(sh)
        if sh.is_mesh():
            assert sh.shape_type() == mi.ShapeType.Mesh
            assert dr.all(dr.gather(mi.MeshPtr, shapes, i) == as_mesh)
            assert as_mesh[0] == shapes[i] and as_mesh[0] == sh
            sh.dedge()
        else:
            assert dr.all(dr.reinterpret_array(mi.UInt32, as_mesh) == 0)
            assert as_mesh[0] is None

    # The ``MeshPtr`` constructor should automatically zero-out non-Mesh
    # entries, so the mask below is not strictly needed
    meshes = mi.MeshPtr(shapes)
    assert dr.all((dr.reinterpret_array(mi.UInt32, meshes) != 0) ==
                  (True, False, True))
    active = shapes.is_mesh()
    assert dr.all(active == mi.Mask([True, False, True]))
    assert dr.count(shapes.shape_type() == mi.ShapeType.Mesh.value) == 2

    # Shapes in the scene are: Mesh, Rectangle, Mesh
    assert dr.all(meshes.vertex_count() == [4, 0, 4])
    assert dr.all(meshes.face_count() == [2, 0, 2])
    assert dr.all(meshes.has_normals() == active)
    assert dr.all(meshes.has_texcoords() == active)
    assert not dr.any(meshes.has_mesh_attributes())
    assert not dr.any(meshes.has_face_normals())

    # 'shape3' loaded the same file with 'flip_normals', which the build
    # baked into the winding
    dr.assert_allclose(meshes.face_normal(mi.UInt32(0), active),
                       mi.Vector3f([0, 0, 0], [1, 0, -1], [0, 0, 0]))

    idx = mi.UInt32([0, 99, 1])
    getters = [
        lambda m, i, a: m.face_indices(i, active=a),
        lambda m, i, a: m.dedge_indices(i, active=a),
        lambda m, i, a: m.vertex_position(i, active=a),
        lambda m, i, a: m.vertex_normal(i, active=a),
        lambda m, i, a: m.vertex_texcoord(i, active=a),
        lambda m, i, a: m.face_normal(i, active=a),
        lambda m, i, a: m.dedge_opposite(i, active=a),
        lambda m, i, a: m.dedge_vertex_edge(i, active=a),
        lambda m, i, a: m.dedge_vertex_valence(i, active=a),
        lambda m, i, a: m.dedge_vertex_flags(i, active=a),
    ]
    results = [g(meshes, idx, active) for g in getters]
    dr.schedule(*results)

    # Check the vcall results against direct calls on the individual Mesh
    # instances
    for i, sh in enumerate(scene.shapes()):
        if not sh.is_mesh():
            continue
        idx_i = dr.gather(mi.UInt32, idx, i)
        for g, res in zip(getters, results):
            direct = g(sh, idx_i, True)
            assert dr.all(dr.gather(type(res), res, i) == direct)

    # It should be possible to construct an empty MeshPtr, and to reshape
    # one down to zero elements
    assert dr.width(mi.ShapePtr()) == 0
    assert dr.width(mi.MeshPtr()) == 0
    assert dr.width(dr.zeros(mi.MeshPtr, 0)) == 0
    assert dr.width(dr.empty(mi.MeshPtr, 0)) == 0
    assert dr.width(dr.reshape(mi.MeshPtr, dr.zeros(mi.MeshPtr, 4), 0,
                               shrink=True)) == 0


def strip_mesh(name="strip", n=5):
    """A triangle strip with ``n`` faces, whose interior edges are paired."""
    positions = np.float32([[i, i % 2, 0] for i in range(n + 2)])
    faces = np.uint32([[i, i + 1, i + 2] if i % 2 == 0 else [i + 1, i, i + 2]
                       for i in range(n)])
    return mi.Mesh(name, mi.TensorXu32(faces), mi.TensorXf32(positions))


def loose_mesh(name="loose", n=5):
    """``n`` disconnected triangles: the same half-edge count as
    ``strip_mesh(n)``, but every edge is a boundary."""
    positions = np.float32([[t + (k == 1), k == 2, 0]
                            for t in range(n) for k in range(3)])
    faces = np.uint32(np.arange(3 * n).reshape(n, 3))
    return mi.Mesh(name, mi.TensorXu32(faces), mi.TensorXf32(positions))


@pytest.mark.parametrize("region", ["vcall", "loop"])
def test07_dedge_built_in_symbolic_region(variants_vec_rgb, region):
    """The adjacency is built on first use, even when that first use is traced
    inside a symbolic region. Dr.Jit forbids evaluation there, so Mesh::dedge()
    escapes it via dr::scoped_eval_scope."""
    n_he = 15
    e = dr.arange(mi.UInt32, n_he)
    ref_strip = mi.UInt32(strip_mesh("ref_strip").dedge_opposite(e))
    ref_loose = mi.UInt32(loose_mesh("ref_loose").dedge_opposite(e))
    dr.eval(ref_strip, ref_loose)
    assert dr.any(ref_strip != ref_loose)  # the two must be distinguishable

    if region == "vcall":
        # Two fresh meshes, so the call has to build both while it is recorded,
        # and must keep the per-instance results apart
        strip, loose = strip_mesh(), loose_mesh()
        is_strip = (e % 2) == 0
        ptr = dr.zeros(mi.MeshPtr, n_he)
        dr.scatter(ptr, mi.MeshPtr(strip), dr.compress(is_strip))
        dr.scatter(ptr, mi.MeshPtr(loose), dr.compress(~is_strip))
        assert dr.all(ptr.dedge_opposite(e) ==
                      dr.select(is_strip, ref_strip, ref_loose))
    else:
        @dr.syntax
        def accumulate(mesh, n):
            i, acc = mi.UInt32(0), mi.UInt32(0)
            while i < n:
                o = mesh.dedge_opposite(i)
                acc += dr.select(o == mi.DirectedEdge.Invalid, mi.UInt32(0), o)
                i += 1
            return acc

        want = dr.sum(dr.select(ref_strip == mi.DirectedEdge.Invalid,
                                mi.UInt32(0), ref_strip))
        assert dr.all(accumulate(strip_mesh(), mi.UInt32(n_he)) ==
                      mi.UInt32(want))
