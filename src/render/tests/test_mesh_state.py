"""
Tests of mesh state updates: what an edit through the parameter interface
refreshes (normals, tangents, bounding box, sampling tables, cached UV
orientation bits), the keys-driven normal regeneration rules, and
structural edits that resize the mesh.
"""

import numpy as np
import pytest
import drjit as dr
import mitsuba as mi

from mitsuba.scalar_rgb.test.util import fresolver_append_path, \
    vertex_positions, vertex_normals, anisotropic_bsdf, \
    assert_uniform_within_groups, quad_corners


def make_quad(uv=True, normals=None):
    """A unit quad built from fields, with optional UVs/authored normals"""
    positions = np.float32([[0, 0, 0], [1, 0, 0], [1, 1, 0], [0, 1, 0]])
    faces = np.uint32([[0, 1, 2], [0, 2, 3]])
    kwargs = {}
    if uv:
        kwargs['texcoords'] = np.float32([[0, 0], [1, 0], [1, 1], [0, 1]])
    if normals is not None:
        kwargs['normals'] = normals
    return mi.Mesh("quad", faces=faces, positions=positions, **kwargs)


def test01_normal_weighting_scheme(variant_scalar_rgb):
    """Generated normals average face normals weighted by the interior
    angle at each vertex (Thuermer and Wuethrich, JGT 1998)."""
    a, b = 1.0, 0.5
    m = mi.Mesh("MyMesh",
                faces=[[0, 1, 2], [0, 3, 4]],
                positions=[[0, 0, 0], [-a, 1, 0], [a, 1, 0],
                           [-b, 0, 1], [b, 0, 1]])

    n0 = mi.Vector3f(0.0, 0.0, -1.0)
    n1 = mi.Vector3f(0.0, 1.0, 0.0)
    angle_0 = dr.pi / 2.0
    angle_1 = dr.acos(3.0 / 5.0)
    n2 = n0 * angle_0 + n1 * angle_1
    n2 /= dr.norm(n2)
    n = np.vstack([n2, n0, n0, n1, n1])

    params = mi.traverse(m)
    assert np.allclose(np.array(params['normals']), n, atol=5e-4)


def test02_normal_regeneration_rules(variants_all_rgb):
    """Shading normals are keys-driven: a batch that writes positions
    without normals regenerates them (and rebuilds the area sampling
    table), a batch that includes normals adopts them, and a key-less
    refresh preserves everything."""
    m = make_quad(uv=False)
    dr.assert_allclose(m.vertex_normal(0), [0, 0, 1])
    dr.assert_allclose(m.surface_area(), 1)

    m.parameters_changed()
    dr.assert_allclose(m.vertex_normal(0), [0, 0, 1])

    params = mi.traverse(m)
    p = np.array(params['positions'])
    p[:, 2] = p[:, 0]  # tilt the quad
    params['positions'] = p
    params.update()

    assert np.allclose(vertex_normals(m),
                       [-np.sqrt(0.5), 0, np.sqrt(0.5)], atol=1e-6)
    dr.assert_allclose(m.surface_area(), np.sqrt(2))

    # A batch that includes 'normals' adopts them, even alongside a
    # position write
    normals = np.linspace(-1, 1, 12, dtype=np.float32).reshape(4, 3)
    params['normals'] = normals
    params['positions'] = p + 0.5
    params.update()
    assert np.allclose(np.array(mi.traverse(m)['normals']), normals,
                       atol=1e-6)

    # A later positions-only write regenerates them again
    params['positions'] = p
    params.update()
    assert np.allclose(vertex_normals(m),
                       [-np.sqrt(0.5), 0, np.sqrt(0.5)], atol=1e-6)

    # So does a topology-only write: flipping the winding flips the
    # regenerated normals
    params['faces'] = np.uint32([[0, 2, 1], [0, 3, 2]])
    params.update()
    assert np.allclose(vertex_normals(m),
                       [np.sqrt(0.5), 0, -np.sqrt(0.5)], atol=1e-6)


def test03_recompute_preserves_grouping(variant_scalar_rgb):
    """Regeneration accumulates per normal group, so an authored grouping
    (e.g. a hard edge) survives a position edit."""
    positions, corner_vertex, uv = quad_corners(seam=True)
    normals = np.zeros((6, 3), dtype=np.float32)
    normals[0:3] = (0, 0, 1)
    normals[3:6] = (0, 1, 0)

    m = mi.Mesh("quad")
    m.from_corners(positions=positions, corner_vertex=corner_vertex,
                   normals=normals, texcoords=uv)
    assert m.normal_count() == 6

    # Bend the quad along the UV seam, whose two sides share their normal
    # groups and must regenerate to one common value per group
    params = mi.traverse(m)
    p = np.array(params['positions'])
    p[:, 2] = p[:, 0] * p[:, 1]
    params['positions'] = p
    params.update()

    assert m.normal_count() == 6
    n = vertex_normals(m)
    assert not np.allclose(n, normals, atol=1e-2)  # regeneration happened
    assert_uniform_within_groups(n, np.array(m.normal_index()))


