"""
Regression tests for the Wavefront OBJ loader: fan triangulation, the
``v``/``vt``/``vn`` face format variants, corner welding and UV seam splitting,
index validation, and line ending robustness.
"""

import pytest
import drjit as dr
import mitsuba as mi
import numpy as np

from mitsuba.scalar_rgb.test.util import vertex_positions, vertex_normals, \
    faces_of


def load_obj(tmp_path, text, **props):
    fn = tmp_path / "mesh.obj"
    fn.write_bytes(text.encode() if isinstance(text, str) else text)
    return mi.load_dict({"type": "obj", "filename": str(fn), **props})


def corner_expand(mesh):
    """
    Return per-corner positions/normals/uvs, i.e. the mesh contents in a form
    that does not depend on the vertex numbering produced by the loader.
    """
    f = faces_of(mesh)
    p = vertex_positions(mesh)[f]
    n = vertex_normals(mesh)[f] if mesh.has_normals() else None
    uv = np.array(mesh.texcoords())[f] if mesh.has_texcoords() else None
    return p, n, uv


def test01_triangle(variant_scalar_rgb, tmp_path):
    """A minimal ``v``/``f`` file, with smooth normals generated when absent"""
    m = load_obj(tmp_path, "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n")
    assert m.vertex_count() == 3 and m.face_count() == 1
    assert np.array_equal(faces_of(m), [[0, 1, 2]])
    assert np.array_equal(vertex_positions(m),
                          [[0, 0, 0], [1, 0, 0], [0, 1, 0]])
    assert m.has_normals() and not m.has_texcoords()
    assert np.allclose(vertex_normals(m), [0, 0, 1], atol=1e-6)


def test02_quad_and_ngon_fan(variant_scalar_rgb, tmp_path):
    """Quads and n-gons are triangulated as a fan around the first vertex"""
    m = load_obj(tmp_path,
                 "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nf 1 2 3 4\n")
    assert m.vertex_count() == 4 and m.face_count() == 2
    p, _, _ = corner_expand(m)
    assert np.array_equal(p, [[[0, 0, 0], [1, 0, 0], [1, 1, 0]],
                              [[0, 0, 0], [1, 1, 0], [0, 1, 0]]])

    m = load_obj(tmp_path,
                 "v 0 0 0\nv 1 0 0\nv 2 1 0\nv 1 2 0\nv 0 1 0\n"
                 "f 1 2 3 4 5\n")
    assert m.vertex_count() == 5 and m.face_count() == 3
    p, _, _ = corner_expand(m)
    assert np.array_equal(p, [[[0, 0, 0], [1, 0, 0], [2, 1, 0]],
                              [[0, 0, 0], [2, 1, 0], [1, 2, 0]],
                              [[0, 0, 0], [1, 2, 0], [0, 1, 0]]])


def test03_full_face_format(variant_scalar_rgb, tmp_path):
    """The ``v/vt/vn`` face format supplies all three attributes"""
    m = load_obj(tmp_path,
                 "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
                 "vt 0 0\nvt 1 0\nvt 0 1\n"
                 "vn 0 0 1\n"
                 "f 1/1/1 2/2/1 3/3/1\n")
    assert m.vertex_count() == 3 and m.face_count() == 1
    assert m.has_normals() and m.has_texcoords()
    p, n, uv = corner_expand(m)
    assert np.array_equal(p, [[[0, 0, 0], [1, 0, 0], [0, 1, 0]]])
    assert np.array_equal(n, [[[0, 0, 1]] * 3])
    # flip_tex_coords defaults to true: v becomes 1 - v
    assert np.array_equal(uv, [[[0, 1], [1, 1], [0, 0]]])


def test04_normals_only_faces(variant_scalar_rgb, tmp_path):
    """Faces in ``v//vn`` form supply normals only"""
    m = load_obj(tmp_path,
                 "v 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 1\n"
                 "f 1//1 2//1 3//1\n")
    assert m.vertex_count() == 3
    assert m.has_normals() and not m.has_texcoords()
    _, n, _ = corner_expand(m)
    assert np.array_equal(n, [[[0, 0, 1]] * 3])


