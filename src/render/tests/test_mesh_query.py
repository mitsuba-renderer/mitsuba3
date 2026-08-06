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

INVALID_DEDGE = 0xffffffff


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
    mesh.directed_edges()

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
            sh.directed_edges()
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
        lambda m, i, a: m.edge_indices(i, i, active=a),
        lambda m, i, a: m.vertex_position(i, active=a),
        lambda m, i, a: m.vertex_normal(i, active=a),
        lambda m, i, a: m.vertex_texcoord(i, active=a),
        lambda m, i, a: m.face_normal(i, active=a),
        lambda m, i, a: m.opposite_dedge(i, active=a),
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


@fresolver_append_path
def test07_vcalls_partial_dedges(variants_vec_rgb):
    """A vcall over a mix of meshes with and without half-edge adjacency is
    well defined: the ones that lack it report every edge as a boundary."""
    scene = mi.load_dict({
        "type": "scene",
        "mesh1": {
            "type": "ply",
            "filename": "resources/data/tests/ply/cbox_smallbox.ply",
        },
        "mesh2": {
            "type": "ply",
            "filename": "resources/data/tests/ply/cbox_smallbox.ply",
        },
    })

    # Only the first mesh gets the structure, and the accessor must not build
    # one for the second
    scene.shapes()[0].directed_edges()

    mesh_ptr = dr.gather(mi.MeshPtr, scene.shapes_dr(), mi.UInt32([0, 1, 0]))
    result = mesh_ptr.opposite_dedge(mi.UInt32([2, 3, 2]))
    assert dr.all(result == mi.UInt32([3, INVALID_DEDGE, 3]))