def test04_uv_edit_refreshes_tangents(variant_scalar_rgb):
    """A texture coordinate edit refreshes the generated tangents.
    Rotating the UV field by 90 degrees turns the tangent accordingly."""
    m = make_quad()
    assert np.allclose(np.array(m.tangents()), [1, 0, 0], atol=1e-6)

    params = mi.traverse(m)
    old = np.array(params['texcoords'])
    params['texcoords'] = np.stack([-old[:, 1], old[:, 0]], axis=-1)
    params.update()
    assert np.allclose(np.array(m.tangents()), [0, -1, 0], atol=1e-6)


def test05_views_stay_symbolic(variant_llvm_ad_rgb):
    """Every view is a symbolic derivation of the packed buffer that
    remains unevaluated until a consumer materializes it, including after
    a repack."""
    positions, corner_vertex, uv = quad_corners(seam=True)
    m = mi.Mesh("quad")
    m.from_corners(positions=positions, corner_vertex=corner_vertex,
                 texcoords=uv)
    m.set_bsdf(anisotropic_bsdf())

    def check_symbolic():
        for view in (m.positions(), m.normals(), m.texcoords(), m.faces(),
                     m.tangents()):
            assert view.array.state == dr.VarState.Unevaluated
        # No per-face materials: the view stays empty
        assert dr.width(m.bsdf_index()) == 0

    mi.traverse(m)
    check_symbolic()

    # A repack rebinds the views; they stay symbolic
    m.parameters_changed(['texcoords'])
    check_symbolic()

    # Materializing yields unit vectors in the plane of the vertex normals
    tn = mi.Vector3f(m.tangents(), flip_axes=True)
    nrm = dr.gather(mi.Vector3f, m.normals().array, m.normal_index())
    dr.assert_allclose(dr.norm(tn), 1, atol=1e-6)
    assert dr.all(dr.abs(dr.dot(tn, nrm)) < 1e-5)


def test06_write_rules(variants_all_rgb):
    """The index fields accept writes at matching counts, and an
    inconsistent batch raises an error naming the missing companion key."""
    m = make_quad()
    params = mi.traverse(m)

    # Index fields accept writes at unchanged counts
    faces = np.array(params['faces'])
    params['faces'] = faces[[1, 0]].copy()
    params['bsdf_index'] = np.uint32([0, 0])
    params.update()
    assert np.all(np.array(m.faces()) == faces[[1, 0]])

    # Writing 'normal_index' requires 'normals' in the same batch, since
    # the group count is only knowable from the normals tensor
    params = mi.traverse(make_quad())
    with pytest.raises(Exception, match="'normals' in the same batch"):
        params['normal_index'] = np.uint32([0, 0, 1, 2])
        params.update()

    # A write that changes the vertex count must update every per-vertex
    # field in the same batch
    params = mi.traverse(make_quad())
    with pytest.raises(Exception, match="'texcoords' has 4 rows"):
        params['positions'] = np.zeros((5, 3), dtype=np.float32)
        params.update()