def test05_texcoords_only_faces(variant_scalar_rgb, tmp_path):
    """Faces in ``v/vt`` form supply texture coordinates only"""
    m = load_obj(tmp_path,
                 "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
                 "vt 0 0\nvt 1 0\nvt 0 1\n"
                 "f 1/1 2/2 3/3\n")
    assert m.vertex_count() == 3
    assert m.has_texcoords()
    _, _, uv = corner_expand(m)
    assert np.array_equal(uv, [[[0, 1], [1, 1], [0, 0]]])


def test06_flip_tex_coords_disabled(variant_scalar_rgb, tmp_path):
    """``flip_tex_coords=False`` keeps the ``v`` coordinate as written"""
    m = load_obj(tmp_path,
                 "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
                 "vt 0 0\nvt 1 0\nvt 0 1\n"
                 "f 1/1 2/2 3/3\n",
                 flip_tex_coords=False)
    _, _, uv = corner_expand(m)
    assert np.array_equal(uv, [[[0, 0], [1, 0], [0, 1]]])


def test07_uv_seam_splits_vertices(variant_scalar_rgb, tmp_path):
    """A corner with disagreeing texture coordinates splits in two"""
    m = load_obj(tmp_path,
                 "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\n"
                 "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\nvt 0.5 0.5\n"
                 "f 1/1 2/2 3/3\n"
                 "f 1/5 3/3 4/4\n")
    assert m.vertex_count() == 5 and m.position_count() == 4
    assert m.face_count() == 2
    p, _, uv = corner_expand(m)
    assert np.array_equal(p, [[[0, 0, 0], [1, 0, 0], [1, 1, 0]],
                              [[0, 0, 0], [1, 1, 0], [0, 1, 0]]])
    assert np.array_equal(uv, [[[0, 1], [1, 1], [1, 0]],
                               [[0.5, 0.5], [1, 0], [0, 0]]])


def test08_weld_identical_corners(variant_scalar_rgb, tmp_path):
    """Corners that agree in every attribute weld into one vertex"""
    m = load_obj(tmp_path,
                 "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\n"
                 "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
                 "f 1/1 2/2 3/3\n"
                 "f 1/1 3/3 4/4\n")
    assert m.vertex_count() == 4 and m.face_count() == 2


def test09_unreferenced_vertices_dropped(variant_scalar_rgb, tmp_path):
    """Vertices that no face references are dropped"""
    m = load_obj(tmp_path,
                 "v 0 0 0\nv 1 0 0\nv 0 1 0\nv 5 5 5\nf 1 2 3\n")
    assert m.vertex_count() == 3
    # The bounding box only spans the vertices that survive welding
    assert dr.allclose(m.bbox().max, [1, 1, 0])


def test10_face_normals_flag(variant_scalar_rgb, tmp_path):
    """``face_normals=True`` discards the normals declared in the file"""
    m = load_obj(tmp_path,
                 "v 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 1\n"
                 "f 1//1 2//1 3//1\n",
                 face_normals=True)
    assert not m.has_normals()
    assert m.vertex_count() == 3 and m.face_count() == 1


def test11_to_world(variant_scalar_rgb, tmp_path):
    """``to_world`` transforms positions and normals at load time"""
    m = load_obj(tmp_path,
                 "v 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 1\n"
                 "f 1//1 2//1 3//1\n",
                 to_world=mi.ScalarTransform4f().translate([1, 2, 3]) @
                          mi.ScalarTransform4f().scale(2))
    assert np.allclose(vertex_positions(m), [[1, 2, 3], [3, 2, 3], [1, 4, 3]])
    assert np.allclose(vertex_normals(m), [[0, 0, 1]] * 3)


