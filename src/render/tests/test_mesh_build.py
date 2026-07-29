"""
Tests mesh construction including ``from_fields``, ``from_packed`` and
``from_corners``. Also covers ``Mesh.merge()``, the packed frame
encoding, and custom mesh attributes.
"""

import numpy as np
import pytest
import drjit as dr
import mitsuba as mi

from mitsuba.scalar_rgb.test.util import quad_corners, seam_quad_fields, \
    anisotropic_bsdf, vertex_positions, vertex_normals, faces_of, \
    face_records, assert_uniform_within_groups, unit_triangle


# -------------------------------------------------------------------
# from_fields
# -------------------------------------------------------------------

def test01_from_fields_verbatim(variants_all_rgb):
    """Check that from_fields() correctly adopts its inputs"""
    positions = np.array([[0, 0, 0], [1, 0, 0], [0, 1, 0], [9, 9, 9]],
                         dtype=np.float32)
    faces = np.array([[0, 1, 2]], dtype=np.uint32)
    uv = np.array([[0, 0], [1, 0], [0, 1], [0, 0]], dtype=np.float32)

    m = mi.Mesh("v", faces=faces, positions=positions, texcoords=uv)
    assert m.vertex_count() == 4 and m.position_count() == 4
    assert np.all(vertex_positions(m) == positions)
    assert np.all(np.array(m.texcoords()) == uv)

    # An identity map is stored (and exposed through traverse) as an empty
    # array
    params = mi.traverse(m)
    assert dr.width(params['position_index']) == 0
    assert dr.width(params['normal_index']) == 0
    assert dr.width(m.position_index()) == 0
    assert np.all(np.array(m.geometric_faces()) == faces_of(m))

    # Zeroed placeholder data is accepted, coincident vertices and all, so
    # a mesh can be allocated up front and filled in later via mi.traverse()
    m = mi.Mesh("z", faces=dr.zeros(mi.TensorXu, (1, 3)),
                positions=dr.zeros(mi.TensorXf, (3, 3)))
    assert m.vertex_count() == 3 and m.face_count() == 1
    assert m.has_normals()
    assert np.all(vertex_positions(m) == 0)


def test02_from_fields_explicit_maps(variants_all_rgb):
    """The coarse topology of a pre-split quad can be supplied as an
    explicit position map rather than inferred from position values."""
    coarse, faces, pidx, uv = seam_quad_fields()

    m = mi.Mesh("w")
    m.from_fields(faces=faces, position_index=pidx, positions=coarse,
                  texcoords=uv)
    assert m.vertex_count() == 6
    assert m.position_count() == 4
    assert np.all(np.array(m.position_index()) == pidx)
    assert np.all(np.array(m.geometric_faces()) == [[0, 1, 2], [0, 2, 3]])
    assert np.all(np.array(m.positions()) == coarse)
    # has_tangents() reports that the mesh *can* supply tangents; whether
    # the records carry one is packs_tangent(), which the BSDF decides
    assert m.has_tangents() and not m.packs_tangent()


def test03_to_string(variant_scalar_rgb):
    """Representative check of the to_string() summary."""
    m = mi.Mesh("MyMesh", faces=[[0, 1, 2], [1, 2, 0]],
                positions=[[0, 0, 0], [1, 0.2, 0], [0.2, 1, 0]])
    m.add_attribute("vertex_color", [[0, 1, 1], [0, 0, 0], [1, 1, 0]])

    assert str(m) == """Mesh[
  filename = "MyMesh",
  bbox = BoundingBox3f[
    min = [0, 0, 0],
    max = [1, 1, 0]
  ],
  position_count = 3,
  normal_count = 3,
  vertex_count = 3,
  vertices = [108 B of vertex data],
  face_count = 2,
  faces = [32 B of face data],
  face_normals = 0,
  mesh attributes = [
    vertex_color: 3 floats
  ],
  bsdf = SmoothDiffuse[
    reflectance = UniformSpectrum[value=0.500000]
  ]
]"""


# -------------------------------------------------------------------
# Corner welding (from_corners)
# -------------------------------------------------------------------

def _corner_weld_reference(corner_vertex, attr_columns):
    """np.unique-based reference for the corner weld: returns (count, labels)"""
    C = corner_vertex.shape[0]
    cols = [corner_vertex.reshape(C, 1).astype(np.uint32)]
    for a in attr_columns:
        a = np.asarray(a)
        # Compare float payloads bitwise, folding -0.0 to +0.0
        if a.dtype == np.float32:
            bits = a.view(np.uint32).copy()
            bits[bits == 0x80000000] = 0
            a = bits
        cols.append(a.reshape(C, -1))
    key = np.concatenate(cols, axis=1)
    uniq, inv = np.unique(key, axis=0, return_inverse=True)
    return uniq.shape[0], inv.ravel()


def _partitions_equal(a, b):
    """Check that two labelings induce the same partition (up to relabeling)"""
    fwd, bwd = {}, {}
    for x, y in zip(a, b):
        x, y = int(x), int(y)
        if fwd.setdefault(x, y) != y or bwd.setdefault(y, x) != x:
            return False
    return True


