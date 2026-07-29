"""
Tests of mesh file I/O: the OBJ, PLY and serialized loaders (including
legacy format versions and to_world transforms) and the PLY/serialized
writers.
"""

import struct
import zlib

import numpy as np
import pytest
import drjit as dr
import mitsuba as mi

from mitsuba.scalar_rgb.test.util import fresolver_append_path, \
    quad_corners, vertex_positions, vertex_normals, faces_of, face_records, \
    anisotropic_bsdf, ANISOTROPIC_BSDF


# -------------------------------------------------------------------
# Loaders
# -------------------------------------------------------------------

@fresolver_append_path
@pytest.mark.parametrize('mesh_format', ['obj', 'ply', 'serialized'])
@pytest.mark.parametrize('features', ['normals', 'uv', 'normals_uv'])
@pytest.mark.parametrize('face_normals', [True, False])
def test01_load_feature_matrix(variant_scalar_rgb, mesh_format, features,
                               face_normals):
    """The loaders reproduce the values stored in the file across all
    format and feature combinations."""
    shape = mi.load_dict({
        "type": mesh_format,
        "filename": f"resources/data/tests/{mesh_format}/rectangle_{features}.{mesh_format}",
        "face_normals": face_normals,
    })
    assert shape.has_normals() == (not face_normals)

    positions = vertex_positions(shape)

    # Identify the corners by position value rather than by index
    def vertex(p):
        match = np.where(np.all(np.abs(positions - p) < 1e-3, axis=1))[0]
        assert len(match) >= 1
        return match[0]

    v0 = vertex([-2.85, 0.0, -7.6])
    v2 = vertex([2.85, 0.0, 0.6])
    v3 = vertex([2.85, 0.0, -7.6])

    if 'uv' in features:
        assert shape.has_texcoords()
        texcoords = np.array(shape.texcoords())
        (uv0, uv2, uv3) = [texcoords[i] for i in [v0, v2, v3]]
        # For OBJs (and .serialized generated from OBJ), UV.y is flipped
        if mesh_format in ['obj', 'serialized']:
            assert np.allclose(uv0, [0.950589, 1 - 0.988416], atol=1e-3)
            assert np.allclose(uv2, [0.025105, 1 - 0.689127], atol=1e-3)
            assert np.allclose(uv3, [0.950589, 1 - 0.689127], atol=1e-3)
        else:
            assert np.allclose(uv0, [0.950589, 0.988416], atol=1e-3)
            assert np.allclose(uv2, [0.025105, 0.689127], atol=1e-3)
            assert np.allclose(uv3, [0.950589, 0.689127], atol=1e-3)

    if shape.has_normals():
        normals = vertex_normals(shape)
        for i in [v0, v2, v3]:
            assert np.allclose(normals[i], [0.0, 1.0, 0.0])


@fresolver_append_path
def test02_obj_quads_and_mixed_faces(variant_scalar_rgb, tmp_path):
    """OBJ quads split along the 0-2 diagonal and dangling lines are
    skipped. Corners without a vt read as zero UVs, while distinct vt
    indices with equal values weld into one vertex."""
    # vt 5 duplicates vt 2, so the corners 2/2 and 2/5 carry distinct
    # indices with equal values
    obj = """
v 0 0 0
v 1 0 0
v 1 1 0
v 0 1 0
v 0 0 1
vt 0 0
vt 1 0
vt 1 1
vt 0 1
vt 1 0
f 1/1 2/2 3/3 4/4
f 2 1
f 1// 2/5 5//
"""
    path = tmp_path / "quad.obj"
    path.write_text(obj)
    m = mi.load_dict({"type": "obj", "filename": str(path)})

    # Quad + triangle; quads split along the 0-2 diagonal
    assert m.face_count() == 3
    faces = faces_of(m)
    pos = vertex_positions(m)
    quad_pts = pos[faces[:2].ravel()]
    assert np.all(quad_pts[[0, 3]] == quad_pts[0])  # both start at corner 1

    # Vertex 2 welds across the two vt indices; vertex 1 splits since its
    # triangle corner has no vt and (0, 0) differs from its flipped uv
    assert m.vertex_count() == 6
    assert faces[0][1] == faces[2][1]

    # Corners without a vt produce zero UVs rather than garbage
    uv = np.array(m.texcoords())
    tri = faces[2]
    assert np.all(uv[tri[0]] == (0, 0)) and np.all(uv[tri[2]] == (0, 0))


