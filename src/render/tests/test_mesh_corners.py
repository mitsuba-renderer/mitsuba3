import pytest
import numpy as np
import mitsuba as mi

MISSING = np.uint32(0xffffffff)


def make_cube():
    """8 vertices, 12 triangles. Returns (positions, corner_vertex, quad id
    and quad-local corner id per corner)."""
    p = np.array([
        [0, 0, 0], [1, 0, 0], [1, 1, 0], [0, 1, 0],
        [0, 0, 1], [1, 0, 1], [1, 1, 1], [0, 1, 1]], np.float32)
    quads = [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4),
             (2, 3, 7, 6), (0, 4, 7, 3), (1, 2, 6, 5)]
    cv, quad_id, local = [], [], []
    for q, (a, b, c, d) in enumerate(quads):
        for tri in ((a, b, c), (a, c, d)):
            cv += list(tri)
        quad_id += [q] * 6
        local += [0, 1, 2, 0, 2, 3]
    return (p, np.array(cv, np.uint32), np.array(quad_id, np.uint32),
            np.array(local, np.uint32))


def make_grid(n=8):
    """Regular grid: (n+1)^2 vertices, 2*n^2 triangles."""
    x, y = np.meshgrid(np.arange(n + 1), np.arange(n + 1))
    p = np.stack([x.ravel(), y.ravel(), np.zeros((n + 1) ** 2)],
                 axis=1).astype(np.float32)
    cv = []
    for i in range(n):
        for j in range(n):
            v00 = i * (n + 1) + j
            v01, v10, v11 = v00 + 1, v00 + n + 1, v00 + n + 2
            cv += [v00, v01, v11, v00, v11, v10]
    return p, np.array(cv, np.uint32)


def check_consistency(m, positions, cv, corner_uv=None):
    """The output, gathered through the face buffer, must reproduce the
    per-corner input."""
    faces = np.array(m.faces_buffer())
    out_p = np.array(m.vertex_positions_buffer()).reshape(-1, 3)
    assert np.array_equal(out_p[faces], positions[cv])
    if corner_uv is not None:
        out_uv = np.array(m.vertex_texcoords_buffer()).reshape(-1, 2)
        assert np.array_equal(out_uv[faces], corner_uv)


def test01_uv_seam_cube(variant_scalar_rgb):
    p, cv, quad_id, local = make_cube()

    # One UV island per quad: corners weld within a quad but not across
    island_uv = np.array([[0, 0], [1, 0], [1, 1], [0, 1]], np.float32)
    uv = (island_uv[local] + np.stack(
        [quad_id, np.zeros_like(quad_id)], axis=1)).astype(np.float32)

    # Smooth per-vertex normals in index form: they never split vertices
    vn = p - 0.5
    vn /= np.linalg.norm(vn, axis=1, keepdims=True)

    m = mi.Mesh.from_corners('cube', p, cv, normals=(vn, cv), texcoords=uv)

    # Each vertex borders three UV islands
    assert m.vertex_count() == 24
    assert m.face_count() == 12
    check_consistency(m, p, cv, uv)


def test02_smooth_grid_welds_to_vertex_count(variant_scalar_rgb):
    p, cv = make_grid()
    n = np.tile(np.array([0, 0, 1], np.float32), (len(cv), 1))
    uv = (p[:, :2] / p[:, :2].max()).astype(np.float32)

    m = mi.Mesh.from_corners('grid', p, cv, normals=n, texcoords=(uv, cv))

    # Everything agrees, so the weld recovers the source vertices
    assert m.vertex_count() == len(p)
    check_consistency(m, p, cv, uv[cv])