def test04_corner_build_basic(variants_all_rgb):
    """Corners of a source vertex fuse into one vertex when their
    attributes agree. Smooth normals are generated when none are
    supplied."""
    positions, corner_vertex, uv = quad_corners()

    m = mi.Mesh("quad")
    m.from_corners(positions=positions,
                   corner_vertex=corner_vertex,
                   texcoords=uv)

    assert m.vertex_count() == 4 and m.face_count() == 2
    assert m.has_texcoords() and m.has_normals()

    # Welded vertices are numbered by source vertex, so the index maps are
    # the identity and the fields compare directly
    assert m.position_count() == 4 and m.normal_count() == 4
    assert dr.all(m.faces() == mi.TensorXu([[0, 1, 2], [0, 2, 3]]), axis=None)
    assert dr.all(m.positions() == mi.TensorXf(positions), axis=None)
    assert dr.all(m.texcoords() == mi.TensorXf(
        [[0, 0], [1, 0], [1, 1], [0, 1]]), axis=None)
    assert dr.allclose(m.normals(), mi.TensorXf([[0, 0, 1]] * 4))

    b = m.bbox()
    dr.assert_allclose(b.min, [0, 0, 0])
    dr.assert_allclose(b.max, [1, 1, 0])

    # An attribute payload that agrees across a source vertex's corners does
    # not split it either, with positive and negative zero comparing equal.
    # Float inputs of other dtypes arrive through a converting copy, while
    # signed index arrays are read in place.
    w = np.zeros(6, dtype=np.float32)
    w[3] = -0.0
    m = mi.Mesh("quad")
    m.from_corners(positions=positions.astype(np.float64),
                   corner_vertex=corner_vertex.astype(np.int32),
                   attrs={"vertex_w": w})
    assert m.vertex_count() == 4


@pytest.mark.parametrize("hard_edge", [False, True])
def test05_corner_build_seam_maps(variant_scalar_rgb, hard_edge):
    """A UV seam splits the shared vertices, which nonetheless remain a
    single surface point each. Without authored normals the normal level
    collapses to P, while a hard edge among them splits one group."""
    positions, corner_vertex, uv = quad_corners(seam=True)
    kwargs = {}
    if hard_edge:
        normals = np.tile(np.float32([0, 0, 1]), (6, 1))
        normals[3] = (0, 1, 0)  # only at the first seam vertex
        kwargs['normals'] = normals

    m = mi.Mesh("quad")
    m.from_corners(positions=positions, corner_vertex=corner_vertex,
                   texcoords=uv, **kwargs)

    # Both shared vertices split; sub-parts are numbered by smallest corner
    assert m.vertex_count() == 6
    assert m.position_count() == 4
    assert m.normal_count() == (5 if hard_edge else 4)
    assert np.all(faces_of(m) == [[0, 2, 3], [1, 4, 5]])

    pos = vertex_positions(m)
    assert np.all(pos[0] == pos[1]) and np.all(pos[3] == pos[4])
    uv_out = np.array(m.texcoords())
    assert np.all(uv_out[1] == (5, 5)) and np.all(uv_out[4] == (6, 6))

    # Each position group holds one distinct position value
    pidx = np.array(m.position_index())
    assert_uniform_within_groups(pos, pidx)
    assert len(np.unique(pidx)) == 4

    # The normal groups follow the surface points, except where the hard
    # edge splits the first seam vertex. The other one stays fused.
    nidx = np.array(m.normal_index())
    if hard_edge:
        assert nidx[0] != nidx[1] and nidx[3] == nidx[4]
    else:
        assert np.all(nidx == pidx)

    # geometric_faces maps both triangles onto the same 4 surface points
    assert np.all(np.array(m.geometric_faces()) == [[0, 1, 2], [0, 2, 3]])


def test06_corner_build_vs_reference(variant_scalar_rgb):
    """The weld of random corner data matches an np.unique reference over
    (source vertex, attribute values) and is deterministic."""
    rng = np.random.RandomState(0)

    n_src, n_faces = 50, 100
    positions = rng.rand(n_src, 3).astype(np.float32)
    corner_vertex = rng.randint(0, n_src, 3 * n_faces).astype(np.uint32)

    # Attributes drawn from small value sets to force fusing
    values = rng.randint(0, 3, (3 * n_faces, 3)).astype(np.float32)
    pool = rng.rand(10, 2).astype(np.float32)
    values_b = pool[rng.randint(0, 10, 3 * n_faces)]

    def build():
        m = mi.Mesh("random")
        m.from_corners(positions=positions, corner_vertex=corner_vertex,
                     attrs={"vertex_a": values, "vertex_b": values_b})
        return m

    m = build()
    ref_count, ref_labels = _corner_weld_reference(
        corner_vertex, [values, values_b])

    assert m.vertex_count() == ref_count
    faces = faces_of(m).ravel()
    assert _partitions_equal(faces, ref_labels)

    # Materialized values match the corner data
    a_out = np.array(m.attribute("vertex_a"))
    b_out = np.array(m.attribute("vertex_b"))
    for c in range(3 * n_faces):
        v = faces[c]
        assert np.all(a_out[v] == values[c])
        assert np.all(b_out[v] == values_b[c])

    # Determinism: an identical rebuild yields identical buffers
    m2 = build()
    for buf, buf2 in [(face_records(m), face_records(m2)),
                      (m.positions(), m2.positions()),
                      (m.attribute("vertex_a"), m2.attribute("vertex_a")),
                      (m.attribute("vertex_b"), m2.attribute("vertex_b"))]:
        assert np.all(np.array(buf) == np.array(buf2))

    # The same mesh with its corner data held in shuffled pools that
    # corner_index addresses. Records that no corner selects are ignored,
    # so the padding below is never validated or read.
    n = 3 * n_faces
    perm = rng.permutation(n)
    inv = np.empty(n, dtype=np.uint32)
    inv[perm] = np.arange(n, dtype=np.uint32)

    def pooled(x, fill):
        return np.vstack([x[perm], np.full((5,) + x.shape[1:], fill, x.dtype)])

    m3 = mi.Mesh("random")
    m3.from_corners(positions=positions,
                    corner_vertex=np.concatenate(
                        [corner_vertex[perm], np.full(5, 999, np.uint32)]),
                    corner_index=inv,
                    attrs={"vertex_a": pooled(values, np.nan),
                           "vertex_b": pooled(values_b, np.nan)})

    for buf, buf3 in [(face_records(m), face_records(m3)),
                      (m.positions(), m3.positions()),
                      (m.attribute("vertex_a"), m3.attribute("vertex_a")),
                      (m.attribute("vertex_b"), m3.attribute("vertex_b"))]:
        assert np.all(np.array(buf) == np.array(buf3))