@fresolver_append_path
def test03_ply_stored_attribute(variant_scalar_rgb):
    """Extra PLY properties become custom mesh attributes."""
    m = mi.load_dict({
        "type": "ply",
        "filename": "resources/data/tests/ply/triangle_face_colors.ply",
    })
    assert m.has_mesh_attributes()
    assert dr.all(m.has_attribute("face_color"))
    attr = np.array(m.attribute("face_color"))
    assert attr.shape == (1, 3)


@fresolver_append_path
@pytest.mark.parametrize('mesh_format', ['obj', 'ply'])
def test04_flip_tex_coords(variants_all_rgb, mesh_format):
    """The flip_tex_coords flag mirrors the V coordinate on load."""
    dicts = [{
        'type': mesh_format,
        'filename': f'resources/data/tests/{mesh_format}/rectangle_uv.{mesh_format}',
        'flip_tex_coords': flip
    } for flip in (False, True)]
    uv, uv_flipped = [mi.traverse(mi.load_dict(d))['texcoords'].numpy()
                      for d in dicts]

    assert (uv[:, 0] == uv_flipped[:, 0]).all()
    assert (uv[:, 1] == 1 - uv_flipped[:, 1]).all()


# -------------------------------------------------------------------
# PLY output
# -------------------------------------------------------------------

@fresolver_append_path
@pytest.mark.parametrize('target', ['file', 'stream'])
def test05_write_ply_roundtrip(variants_all_rgb, tmp_path, target):
    """A PLY round trip preserves edited positions and custom attributes.
    Pending JIT state is evaluated before writing."""
    filepath = str(tmp_path / 'roundtrip.ply')
    mesh = mi.load_dict({
        'type': 'ply',
        'filename': 'resources/data/tests/ply/rectangle_normals_uv.ply'
    })
    params = mi.traverse(mesh)
    positions = type(params['positions'])(params['positions'])

    # Modify one buffer, to check that pending JIT state is evaluated
    params['positions'] = params['positions'] + 10
    params.update()
    # Add a mesh attribute, to check that they are properly migrated in CUDA
    # modes
    mesh.add_attribute('vertex_test', [[1], [2], [3], [4]])

    if target == 'file':
        mesh.write_ply(filepath)
    else:
        fs = mi.FileStream(filepath, mi.FileStream.ETruncReadWrite)
        mesh.write_ply(fs)
        fs.close()

    mesh_saved = mi.load_dict({'type': 'ply', 'filename': filepath})
    params_saved = mi.traverse(mesh_saved)

    dr.assert_allclose(params_saved['positions'], positions + 10.0)
    assert 'vertex_test' in params_saved and \
           dr.allclose(params_saved['vertex_test'].array, [1, 2, 3, 4])


def test06_write_ply_flattens_shared_points(variant_scalar_rgb, tmp_path):
    """PLY is single-indexed, so meshes with seams export their flattened
    vertices and a warning. The coarse maps do not survive."""
    positions, corner_vertex, uv = quad_corners(seam=True)
    m = mi.Mesh("quad")
    m.from_corners(positions=positions, corner_vertex=corner_vertex,
                 texcoords=uv)
    assert m.position_count() < m.vertex_count()

    m.write_ply(str(tmp_path / "seam.ply"))
    m2 = mi.load_dict({"type": "ply",
                       "filename": str(tmp_path / "seam.ply")})
    assert m2.face_count() == 2
    assert m2.vertex_count() == m.vertex_count()
    assert m2.position_count() == m2.vertex_count()


def test07_write_ply_tangent_packed(variants_all_rgb, tmp_path):
    """The PLY writer decodes packed tangent frames, so the stored
    normals round trip to encoding precision."""
    rng = np.random.RandomState(0)
    n = rng.randn(6, 3)
    n = (n / np.linalg.norm(n, axis=1, keepdims=True)).astype(np.float32)
    positions = rng.randn(6, 3).astype(np.float32)
    uv = np.tile([[0, 0], [1, 0], [0, 1]], (2, 1)).astype(np.float32)

    m = mi.Mesh("t")
    m.from_fields(faces=np.arange(6, dtype=np.uint32).reshape(2, 3),
                  positions=positions, normals=n, texcoords=uv)
    m.set_bsdf(anisotropic_bsdf())
    assert m.packs_tangent()

    fname = str(tmp_path / "aniso.ply")
    m.write_ply(fname)
    m2 = mi.load_dict({'type': 'ply', 'filename': fname})
    assert np.allclose(vertex_normals(m2), n, atol=1e-5)
    assert np.allclose(np.array(m2.texcoords()), uv)