def test12_line_ending_robustness(variant_scalar_rgb, tmp_path):
    """CRLF endings, a missing final newline and stray directives all parse"""
    # CRLF line endings
    m = load_obj(tmp_path, "v 0 0 0\r\nv 1 0 0\r\nv 0 1 0\r\nf 1 2 3\r\n")
    assert m.vertex_count() == 3 and m.face_count() == 1

    # Missing trailing newline
    m = load_obj(tmp_path, "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3")
    assert m.vertex_count() == 3 and m.face_count() == 1

    # Blank lines, comments, unknown directives, extra whitespace
    m = load_obj(tmp_path,
                 "# comment\no object\ng group\ns off\nusemtl foo\n\n"
                 "  v  0   0  0\n\tv 1 0 0\nv 0 1 0\n\nf  1   2  3\n\n")
    assert m.vertex_count() == 3 and m.face_count() == 1


def test13_invalid_indices(variant_scalar_rgb, tmp_path):
    """Zero, negative and out-of-range face indices are rejected"""
    # Negative (relative) indices are not supported
    with pytest.raises(RuntimeError):
        load_obj(tmp_path, "v 0 0 0\nv 1 0 0\nv 0 1 0\nf -3 -2 -1\n")

    # OBJ indices are one-based
    with pytest.raises(RuntimeError):
        load_obj(tmp_path, "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 0 1 2\n")

    # References past the end of the vertex/texcoord/normal lists
    with pytest.raises(RuntimeError):
        load_obj(tmp_path, "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 4\n")
    with pytest.raises(RuntimeError):
        load_obj(tmp_path, "v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\n"
                           "f 1/1 2/9 3/1\n")
    with pytest.raises(RuntimeError):
        load_obj(tmp_path, "v 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 1\n"
                           "f 1//1 2//9 3//1\n")


def test14_malformed_input(variant_scalar_rgb, tmp_path):
    """Vertex lines with missing or non-numeric coordinates are rejected"""
    with pytest.raises(RuntimeError):
        load_obj(tmp_path, "v 0 0\nf 1 2 3\n")  # missing coordinate
    with pytest.raises(RuntimeError):
        load_obj(tmp_path, "v a b c\n")  # non-numeric coordinates


def test15_degenerate_face_lines(variant_scalar_rgb, tmp_path):
    """A face with fewer than three vertices contributes no triangle"""
    m = load_obj(tmp_path,
                 "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2\nf 1 2 3\n")
    assert m.face_count() == 1


def test16_mixed_faces(variant_scalar_rgb, tmp_path):
    """Mixed face formats weld or split corners as their attributes require"""
    m = load_obj(tmp_path,
                 "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nv 2 0 0\nv 2 1 0\n"
                 "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\nvt 0.25 0.25\n"
                 "vn 0 0 1\nvn 0 1 0\n"
                 "f 1/1/1 2/2/1 3/3/1 4/4/1\n"
                 "f 2/2/1 5/5/1 6/3/2 3/3/1\n"
                 "f 1/5/2 2/2/1 4/4/2\n")
    # Source vertices 2 and 6 additionally split because their triangles
    # disagree on the UV orientation and cannot share a tangent frame
    assert m.vertex_count() == 10 and m.position_count() == 6
    assert m.face_count() == 5
    p, n, uv = corner_expand(m)
    assert np.array_equal(p, [
        [[0, 0, 0], [1, 0, 0], [1, 1, 0]],
        [[0, 0, 0], [1, 1, 0], [0, 1, 0]],
        [[1, 0, 0], [2, 0, 0], [2, 1, 0]],
        [[1, 0, 0], [2, 1, 0], [1, 1, 0]],
        [[0, 0, 0], [1, 0, 0], [0, 1, 0]]])
    assert np.array_equal(n, [
        [[0, 0, 1], [0, 0, 1], [0, 0, 1]],
        [[0, 0, 1], [0, 0, 1], [0, 0, 1]],
        [[0, 0, 1], [0, 0, 1], [0, 1, 0]],
        [[0, 0, 1], [0, 1, 0], [0, 0, 1]],
        [[0, 1, 0], [0, 0, 1], [0, 1, 0]]])
    assert np.array_equal(uv, [
        [[0, 1], [1, 1], [1, 0]],
        [[0, 1], [1, 0], [0, 0]],
        [[1, 1], [0.25, 0.75], [1, 0]],
        [[1, 1], [1, 0], [1, 0]],
        [[0.25, 0.75], [1, 1], [0, 0]]])