def test07_corner_build_supplied_normals(variant_scalar_rgb):
    """Authored normals are normalized and split hard edges. When face
    normals are requested instead, they are ignored entirely."""
    # Unlike from_fields(), which requires unit normals, from_corners()
    # normalizes on the way in, since it ingests DCC and file data
    positions, corner_vertex, _ = quad_corners()

    # A hard edge along the diagonal: both triangles supply constant but
    # different (unnormalized) normals
    normals = np.zeros((6, 3), dtype=np.float32)
    normals[0:3] = (0, 0, 2)
    normals[3:6] = (0, 2, 2)
    m = mi.Mesh("quad")
    m.from_corners(positions=positions, corner_vertex=corner_vertex,
                 normals=normals)

    assert m.vertex_count() == 6
    n_out = vertex_normals(m)
    assert np.allclose(np.linalg.norm(n_out, axis=1), 1)
    s = np.sqrt(0.5)
    assert np.allclose(n_out[0], [0, 0, 1])
    assert np.allclose(n_out[1], [0, s, s])

    m = mi.Mesh("quad", face_normals=True)
    m.from_corners(positions=positions, corner_vertex=corner_vertex,
                 normals=normals)
    assert m.vertex_count() == 4
    assert not m.has_normals()


def test08_corner_build_normal_seam_average(variants_all_rgb):
    """Generated normals average over both incident faces even where a UV
    seam has split the vertices, so the seam does not turn into a crease."""
    positions = np.array([[0, 0, 0], [1, 0, 0], [0.5, 1, 0], [0.5, -1, 1]],
                         dtype=np.float32)
    corner_vertex = np.array([0, 1, 2, 1, 0, 3], dtype=np.uint32)
    uv = np.array([[0, 0], [1, 0], [1, 1],
                   [6, 5], [5, 5], [6, 6]], dtype=np.float32)

    m = mi.Mesh("bent")
    m.from_corners(positions=positions, corner_vertex=corner_vertex,
                 texcoords=uv)
    assert m.vertex_count() == 6
    assert m.position_count() == 4

    pidx = np.array(m.position_index())
    normals = vertex_normals(m)
    assert_uniform_within_groups(normals, pidx)

    # The seam normals average both faces, so they match neither face normal
    seam = normals[pidx == pidx[0]]
    assert not np.allclose(seam[0], [0, 0, 1], atol=1e-3)


def test09_bounds(variant_scalar_rgb):
    """Source vertices without a referencing corner are dropped and do not
    affect the bounding box. Per-primitive bounds cover one face, clipped
    against an optional box."""
    positions, corner_vertex, _ = quad_corners()
    positions = np.vstack([positions, [(100, 100, 100)]]).astype(np.float32)

    m = mi.Mesh("quad")
    m.from_corners(positions=positions, corner_vertex=corner_vertex)
    assert m.vertex_count() == 4
    dr.assert_allclose(m.bbox().max, [1, 1, 0])

    # Face 0 is the lower right half of the quad, so clipping it to the
    # upper left quadrant cuts the corner off its bounds
    dr.assert_allclose(m.bbox(0).min, [0, 0, 0])
    clipped = m.bbox(0, mi.ScalarBoundingBox3f([0, 0.5, -1], [0.5, 1, 1]))
    dr.assert_allclose(clipped.min, [0.5, 0.5, 0])
    dr.assert_allclose(clipped.max, [0.5, 0.5, 0])


def test10_polygon_faces(variant_scalar_rgb):
    """Polygons fan around their first corner, so quads split along the
    0-2 diagonal. Faces with fewer than three corners are skipped."""
    positions = np.array([(np.cos(a), np.sin(a), 0) for a in
                          np.linspace(0, 2 * np.pi, 5, endpoint=False)],
                         dtype=np.float32)

    # Quad: deterministic 0-2 diagonal
    quad_pos = np.array([[0, 0, 0], [1, 0, 0], [1, 1, 0], [0, 1, 0]],
                        dtype=np.float32)
    m = mi.Mesh("quad")
    m.from_corners(positions=quad_pos,
                 corner_vertex=np.arange(4, dtype=np.uint32),
                 face_offsets=np.array([0, 4], dtype=np.uint32))
    assert np.array(m.geometric_faces()).tolist() == [[0, 1, 2], [0, 2, 3]]

    # A pentagon fans around corner 0; faces with fewer than 3 corners are
    # skipped rather than rejected
    corner_vertex = np.array([0, 1, 0, 1, 2, 3, 4], dtype=np.uint32)
    offsets = np.array([0, 1, 2, 7], dtype=np.uint32)
    m = mi.Mesh("pent")
    m.from_corners(positions=positions, corner_vertex=corner_vertex,
                   face_offsets=offsets)
    assert m.face_count() == 3
    assert np.all(faces_of(m) == [[0, 1, 2], [0, 2, 3], [0, 3, 4]])


# -------------------------------------------------------------------
# Cross-path equivalence
# -------------------------------------------------------------------

def _build_seam_quad(path):
    """The unit quad with a UV seam, built through the given construction path"""
    coarse, faces, pidx, uv_fields = seam_quad_fields()
    if path == 'fields':
        m = mi.Mesh("q")
        m.from_fields(faces=faces, position_index=pidx, positions=coarse,
                      texcoords=uv_fields)
    elif path == 'corner':
        positions, corner_vertex, uv = quad_corners(seam=True)
        m = mi.Mesh("q")
        m.from_corners(positions=positions, corner_vertex=corner_vertex,
                     texcoords=uv)
    elif path == 'packed':
        base = _build_seam_quad('corner')
        params = mi.traverse(base)
        m = mi.Mesh("q")
        layout = mi.Layout.Normals | mi.Layout.Texcoords
        m.from_packed(layout, face_records(base).astype(np.uint32),
                      np.array(base.packed_vertices()).reshape(-1, 8),
                      position_index=params['position_index'],
                      normal_index=params['normal_index'],
                      position_count=base.position_count(),
                      normal_count=base.normal_count())
    return m