# -------------------------------------------------------------------
# Serialized (v5) output
# -------------------------------------------------------------------

def test08_write_serialized_roundtrip(variants_all_rgb, tmp_path):
    """A round trip to the serialized format preserves every level of the
    representation exactly. Generated normals remain regenerable."""
    positions, corner_vertex, uv = quad_corners(seam=True)
    col = np.arange(18, dtype=np.float32).reshape(6, 3)
    m = mi.Mesh("quad")
    m.from_corners(positions=positions, corner_vertex=corner_vertex,
                   texcoords=uv, attrs={"vertex_color": col})
    m.add_attribute("face_id", [[3], [7]])

    fname = str(tmp_path / "mesh.serialized")
    m.write_serialized(fname)
    m2 = mi.load_dict({'type': 'serialized', 'filename': fname})

    assert m2.vertex_count() == m.vertex_count()
    assert m2.position_count() == m.position_count()
    assert m2.normal_count() == m.normal_count()
    for get in [lambda x: face_records(x),
                lambda x: x.position_index(),
                lambda x: x.normal_index(),
                lambda x: x.positions(),
                lambda x: x.texcoords(),
                lambda x: x.attribute("vertex_color"),
                lambda x: x.attribute("face_id")]:
        assert np.array_equal(np.array(get(m)), np.array(get(m2)))

    # Loading re-normalizes the stored normals
    assert np.allclose(np.array(m2.normals()), np.array(m.normals()),
                       atol=1e-6)

    # Tangents regenerate from the identical inputs
    assert m2.has_tangents()
    assert np.allclose(np.array(m2.tangents()), np.array(m.tangents()),
                       atol=1e-6)


@pytest.mark.parametrize('scale', [[1, 1, 1], [1, 2, 4]])
def test09_serialized_to_world(variants_all_rgb, tmp_path, scale):
    """A to_world transforms the stored positions/normals/tangents: the
    positions through the matrix, the normals through its inverse
    transpose."""
    positions, corner_vertex, uv = quad_corners(seam=True)
    positions[:, 2] = positions[:, 0] * positions[:, 1]  # bend the quad
    fname = str(tmp_path / "mesh.serialized")
    T = mi.ScalarTransform4f().translate([1, 2, 3]).scale(scale)

    def build(p, **kwargs):
        m = mi.Mesh("quad")
        m.from_corners(positions=p, corner_vertex=corner_vertex,
                       texcoords=uv, **kwargs)
        return m

    m = build(positions)
    m.write_serialized(fname)
    m2 = mi.load_dict({'type': 'serialized', 'filename': fname,
                       'to_world': T})
    assert np.array_equal(np.array(m2.position_index()),
                          np.array(m.position_index()))
    assert np.array_equal(face_records(m2), face_records(m))

    moved = positions * np.float32(scale) + np.float32([1, 2, 3])
    assert np.allclose(vertex_positions(m2), vertex_positions(build(moved)),
                       atol=1e-6)

    # Stored normals are data: they are transported, not regenerated. This
    # is where a non-uniform scale becomes visible.
    n_ref = vertex_normals(m) / np.float32(scale)
    n_ref /= np.linalg.norm(n_ref, axis=1, keepdims=True)
    assert np.allclose(vertex_normals(m2), n_ref, atol=1e-6)

    # The regenerated tangents stay perpendicular to the transformed frame
    tan = np.array(m2.tangents())
    nrm = vertex_normals(m2)
    assert np.abs((tan * nrm).sum(axis=1)).max() < 1e-5

    # With a material that packs the tangent frame, the file stores encoded
    # frames, which a rotation must decode, transform, and re-encode
    m.set_bsdf(anisotropic_bsdf())
    assert m.packs_tangent()
    m.write_serialized(fname)
    m4 = mi.load_dict({'type': 'serialized', 'filename': fname,
                       'to_world': mi.ScalarTransform4f().rotate([0, 0, 1], 90),
                       'bsdf': ANISOTROPIC_BSDF})
    def rot_z(a):
        return np.stack([-a[:, 1], a[:, 0], a[:, 2]], axis=1)

    assert np.allclose(np.array(m4.tangents()), rot_z(np.array(m.tangents())),
                       atol=1e-5)
    assert np.allclose(vertex_normals(m4), rot_z(vertex_normals(m)),
                       atol=1e-5)