@fresolver_append_path
def test07_update_geometry_accel(variants_vec_rgb):
    """Position edits propagate to the ray tracing acceleration
    structure."""
    scene = mi.load_dict({
        'type': 'scene',
        'rect': {
            'type': 'ply',
            'id': 'rect',
            'filename': 'resources/data/tests/ply/rectangle_normals_uv.ply'
        }
    })

    params = mi.traverse(scene)
    init_pos = mi.Point3f(params['rect.positions'], flip_axes=True)

    def translate(v):
        transform = mi.Transform4f().translate(mi.Vector3f(v))
        params['rect.positions'] = mi.TensorXf(transform @ init_pos,
                                               flip_axes=True)
        params.update()

    film_size = mi.ScalarVector2i([4, 4])
    total_sample_count = dr.prod(film_size)
    pos = dr.arange(mi.UInt32, total_sample_count)
    pos = mi.Vector2f(mi.Float(pos % int(film_size[0])),
                      mi.Float(pos // int(film_size[0])))
    pos = 2.0 * (pos / (film_size - 1.0) - 0.5)

    ray = mi.Ray3f([pos[0], -5, pos[1]], [0, 1, 0])
    init_t = scene.ray_intersect_preliminary(ray, coherent=True).t
    dr.eval(init_t)

    for v in ([0, 0, 10], [-5, 0, 10]):
        translate(v)
        ray.o += v
        t = scene.ray_intersect_preliminary(ray, coherent=True).t
        ray.o -= v
        dr.assert_allclose(t, init_t)

    # Structural phase: remesh through the parameter map. Fanning every
    # face into three around its centroid leaves the surface unchanged
    # but resizes all buffers; the per-face material assignment written
    # below must be rewritten in the same batch.
    translate([0, 0, 0])
    params['rect.bsdf_index'] = np.uint32([7, 9])
    params.update()

    faces = np.array(params['rect.faces'])
    pos = np.array(params['rect.positions'])
    uv = np.array(params['rect.texcoords'])
    V, F = pos.shape[0], faces.shape[0]

    dr.enable_grad(params['rect.positions'])
    assert dr.grad_enabled(params['rect.positions'])

    center = np.arange(V, V + F, dtype=np.uint32)
    fan = np.concatenate([np.stack([faces[:, k], faces[:, (k + 1) % 3],
                                    center], axis=1) for k in range(3)])
    fan_pos = np.vstack([pos, pos[faces].mean(axis=1)])
    fan_uv = np.vstack([uv, uv[faces].mean(axis=1)])
    fan_bsdf = np.uint32(np.arange(3 * F) % 2)

    params['rect.faces'] = fan
    params['rect.positions'] = fan_pos
    params['rect.texcoords'] = fan_uv
    with pytest.raises(RuntimeError, match="'bsdf_index' has 2 entries"):
        params.update()

    params['rect.faces'] = fan
    params['rect.positions'] = fan_pos
    params['rect.texcoords'] = fan_uv
    params['rect.bsdf_index'] = fan_bsdf
    params.update()

    # The old handles remained valid across the repack; the AD identity
    # of the resized tensor was dropped
    assert params['rect.positions'].shape == (V + F, 3)
    assert not dr.grad_enabled(params['rect.positions'])
    assert np.all(np.array(params['rect.bsdf_index']) == fan_bsdf)

    t = scene.ray_intersect_preliminary(ray, coherent=True).t
    dr.assert_allclose(t, init_t)


def test08_scalar_and_jit_kernels_agree(variants_vec_rgb):
    """The scalar and JIT implementations of the generation kernels
    (smooth normals, tangents, UV flip bits) produce matching results."""
    # A bent quad with a UV seam so that maps, averaging and orientation
    # bits are all in play
    positions = np.float32([[0, 0, 0.1], [1, 0, -0.2], [1, 1, 0.3],
                            [0, 1, 0]])
    faces = np.uint32([[0, 1, 2], [3, 4, 5]])
    pidx = np.uint32([0, 1, 2, 0, 2, 3])
    uv = np.float32([[0, 0], [1, 0], [1, 1], [5, 5], [6, 7], [5, 6]])

    def build_and_snapshot():
        m = mi.Mesh("m")
        m.from_fields(faces=faces, position_index=pidx,
                      positions=positions, texcoords=uv)
        m.set_bsdf(anisotropic_bsdf())
        assert m.packs_tangent()
        return {
            'positions': np.array(m.positions()),
            'normals': np.array(m.normals()),
            'texcoords': np.array(m.texcoords()),
            'tangents': np.array(m.tangents()),
            'packed': np.array(m.packed_vertices()),
            'faces': np.array(mi.traverse(m)['faces']),
            'bsdf_index': np.array(m.bsdf_index()),
        }

    jit = build_and_snapshot()
    variant = mi.variant()
    mi.set_variant('scalar_rgb')
    try:
        scalar = build_and_snapshot()
    finally:
        mi.set_variant(variant)

    assert np.array_equal(jit['positions'], scalar['positions'])
    assert np.array_equal(jit['texcoords'], scalar['texcoords'])
    assert np.allclose(jit['normals'], scalar['normals'], atol=1e-5)
    assert np.allclose(jit['tangents'], scalar['tangents'], atol=1e-5)
    assert np.allclose(jit['packed'], scalar['packed'], atol=1e-5)
    assert np.array_equal(jit['faces'], scalar['faces'])
    assert np.array_equal(jit['bsdf_index'], scalar['bsdf_index'])


def test09_transform(variants_all_rgb):
    """Mesh.transform() maps positions through the matrix and normals
    through its inverse transpose, transforming encoded tangent frames."""
    m = make_quad()
    m.set_bsdf(anisotropic_bsdf())
    assert m.packs_tangent()
    t_before = np.array(m.tangents())

    m.transform(mi.AffineTransform4f().translate([1, 2, 3]).scale([1, 2, 4]))

    p_ref = np.float32([[0, 0, 0], [1, 0, 0], [1, 1, 0], [0, 1, 0]])
    assert np.allclose(vertex_positions(m),
                       p_ref * np.float32([1, 2, 4]) + [1, 2, 3], atol=1e-6)
    # The inverse transpose scales (0, 0, 1) to (0, 0, 1/4); renormalized
    assert np.allclose(vertex_normals(m), [0, 0, 1], atol=1e-6)
    # The (1, 0, 0) tangents are invariant under this transform
    assert np.allclose(np.array(m.tangents()), t_before, atol=1e-5)
    dr.assert_allclose(m.bbox().min, [1, 2, 3])
    dr.assert_allclose(m.bbox().max, [2, 4, 3])