def test11_construction_paths_agree(variants_all_rgb):
    """The same mesh built via from_fields, from_corners and from_packed
    renders identically. The packed path additionally adopts the state of
    its source verbatim."""
    meshes = {p: _build_seam_quad(p) for p in ('fields', 'corner', 'packed')}

    counts = {(m.vertex_count(), m.face_count(),
               m.position_count(), m.normal_count())
              for m in meshes.values()}
    assert counts == {(6, 2, 4, 4)}

    results = []
    for m in meshes.values():
        scene = mi.load_dict({'type': 'scene', 'm': m})
        x = np.float32([0.2, 0.7, 0.4])
        y = np.float32([0.1, 0.8, 0.6])
        si_p, si_uv = [], []
        for xi, yi in zip(x, y):
            ray = mi.Ray3f(mi.Point3f(float(xi), float(yi), 5),
                           mi.Vector3f(0, 0, -1))
            si = scene.ray_intersect(ray)
            assert dr.all(si.is_valid())
            si_p.append(np.array(si.p).ravel())
            si_uv.append(np.array(si.uv).ravel())
        results.append((np.array(si_p), np.array(si_uv)))

    for p, uv in results[1:]:
        assert np.allclose(p, results[0][0], atol=1e-6)
        assert np.allclose(uv, results[0][1], atol=1e-6)

    # The packed path recovers the coarse fields of its source exactly
    m, m2 = meshes['corner'], meshes['packed']
    assert np.array_equal(np.array(m2.packed_vertices()),
                          np.array(m.packed_vertices()))
    assert np.array_equal(face_records(m2), face_records(m))
    assert np.array_equal(np.array(m2.positions()), np.array(m.positions()))
    assert np.array_equal(np.array(m2.normals()), np.array(m.normals()))
    assert np.array_equal(np.array(m2.texcoords()), np.array(m.texcoords()))
    assert np.allclose(np.array(m2.bbox().min), np.array(m.bbox().min))
    assert np.allclose(np.array(m2.bbox().max), np.array(m.bbox().max))

    # Positions-only records with requested generation reproduce the
    # generated normals from scratch
    m3 = mi.Mesh("quad3")
    m3.from_packed(mi.Layout.Positions,
                   face_records(m).astype(np.uint32),
                   np.array(m.packed_vertices()).reshape(-1, 8),
                   position_index=mi.traverse(m)['position_index'],
                   position_count=m.position_count())
    assert m3.has_normals() and not m3.has_texcoords()
    assert np.allclose(np.array(m3.normals()), np.array(m.normals()),
                       atol=1e-6)


# -------------------------------------------------------------------
# Input validation
# -------------------------------------------------------------------

def test12_construction_errors(variants_all_rgb, capfd):
    """Malformed inputs to the three construction entry points raise an
    error. Non-finite positions are the exception: they only warn."""
    positions, corner_vertex, uv = quad_corners()
    zeros = np.zeros((4, 3), dtype=np.float32)
    faces = np.uint32([[0, 1, 2]])
    packed = np.zeros((3, 8), dtype=np.float32)
    frec = np.uint32([[0, 1, 2, 0]])
    bad_vertex = corner_vertex.copy()
    bad_vertex[2] = 99
    m = mi.Mesh("quad")

    cases = [
        # from_fields()
        (lambda: m.from_fields(faces=np.uint32([0, 1, 2]), positions=zeros),
         RuntimeError, "'faces' must be a"),
        (lambda: m.from_fields(faces=faces,
                               positions=np.zeros((4, 2), np.float32)),
         RuntimeError, "'positions' must be a"),
        (lambda: m.from_fields(faces=faces, positions=zeros,
                               texcoords=np.zeros((2, 2), np.float32)),
         RuntimeError, "'texcoords' has 2 rows"),
        (lambda: m.from_fields(faces=faces, positions=zeros,
                               normals=np.zeros((4, 3), np.float32),
                               normal_index=np.uint32([0, 0])),
         RuntimeError, "'normal_index' has 2 entries"),
        (lambda: m.from_fields(faces=faces, positions=zeros,
                               normal_index=np.uint32([0, 0, 0, 0])),
         RuntimeError, "requires a 'normals'"),
        (lambda: m.from_fields(faces=faces, positions=zeros,
                               bsdf_index=np.uint32([0, 0])),
         RuntimeError, "'bsdf_index' has 2 entries"),
        # from_corners(). The dtype and shape of an argument are enforced
        # by the binding signature, hence the TypeError
        (lambda: m.from_corners(positions=positions[:, :2].copy(),
                                corner_vertex=corner_vertex),
         TypeError, "shape"),
        (lambda: m.from_corners(positions=positions,
                                corner_vertex=corner_vertex,
                                texcoords=uv[:3].copy()),
         RuntimeError, "record_count"),
        (lambda: m.from_corners(positions=positions,
                                corner_vertex=corner_vertex,
                                corner_index=np.uint32([0, 1, 6])),
         RuntimeError, "'corner_index' selects record 6"),
        # Index arrays are reinterpreted rather than converted, so anything
        # that is not a 32-bit integer is rejected outright
        (lambda: m.from_corners(positions=positions,
                                corner_vertex=corner_vertex.astype(np.int64)),
         RuntimeError, "'corner_vertex' must be a 32-bit integer"),
        (lambda: m.from_corners(positions=positions,
                                corner_vertex=corner_vertex[:5].copy()),
         RuntimeError, "multiple of 3"),
        (lambda: m.from_corners(positions=positions,
                                corner_vertex=bad_vertex),
         RuntimeError, "references vertex"),
        (lambda: m.from_corners(positions=positions,
                                corner_vertex=corner_vertex,
                                attrs={"foo": uv}),
         RuntimeError, "vertex_"),
        # from_packed()
        (lambda: m.from_packed(mi.Layout.Tangents |
                               mi.Layout.Normals, frec, packed),
         RuntimeError, "'Tangents' layout requires"),
        (lambda: m.from_packed(mi.Layout.Positions, frec, packed,
                               position_index=np.uint32([0, 0, 0])),
         RuntimeError, "requires a nonzero"),
    ]
    for fn, exc, match in cases:
        with pytest.raises(exc, match=match):
            fn()

    # Polygon offsets must start at zero and increase without running past
    # the corner array
    for bad in ([0, 3, 2], [1, 4], [0, 5]):
        with pytest.raises(RuntimeError, match="face_offsets"):
            m.from_corners(positions=positions,
                           corner_vertex=corner_vertex[:4].copy(),
                           face_offsets=np.uint32(bad))

    # Non-finite positions only produce a warning naming the mesh
    positions[0] = (np.nan, 0, 0)
    m.from_corners(positions=positions, corner_vertex=corner_vertex,
                   texcoords=uv)
    assert m.vertex_count() == 4
    out = capfd.readouterr().out
    assert "quad" in out and "not finite" in out

    # The failed calls above left the mesh buildable, but construction is
    # one-shot: after the success, every entry point rejects a second call
    for fn in (lambda: m.from_fields(faces=faces, positions=zeros),
               lambda: m.from_corners(positions=positions,
                                      corner_vertex=corner_vertex),
               lambda: m.from_packed(mi.Layout.Positions, frec,
                                     packed)):
        with pytest.raises(RuntimeError, match='already built'):
            fn()

    # Construction trusts that indices are in bounds; the opt-in second
    # validation tier catches violations
    m = mi.Mesh("oob", faces=np.uint32([[0, 1, 9]]), positions=zeros,
                face_normals=True)
    m.validate()
    with pytest.raises(RuntimeError, match='out-of-range'):
        m.validate(check_bounds=True)