def test03_flat_shaded_splits_per_face(variant_scalar_rgb):
    p, cv = make_grid()
    tri_n = np.zeros((len(cv) // 3, 3), np.float32)
    tri_n[:, 0] = np.arange(len(tri_n))  # distinct normal per triangle
    tri_n[:, 2] = 1
    n = np.repeat(tri_n, 3, axis=0)

    m = mi.Mesh.from_corners('grid', p, cv, normals=n)

    # No two corners of a vertex agree, hence every corner splits
    assert m.vertex_count() == len(cv)
    check_consistency(m, p, cv)


def test04_color_discontinuity_not_merged(variant_scalar_rgb):
    # Two triangles sharing the edge (1, 2). All attributes but the color
    # agree, so only the color discontinuity keeps the edge vertices split.
    p = np.array([[0, 0, 0], [1, 0, 0], [0, 1, 0], [1, 1, 0]], np.float32)
    cv = np.array([0, 1, 2, 1, 3, 2], np.uint32)
    n = np.tile(np.array([0, 0, 1], np.float32), (6, 1))
    col = np.zeros((6, 3), np.float32)
    col[:3, 0] = 1  # red triangle
    col[3:, 2] = 1  # blue triangle

    m = mi.Mesh.from_corners('bicolor', p, cv, normals=n,
                             attributes={'vertex_color': col})

    assert m.vertex_count() == 6
    check_consistency(m, p, cv)

    faces = np.array(m.faces_buffer())
    out_col = np.array(m.attribute_buffer('vertex_color')).reshape(-1, 3)
    assert np.array_equal(out_col[faces], col)

    # Without the color attribute, the same input welds to 4 vertices
    m2 = mi.Mesh.from_corners('merged', p, cv, normals=n)
    assert m2.vertex_count() == 4


def test05_index_form_matches_value_form(variant_scalar_rgb):
    p, cv = make_grid()
    rng = np.random.default_rng(0)
    pool = rng.random((10, 2)).astype(np.float32)
    idx = rng.integers(0, len(pool), len(cv)).astype(np.uint32)

    m1 = mi.Mesh.from_corners('index', p, cv, texcoords=(pool, idx))
    m2 = mi.Mesh.from_corners('value', p, cv, texcoords=pool[idx])

    assert m1.vertex_count() == m2.vertex_count()
    assert np.array_equal(np.array(m1.faces_buffer()),
                          np.array(m2.faces_buffer()))
    assert np.array_equal(np.array(m1.vertex_texcoords_buffer()),
                          np.array(m2.vertex_texcoords_buffer()))
    assert np.array_equal(np.array(m1.vertex_positions_buffer()),
                          np.array(m2.vertex_positions_buffer()))


def test06_missing_index_entries(variant_scalar_rgb):
    # OBJ-style mixed faces: the second triangle has no texture coordinates.
    # Its corners must not weld with the first triangle's, and the missing
    # entries produce zeros.
    p = np.array([[0, 0, 0], [1, 0, 0], [0, 1, 0], [1, 1, 0]], np.float32)
    cv = np.array([0, 1, 2, 1, 3, 2], np.uint32)
    pool = np.array([[.5, .5], [1, .5], [.5, 1]], np.float32)
    idx = np.array([0, 1, 2, MISSING, MISSING, MISSING], np.uint32)

    m = mi.Mesh.from_corners('mixed', p, cv, texcoords=(pool, idx))

    assert m.vertex_count() == 6
    faces = np.array(m.faces_buffer())
    out_uv = np.array(m.vertex_texcoords_buffer()).reshape(-1, 2)
    assert np.array_equal(out_uv[faces[:3]], pool)
    assert np.all(out_uv[faces[3:]] == 0)


def test07_bitwise_weld_semantics(variant_scalar_rgb):
    p = np.array([[0, 0, 0], [1, 0, 0], [0, 1, 0], [1, 1, 0]], np.float32)
    cv = np.array([0, 1, 2, 1, 3, 2], np.uint32)

    # -0.0 != 0.0 under the bitwise comparison
    uv = np.zeros((6, 2), np.float32)
    uv[3:, :] = -0.0
    m = mi.Mesh.from_corners('signed_zero', p, cv, texcoords=uv)
    assert m.vertex_count() == 6

    # ... but bitwise-identical NaNs weld (memcmp semantics)
    uv = np.full((6, 2), np.nan, np.float32)
    m = mi.Mesh.from_corners('nan', p, cv, texcoords=uv)
    assert m.vertex_count() == 4


def test08_scalar_extra_attribute(variant_scalar_rgb):
    p = np.array([[0, 0, 0], [1, 0, 0], [0, 1, 0]], np.float32)
    cv = np.array([0, 1, 2], np.uint32)
    alpha = np.array([.25, .5, .75], np.float32)  # 1D value form, dim=1

    m = mi.Mesh.from_corners('alpha', p, cv,
                             attributes={'vertex_alpha': alpha})
    faces = np.array(m.faces_buffer())
    out = np.array(m.attribute_buffer('vertex_alpha'))
    assert np.array_equal(out[faces], alpha)


def test09_face_normals_ignore_normal_attribute(variant_scalar_rgb):
    p, cv, quad_id, _ = make_cube()
    n = np.zeros((len(cv), 3), np.float32)
    n[:, 0] = quad_id  # would split each vertex three ways

    props = mi.Properties()
    props['face_normals'] = True
    m = mi.Mesh.from_corners('cube', p, cv, normals=n, props=props)

    assert m.vertex_count() == 8
    assert not m.has_vertex_normals()
    assert m.has_face_normals()


def test10_empty_and_degenerate(variant_scalar_rgb):
    m = mi.Mesh.from_corners('empty', np.zeros((0, 3), np.float32),
                             np.zeros((0,), np.uint32))
    assert m.vertex_count() == 0
    assert m.face_count() == 0

    # Unreferenced source vertices are dropped
    p = np.zeros((5, 3), np.float32)
    p[:, 0] = np.arange(5)
    cv = np.array([0, 1, 2], np.uint32)
    m = mi.Mesh.from_corners('partial', p, cv)
    assert m.vertex_count() == 3
    assert m.face_count() == 1

    # Vertices without corners, no faces at all
    m = mi.Mesh.from_corners('faceless', p, np.zeros((0,), np.uint32))
    assert m.vertex_count() == 0
    assert m.face_count() == 0


def test11_validation_errors(variant_scalar_rgb):
    p = np.array([[0, 0, 0], [1, 0, 0], [0, 1, 0]], np.float32)
    cv = np.array([0, 1, 2], np.uint32)

    with pytest.raises(RuntimeError, match=r'positions.*shape'):
        mi.Mesh.from_corners('m', np.zeros((3, 4), np.float32), cv)

    with pytest.raises(RuntimeError, match=r'corner_vertex'):
        mi.Mesh.from_corners('m', p, cv.reshape(1, 3))

    with pytest.raises(RuntimeError, match=r'multiple of 3'):
        mi.Mesh.from_corners('m', p, np.array([0, 1], np.uint32))

    with pytest.raises(RuntimeError, match=r'nonexistent vertex'):
        mi.Mesh.from_corners('m', p, np.array([0, 1, 5], np.uint32))

    # Wrong per-corner count
    with pytest.raises(RuntimeError, match=r'corner_count'):
        mi.Mesh.from_corners('m', p, cv, texcoords=np.zeros((6, 2), np.float32))

    # Wrong dimension for a fixed-dimension attribute
    with pytest.raises(RuntimeError, match=r'dimension 3'):
        mi.Mesh.from_corners('m', p, cv, normals=np.zeros((3, 2), np.float32))

    # Non-convertible dtype
    with pytest.raises((RuntimeError, TypeError)):
        mi.Mesh.from_corners('m', p, cv,
                             texcoords=np.zeros((3, 2), np.str_))

    # Malformed index form
    with pytest.raises(RuntimeError, match=r'\(pool, indices\)'):
        mi.Mesh.from_corners('m', p, cv,
                             texcoords=(np.zeros((3, 2), np.float32),))

    # Out-of-bounds pool index
    with pytest.raises(RuntimeError, match=r'out of bounds'):
        mi.Mesh.from_corners(
            'm', p, cv, texcoords=(np.zeros((2, 2), np.float32),
                                   np.array([0, 1, 2], np.uint32)))

    # Extra attributes must use the vertex_ prefix
    with pytest.raises(RuntimeError, match=r'vertex_'):
        mi.Mesh.from_corners(
            'm', p, cv, attributes={'color': np.zeros((3, 3), np.float32)})


def test12_render_smoke(variant_scalar_rgb):
    # A from_corners mesh must be directly usable in a scene
    p, cv = make_grid(2)
    m = mi.Mesh.from_corners('grid', p, cv)
    scene = mi.load_dict({
        'type': 'scene',
        'mesh': m,
        'sensor': {
            'type': 'perspective',
            'to_world': mi.ScalarTransform4f().look_at(
                origin=[1, 1, 5], target=[1, 1, 0], up=[0, 1, 0]),
            'film': {'type': 'hdrfilm', 'width': 16, 'height': 16},
        },
        'integrator': {'type': 'path', 'max_depth': 2},
        'light': {'type': 'constant'},
    })
    img = mi.render(scene, spp=4)
    assert np.all(np.isfinite(np.array(img)))