def test10_write_serialized_multiple(variant_scalar_rgb, tmp_path):
    """Multiple serialized meshes concatenate into one file addressed
    through shape_index."""
    positions, corner_vertex, uv = quad_corners()
    m0 = mi.Mesh("first")
    m0.from_corners(positions=positions, corner_vertex=corner_vertex)
    m1 = mi.Mesh("second")
    m1.from_corners(positions=positions + 1, corner_vertex=corner_vertex,
                  texcoords=uv)

    fname = str(tmp_path / "multi.serialized")
    stream = mi.FileStream(fname, mi.FileStream.EMode.ETruncReadWrite)
    offsets = []
    for m in (m0, m1):
        offsets.append(stream.tell())
        m.write_serialized(stream)
    for o in offsets:
        stream.write_uint64(o)
    stream.write_uint32(len(offsets))
    stream.close()

    for i, m in enumerate((m0, m1)):
        m2 = mi.load_dict({'type': 'serialized', 'filename': fname,
                           'shape_index': i})
        assert np.array_equal(vertex_positions(m2), vertex_positions(m))
        assert m2.has_texcoords() == m.has_texcoords()

    with pytest.raises(Exception, match='out of range'):
        mi.load_dict({'type': 'serialized', 'filename': fname,
                      'shape_index': len(offsets)})


# -------------------------------------------------------------------
# Legacy serialized input (versions 3 and 4)
# -------------------------------------------------------------------

def _write_legacy_serialized(path, version, double_precision, positions,
                             normals, texcoords, colors, faces):
    """Hand-assemble a single-mesh legacy .serialized file"""
    flags = 0x2000 if double_precision else 0
    if normals is not None:
        flags |= 0x0001
    if texcoords is not None:
        flags |= 0x0002
    if colors is not None:
        flags |= 0x0008
    fmt = 'd' if double_precision else 'f'

    payload = struct.pack('<I', flags)
    if version == 4:
        payload += b'legacy\x00'
    payload += struct.pack('<QQ', len(positions), len(faces))

    def arr(a):
        flat = np.asarray(a).ravel()
        return struct.pack(f'<{flat.size}{fmt}', *flat.tolist())

    payload += arr(positions)
    if normals is not None:
        payload += arr(normals)
    if texcoords is not None:
        payload += arr(texcoords)
    if colors is not None:
        payload += arr(colors)
    flat_faces = np.asarray(faces).ravel()
    payload += struct.pack(f'<{flat_faces.size}I', *flat_faces.tolist())

    with open(path, 'wb') as f:
        f.write(struct.pack('<HH', 0x041C, version))
        f.write(zlib.compress(payload))


@pytest.mark.parametrize('version', [3, 4])
@pytest.mark.parametrize('double_precision', [False, True])
@pytest.mark.parametrize('colors', [False, True])
def test11_serialized_legacy_versions(variants_all_rgb, tmp_path, version,
                                      double_precision, colors):
    """Legazy serialized files remain readable. The single-indexed data loads
    with every vertex forming its own surface point."""
    positions = np.float64([[0, 0, 0], [1, 0, 0], [1, 1, 0.5], [0, 1, 0]])
    normals = np.float64([[0, 0, 1], [0, 0, 1], [0, 1, 1], [0, 0, 1]])
    normals /= np.linalg.norm(normals, axis=1, keepdims=True)
    texcoords = np.float64([[0, 0], [1, 0], [1, 1], [0, 1]])
    faces = np.uint32([[0, 1, 2], [0, 2, 3]])

    fname = str(tmp_path / f"legacy_v{version}.serialized")
    _write_legacy_serialized(fname, version, double_precision, positions,
                             normals, texcoords,
                             np.full((4, 3), 0.5) if colors else None, faces)

    m = mi.load_dict({'type': 'serialized', 'filename': fname})
    assert m.vertex_count() == 4 and m.face_count() == 2
    assert m.position_count() == 4  # single-indexed
    assert np.allclose(vertex_positions(m), positions, atol=1e-6)
    assert np.allclose(vertex_normals(m), normals, atol=1e-6)
    assert np.allclose(np.array(m.texcoords()), texcoords, atol=1e-6)
    assert np.array_equal(faces_of(m), faces)

    # face_normals discards the stored normals
    m = mi.load_dict({'type': 'serialized', 'filename': fname,
                      'face_normals': True})
    assert not m.has_normals()