# -------------------------------------------------------------------
# Merge
# -------------------------------------------------------------------

def _merge_input(name, offset, bsdf=None, bsdf_index=None, seam=False,
                 normals=None, texcoords=False):
    """
    Build an input mesh for the Mesh.merge() tests, translated by ``offset``.

    By default this is a single triangle, whose position map is the
    identity. With ``seam``, it is a quad split into 6 vertices over 4
    positions instead, which gives merge() a non-identity map to renumber.
    """
    kwargs = {}
    if bsdf_index is not None:
        kwargs['bsdf_index'] = np.uint32(bsdf_index)
    if normals is not None:
        kwargs['normals'] = normals
    if texcoords:
        kwargs['texcoords'] = np.float32([[0, 0], [1, 0], [0, 1]])
    if seam:
        coarse = np.float32([[0, 0, 0], [1, 0, 0], [1, 1, 0], [0, 1, 0]])
        m = mi.Mesh(name, faces=[[0, 1, 2], [3, 4, 5]],
                    positions=coarse + np.float32(offset),
                    position_index=np.uint32([0, 1, 2, 0, 2, 3]), **kwargs)
    else:
        tri = np.float32([[0, 0, 0], [1, 0, 0], [0, 1, 0]])
        m = mi.Mesh(name, faces=[[0, 1, 2]],
                    positions=tri + np.float32(offset), **kwargs)
    if bsdf is not None:
        m.set_bsdf(bsdf)
    return m


def test13_merge_batch(variants_all_rgb):
    """Mesh.merge() concatenates every level of the representation. The
    vertex index lanes and maps shift by prefix offsets, while the BSDF
    lane passes through unchanged."""
    bsdf = mi.load_dict({'type': 'diffuse'})
    a = _merge_input("a", 0, bsdf, bsdf_index=[1])
    b = _merge_input("b", 10, bsdf, bsdf_index=[2, 3], seam=True)
    c = _merge_input("c", 20, bsdf)

    # Errors: empty input, non-mesh entries, incompatible meshes
    with pytest.raises(RuntimeError, match="empty"):
        mi.Mesh.merge([])
    with pytest.raises(RuntimeError, match="meshes"):
        mi.Mesh.merge([a, mi.load_dict({'type': 'sphere'})])
    uv_mesh = _merge_input("uv", 0, texcoords=True)
    with pytest.raises(RuntimeError, match="incompatible"):
        mi.Mesh.merge([a, uv_mesh])

    # A singleton list returns its element; the result is a Mesh
    # (regression test for the ref<T> return value caster)
    assert mi.Mesh.merge([a]) is a
    assert isinstance(mi.Mesh.merge([a, a]), mi.Mesh)

    m = mi.Mesh.merge([a, b, c])
    assert m.vertex_count() == 12 and m.face_count() == 4
    assert m.position_count() == 10

    rec = face_records(m)
    assert np.all(rec == [[0, 1, 2, 1],
                          [3, 4, 5, 2], [6, 7, 8, 3],
                          [9, 10, 11, 0]])
    assert np.all(np.array(m.position_index())
                  == [0, 1, 2, 3, 4, 5, 3, 5, 6, 7, 8, 9])
    assert np.allclose(np.array(m.positions()),
                       np.vstack([np.array(x.positions()) for x in (a, b, c)]))
    assert m.bbox().contains(a.bbox().min) and m.bbox().contains(c.bbox().max)


def test14_merge_full_layout(variants_all_rgb):
    """Merged meshes with a packed tangent layout keep their encoded
    frames."""
    bsdf = anisotropic_bsdf()
    rng = np.random.RandomState(0)

    def rich_tri(name, offset):
        n = rng.randn(3, 3).astype(np.float32)
        n /= np.linalg.norm(n, axis=1, keepdims=True)
        return _merge_input(name, offset, bsdf, normals=n, texcoords=True)

    a, b = rich_tri("a", 0), rich_tri("b", 10)
    assert a.packs_tangent() and b.packs_tangent()

    m = mi.Mesh.merge([a, b])
    assert m.vertex_count() == 6 and m.face_count() == 2
    assert m.packs_tangent() and m.has_texcoords()
    assert np.array_equal(
        np.array(m.packed_vertices()),
        np.concatenate([np.array(a.packed_vertices()),
                        np.array(b.packed_vertices())]))
    assert np.allclose(np.array(m.normals()),
                       np.vstack([np.array(a.normals()),
                                  np.array(b.normals())]), atol=1e-6)


# -------------------------------------------------------------------
# Packed frame encoding
# -------------------------------------------------------------------

def _random_frames(count):
    """Random orthonormal (normal, tangent) pairs as wide vectors"""
    rng = mi.PCG32(size=count)
    n = mi.Vector3f(mi.warp.square_to_uniform_sphere(
        mi.Point2f(rng.next_float32(), rng.next_float32())))

    # A random rotation of a coordinate frame around n is always
    # well conditioned
    f = mi.Frame3f(n)
    a = rng.next_float32() * (2 * dr.pi)
    return n, f.s * dr.cos(a) + f.t * dr.sin(a)


def _angle_deg(a, b):
    """Angle between corresponding unit vectors, robust for small angles"""
    return dr.rad2deg(2 * dr.asin(dr.clip(dr.norm(a - b) * 0.5, 0, 1)))


@pytest.mark.parametrize("delta", [None, 1e-1, 1e-3, 1e-6, 0.0])
def test16_frame_encode_roundtrip(variants_vec_rgb, delta):
    """frame_encode() and frame_decode() round trip accurately over the
    whole of SO(3). At the seam (rotation angle pi), antipodal points
    decode to the same frame."""
    import math
    if delta is None:
        n, s = _random_frames(8192)
    else:
        # Frames rotated by an angle close to pi around random axes
        rng = mi.PCG32(size=4096)
        u = mi.Vector3f(mi.warp.square_to_uniform_sphere(
            mi.Point2f(rng.next_float32(), rng.next_float32())))
        st, ct = math.sin(math.pi - delta), math.cos(math.pi - delta)

        def rot(v):
            return v * ct + dr.cross(u, v) * st + u * (dr.dot(u, v) * (1 - ct))

        n = dr.normalize(rot(mi.Vector3f(0, 0, 1)))
        s = rot(mi.Vector3f(1, 0, 0))
        s = dr.normalize(s - n * dr.dot(n, s))

    p = mi.Mesh.frame_encode(n, s)
    assert dr.all(dr.norm(p) <= 1 + 1e-6)

    # The thresholds sit a few times above the float32 round-off of the
    # encoding itself, which is ~3e-5 degrees and ~6e-7 per component
    n_dec, s_dec = mi.Mesh.frame_decode(p)
    assert dr.all(_angle_deg(n, n_dec) < 2e-4)
    assert dr.all(_angle_deg(s, s_dec) < 2e-4)

    # The bitangent is implied, so the decoded frame is right-handed
    dr.assert_allclose(dr.dot(n_dec, s_dec), 0, atol=3e-6)
    dr.assert_allclose(dr.norm(n_dec), 1, atol=3e-6)
    dr.assert_allclose(dr.cross(n_dec, s_dec), dr.cross(n, s), atol=5e-6)


# frame_encode() after a linear transform (renormalized inverse-transpose
# normal, transformed tangent) is exercised end to end through the
# serialized loader's to_world path in test_mesh_io.test09.


def test17_packed_frame_layout(variants_vec_rgb):
    """The frame lanes hold the plain normal until a material consumes
    the tangent frame, at which point they hold the encoded frame.
    Detaching the material repacks them back to plain normals."""
    rng = np.random.RandomState(1)
    n_tri = 64
    # Include frames near the encoding seam
    n = np.stack([np.ones(n_tri), rng.randn(n_tri),
                  np.linspace(-1e-3, 1e-3, n_tri)], axis=1)
    n = np.concatenate([n, rng.randn(2 * n_tri, 3)])
    n = (n / np.linalg.norm(n, axis=1, keepdims=True)).astype(np.float32)
    rng.shuffle(n)

    v = n.shape[0]
    positions = rng.randn(v, 3).astype(np.float32)
    uv = np.tile([[0, 0], [1, 0], [0, 1]], (v // 3, 1)).astype(np.float32)
    faces = np.arange(v, dtype=np.uint32).reshape(-1, 3)
    m = mi.Mesh("frames")
    m.from_fields(faces=faces, positions=positions, normals=n, texcoords=uv)

    # Positions and texture coordinates are stored verbatim; the normals
    # pass through a renormalization on the way in
    assert not m.packs_tangent()
    buf = np.array(m.packed_vertices()).reshape(-1, 8)
    assert np.array_equal(buf[:, 0:3], positions)
    assert np.allclose(buf[:, 3:6], n, atol=1e-6)
    assert np.array_equal(buf[:, 6:8], uv)

    m.set_bsdf(anisotropic_bsdf())
    assert m.packs_tangent()
    buf = np.array(m.packed_vertices()).reshape(-1, 8)
    assert np.array_equal(buf[:, 0:3], positions)
    assert np.array_equal(buf[:, 6:8], uv)
    assert np.all(np.linalg.norm(buf[:, 3:6], axis=1) <= 1 + 1e-6)

    n_dec, t_dec = mi.Mesh.frame_decode(
        mi.Vector3f(buf[:, 3], buf[:, 4], buf[:, 5]))
    assert np.allclose(np.array(n_dec).T, n, atol=1e-6)
    assert np.allclose(np.array(t_dec).T, np.array(m.tangents()), atol=1e-5)
    dr.assert_allclose(dr.dot(n_dec, t_dec), 0, atol=1e-6)

    # Back to a plain layout: the tangents are no longer resident, but
    # rebuild on demand and agree with the values that round-tripped
    # through the packed lane
    t_packed = np.array(m.tangents())
    m.set_bsdf(mi.load_dict({'type': 'diffuse'}))
    assert not m.packs_tangent() and m.has_tangents()
    buf = np.array(m.packed_vertices()).reshape(-1, 8)
    # The plain normals are decoded from the frame lanes, so they return
    # to the inputs at encoding rather than storage precision
    assert np.allclose(buf[:, 3:6], n, atol=1e-5)
    assert np.allclose(np.array(m.tangents()), t_packed, atol=1e-5)


# -------------------------------------------------------------------
# Custom attributes
# -------------------------------------------------------------------

def test18_attribute_management(variants_all_rgb):
    """add_attribute() validates the name and tensor shape. Attributes can
    be read back and removed again. Those with 2 or 4 channels round trip
    through attribute() but read as zero through eval_attribute_1/3."""
    m = unit_triangle()

    for name, values, match in [
            ("vertex_uv2", [[0, 1]], 'one row per vertex'),
            ("face_id", [[0], [1]], 'one row per face'),
            ("vertex_big", np.zeros((3, 5), np.float32), '1 to 4 channels'),
            ("bogus", [[0], [1], [2]], 'vertex_.*face_')]:
        with pytest.raises(Exception, match=match):
            m.add_attribute(name, values)

    m.add_attribute("vertex_id", [[3], [7], [9]])
    assert m.has_mesh_attributes()
    dr.assert_allclose(m.attribute("vertex_id").array, [3, 7, 9])
    with pytest.raises(Exception, match='already exists'):
        m.add_attribute("vertex_id", [[0], [0], [0]])

    with pytest.raises(Exception, match='not found'):
        m.remove_attribute("vertex_wrong")
    m.remove_attribute("vertex_id")
    assert not m.has_attribute("vertex_id")

    uv2 = np.arange(6, dtype=np.float32).reshape(3, 2)
    quad = np.arange(12, dtype=np.float32).reshape(3, 4)
    m.add_attribute("vertex_uv2", uv2)
    m.add_attribute("vertex_quad", quad)
    assert np.array_equal(np.array(m.attribute("vertex_uv2")), uv2)
    assert np.array_equal(np.array(m.attribute("vertex_quad")), quad)

    si = dr.zeros(mi.SurfaceInteraction3f)
    si.prim_index = 0
    si.uv = mi.Point2f(0.3, 0.3)
    for name in ("vertex_uv2", "vertex_quad"):
        dr.assert_allclose(m.eval_attribute_1(name, si), 0)
        dr.assert_allclose(m.eval_attribute_3(name, si), 0)


def test19_attribute_params(variants_all_rgb):
    """Custom attributes appear as (rows, dim) tensors in the parameter
    view. Writes replace the records but cannot change the shape."""
    positions, corner_vertex, uv = quad_corners(seam=True)
    col = np.arange(18, dtype=np.float32).reshape(6, 3)
    m = mi.Mesh("quad")
    m.from_corners(positions=positions, corner_vertex=corner_vertex,
                 texcoords=uv, attrs={"vertex_color": col})
    m.add_attribute("face_id", [[3], [7]])
    params = mi.traverse(m)
    V, F = m.vertex_count(), m.face_count()

    # Inserting another attribute must not invalidate the handles that
    # the traversal above placed in the parameter map
    m.add_attribute("vertex_id", np.zeros((V, 1), np.float32))

    color = params['vertex_color']
    assert color.shape == (V, 3)
    assert np.array_equal(np.array(color),
                          np.array(m.attribute("vertex_color")))
    face_id = params['face_id']
    assert face_id.shape == (F, 1)
    assert np.array_equal(np.array(face_id).ravel(),
                          np.array(m.attribute("face_id")).ravel())

    val = np.arange(V * 3, dtype=np.float32).reshape(V, 3) + 100
    params['vertex_color'] = val
    params.update()
    assert np.array_equal(np.array(m.attribute("vertex_color")), val)

    with pytest.raises(Exception, match='must be a'):
        params['vertex_color'] = np.zeros((V + 1, 3), np.float32)
        params.update()


def test20_texture_attributes(variants_all_rgb):
    """Texture attributes work on meshes and analytic shapes alike."""
    from mitsuba.scalar_rgb.test.util import fresolver_append_path

    @fresolver_append_path
    def run():
        texture = mi.load_dict({
            "type": "bitmap",
            "filename": "resources/data/common/textures/flower.bmp",
        })
        texture2 = mi.load_dict({
            "type": "bitmap",
            "filter_type": "nearest",
            "data": dr.full(mi.TensorXf, 0.3, [1, 1, 3])
        })

        shapes = [
            mi.load_dict({
                "type": "obj",
                "filename": "resources/data/common/meshes/rectangle.obj",
                "attribute_1": texture
            }),
            mi.load_dict({
                "type": "sphere",
                "attribute_1": texture
            }),
        ]

        for shape in shapes:
            # Texture attribute registered at creation
            assert dr.all(shape.has_attribute('attribute_1'))
            assert not dr.any(shape.has_attribute('foo'))

            si = mi.SurfaceInteraction3f()
            si.uv = mi.Point2f(0.5)
            dr.assert_allclose(shape.eval_attribute('attribute_1', si),
                               texture.eval(si))

            # Registration after construction, replacement, deletion
            shape.add_texture_attribute('attribute_2', texture2)
            assert dr.all(shape.has_attribute('attribute_2'))
            shape.add_texture_attribute('attribute_2', texture)
            assert shape.texture_attribute('attribute_2') == texture
            shape.add_texture_attribute('attribute_2', texture2)

            shape.remove_attribute('attribute_1')
            assert not dr.any(shape.has_attribute('attribute_1'))
            with pytest.raises(RuntimeError,
                               match='Attribute "attribute_1" not found'):
                shape.remove_attribute('attribute_1')

            # Constant-valued texture attribute
            for p in (0.0, 1.0):
                si.uv = mi.Point2f(p)
                dr.assert_allclose(shape.eval_attribute_3('attribute_2', si),
                                   mi.Color3f(0.3))
    run()


def test13_to_world_baked(variants_all_rgb):
    """A 'to_world' property is baked into the geometry when the mesh is
    built, as the file loaders do it. A mirroring transform additionally
    reverses the winding, so the geometric normals keep agreeing with the
    shading normals that the inverse transpose already turned outwards."""
    positions, corner_vertex, uv = quad_corners()
    normals = np.tile(np.float32([0, 0, 1]), (6, 1))

    def build(to_world=None):
        props = mi.Properties()
        if to_world is not None:
            props['to_world'] = to_world
        m = mi.Mesh(props)
        m.from_corners(positions=positions, corner_vertex=corner_vertex,
                       normals=normals, texcoords=uv)
        return m

    plain = build()
    shifted = build(mi.ScalarAffineTransform4f().translate([1, 2, 3]))
    assert np.allclose(np.array(shifted.bbox().min),
                       np.array(plain.bbox().min) + [1, 2, 3])
    assert np.allclose(np.array(shifted.bbox().max),
                       np.array(plain.bbox().max) + [1, 2, 3])
    assert np.all(faces_of(shifted) == faces_of(plain))

    # Mirroring reverses every face. The inverse transpose already keeps the
    # shading normals pointing away from the surface, so reversing the
    # winding is what stops the geometric normal from ending up opposed to
    # them: both survive the mirror unchanged here.
    mirrored = build(mi.ScalarAffineTransform4f().scale([-1, 1, 1]))
    assert np.all(faces_of(mirrored) == faces_of(plain)[:, ::-1])
    assert np.allclose(np.array(mirrored.positions())[:, 0],
                       -np.array(plain.positions())[:, 0])

    for m in (plain, mirrored):
        n_face = np.array(m.face_normal(0)).ravel()
        assert np.allclose(n_face, [0, 0, 1])
        assert np.all(vertex_normals(m) @ n_face > 0)

    # Polygons are affected the same way, and because the reversal applies
    # to the triangles the fan produced, it keeps their diagonals
    pent = np.array([(np.cos(a), np.sin(a), 0) for a in
                     np.linspace(0, 2 * np.pi, 5, endpoint=False)],
                    dtype=np.float32)

    def build_pentagon(to_world=None):
        props = mi.Properties()
        if to_world is not None:
            props['to_world'] = to_world
        m = mi.Mesh(props)
        m.from_corners(positions=pent,
                       corner_vertex=np.arange(5, dtype=np.uint32),
                       face_offsets=np.uint32([0, 5]))
        return m

    fan = build_pentagon()
    fan_mirrored = build_pentagon(mi.ScalarAffineTransform4f().scale([-1, 1, 1]))
    assert np.all(faces_of(fan) == [[0, 1, 2], [0, 2, 3], [0, 3, 4]])
    assert np.all(faces_of(fan_mirrored) == faces_of(fan)[:, ::-1])


def test14_flip_normals_baked(variants_all_rgb):
    """'flip_normals' turns the surface inside out as the mesh is built: it
    reverses the winding and negates the stored shading normals, so that a
    later regeneration reproduces the same orientation. A mirroring
    'to_world' reverses the winding too, so the two cancel."""
    positions, corner_vertex, uv = quad_corners()
    normals = np.tile(np.float32([0, 0, 1]), (6, 1))

    def build(flip=False, to_world=None, fields=False):
        props = mi.Properties()
        props['flip_normals'] = flip
        if to_world is not None:
            props['to_world'] = to_world
        m = mi.Mesh(props)
        if fields:
            m.from_fields(faces=faces_of(build()), positions=positions[:4],
                          normals=normals[:4])
        else:
            m.from_corners(positions=positions, corner_vertex=corner_vertex,
                           normals=normals, texcoords=uv)
        return m

    plain, flipped = build(), build(flip=True)
    assert np.all(faces_of(flipped) == faces_of(plain)[:, ::-1])
    assert np.allclose(vertex_normals(flipped), -vertex_normals(plain))
    for f in range(plain.face_count()):
        dr.assert_allclose(flipped.face_normal(f), -plain.face_normal(f))

    # The flip survives a regeneration of the shading normals, because it
    # lives in the winding that the regeneration reads
    params = mi.traverse(flipped)
    params['positions'] = params['positions']
    params.update()
    assert np.allclose(vertex_normals(flipped), -vertex_normals(plain))

    # from_fields() bakes it the same way
    assert np.all(faces_of(build(flip=True, fields=True)) ==
                  faces_of(build(fields=True))[:, ::-1])

    # A mirroring 'to_world' reverses the winding as well, so specifying
    # both leaves it alone
    mirror = mi.ScalarAffineTransform4f().scale([-1, 1, 1])
    both = build(flip=True, to_world=mirror)
    assert np.all(faces_of(both) == faces_of(plain))
    assert np.allclose(vertex_normals(both), -vertex_normals(plain))


def test15_packed_build_rejects_pending_transform(variants_all_rgb):
    """from_packed() adopts its input as it is, so a mesh that still carries
    a 'to_world' or 'flip_normals' property is rejected rather than silently
    ignoring it. from_fields() only bakes the latter."""
    positions, corner_vertex, _ = quad_corners()
    faces = faces_of(mi.Mesh("q", faces=np.uint32([[0, 1, 2]]),
                             positions=positions[:4]))

    def props(**kwargs):
        p = mi.Properties()
        for k, v in kwargs.items():
            p[k] = v
        return p

    shift = mi.ScalarAffineTransform4f().translate([1, 2, 3])

    with pytest.raises(RuntimeError, match="preapply"):
        mi.Mesh(props(to_world=shift)).from_fields(
            faces=faces, positions=positions[:4])

    for p in (props(to_world=shift), props(flip_normals=True)):
        with pytest.raises(RuntimeError, match="caller's responsibility"):
            mi.Mesh(p).from_packed(
                layout=0, packed_faces=dr.zeros(mi.TensorXu, (1, 4)),
                packed_vertices=dr.zeros(mi.TensorXf, (3, 8)))
