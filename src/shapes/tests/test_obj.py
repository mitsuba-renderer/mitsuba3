import os

import pytest
import drjit as dr
import mitsuba as mi
import numpy as np


def load_obj(tmp_path, text, **props):
    fn = tmp_path / "mesh.obj"
    fn.write_bytes(text.encode() if isinstance(text, str) else text)
    return mi.load_dict({"type": "obj", "filename": str(fn), **props})


def corner_expand(mesh):
    """
    Return per-corner positions/normals/uvs, i.e. the mesh contents in a form
    that does not depend on the vertex numbering produced by the loader.
    """
    f = np.array(mesh.faces_buffer()).reshape(-1, 3)
    p = np.array(mesh.vertex_positions_buffer()).reshape(-1, 3)[f]
    n = uv = None
    if mesh.has_vertex_normals():
        n = np.array(mesh.vertex_normals_buffer()).reshape(-1, 3)[f]
    if mesh.has_vertex_texcoords():
        uv = np.array(mesh.vertex_texcoords_buffer()).reshape(-1, 2)[f]
    return p, n, uv


def test01_triangle(variant_scalar_rgb, tmp_path):
    m = load_obj(tmp_path, "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n")
    assert m.vertex_count() == 3 and m.face_count() == 1
    assert np.array_equal(np.array(m.faces_buffer()), [0, 1, 2])
    assert np.array_equal(np.array(m.vertex_positions_buffer()),
                          [0, 0, 0, 1, 0, 0, 0, 1, 0])
    # Smooth normals are computed when the file provides none
    assert m.has_vertex_normals() and not m.has_vertex_texcoords()
    n = np.array(m.vertex_normals_buffer()).reshape(-1, 3)
    assert np.allclose(n, [0, 0, 1], atol=1e-6)


def test02_quad_and_ngon_fan(variant_scalar_rgb, tmp_path):
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
    m = load_obj(tmp_path,
                 "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
                 "vt 0 0\nvt 1 0\nvt 0 1\n"
                 "vn 0 0 1\n"
                 "f 1/1/1 2/2/1 3/3/1\n")
    assert m.vertex_count() == 3 and m.face_count() == 1
    assert m.has_vertex_normals() and m.has_vertex_texcoords()
    p, n, uv = corner_expand(m)
    assert np.array_equal(p, [[[0, 0, 0], [1, 0, 0], [0, 1, 0]]])
    assert np.array_equal(n, [[[0, 0, 1]] * 3])
    # flip_tex_coords defaults to true: v becomes 1 - v
    assert np.array_equal(uv, [[[0, 1], [1, 1], [0, 0]]])


def test04_normals_only_faces(variant_scalar_rgb, tmp_path):
    m = load_obj(tmp_path,
                 "v 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 1\n"
                 "f 1//1 2//1 3//1\n")
    assert m.vertex_count() == 3
    assert m.has_vertex_normals() and not m.has_vertex_texcoords()
    _, n, _ = corner_expand(m)
    assert np.array_equal(n, [[[0, 0, 1]] * 3])


def test05_texcoords_only_faces(variant_scalar_rgb, tmp_path):
    m = load_obj(tmp_path,
                 "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
                 "vt 0 0\nvt 1 0\nvt 0 1\n"
                 "f 1/1 2/2 3/3\n")
    assert m.vertex_count() == 3
    assert m.has_vertex_texcoords()
    _, _, uv = corner_expand(m)
    assert np.array_equal(uv, [[[0, 1], [1, 1], [0, 0]]])


def test06_flip_tex_coords_disabled(variant_scalar_rgb, tmp_path):
    m = load_obj(tmp_path,
                 "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
                 "vt 0 0\nvt 1 0\nvt 0 1\n"
                 "f 1/1 2/2 3/3\n",
                 flip_tex_coords=False)
    _, _, uv = corner_expand(m)
    assert np.array_equal(uv, [[[0, 0], [1, 0], [0, 1]]])


def test07_uv_seam_splits_vertices(variant_scalar_rgb, tmp_path):
    # Two triangles share vertices 1 and 3. Vertex 1 uses a different
    # texture coordinate in each face and must be split; vertex 3 agrees
    # and must be welded.
    m = load_obj(tmp_path,
                 "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\n"
                 "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\nvt 0.5 0.5\n"
                 "f 1/1 2/2 3/3\n"
                 "f 1/5 3/3 4/4\n")
    assert m.vertex_count() == 5 and m.face_count() == 2
    p, _, uv = corner_expand(m)
    assert np.array_equal(p, [[[0, 0, 0], [1, 0, 0], [1, 1, 0]],
                              [[0, 0, 0], [1, 1, 0], [0, 1, 0]]])
    assert np.array_equal(uv, [[[0, 1], [1, 1], [1, 0]],
                               [[0.5, 0.5], [1, 0], [0, 0]]])


def test08_weld_identical_corners(variant_scalar_rgb, tmp_path):
    # All corner data agrees, so the shared edge welds
    m = load_obj(tmp_path,
                 "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\n"
                 "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
                 "f 1/1 2/2 3/3\n"
                 "f 1/1 3/3 4/4\n")
    assert m.vertex_count() == 4 and m.face_count() == 2


def test09_unreferenced_vertices_dropped(variant_scalar_rgb, tmp_path):
    m = load_obj(tmp_path,
                 "v 0 0 0\nv 1 0 0\nv 0 1 0\nv 5 5 5\nf 1 2 3\n")
    assert m.vertex_count() == 3
    # The bounding box only spans the vertices that survive welding
    assert dr.allclose(m.bbox().max, [1, 1, 0])


def test10_face_normals_flag(variant_scalar_rgb, tmp_path):
    m = load_obj(tmp_path,
                 "v 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 1\n"
                 "f 1//1 2//1 3//1\n",
                 face_normals=True)
    assert not m.has_vertex_normals()
    assert m.vertex_count() == 3 and m.face_count() == 1


def test11_to_world(variant_scalar_rgb, tmp_path):
    m = load_obj(tmp_path,
                 "v 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 1\n"
                 "f 1//1 2//1 3//1\n",
                 to_world=mi.ScalarTransform4f().translate([1, 2, 3]) @
                          mi.ScalarTransform4f().scale(2))
    p = np.array(m.vertex_positions_buffer()).reshape(-1, 3)
    assert np.allclose(p, [[1, 2, 3], [3, 2, 3], [1, 4, 3]])
    n = np.array(m.vertex_normals_buffer()).reshape(-1, 3)
    assert np.allclose(n, [[0, 0, 1]] * 3)


def test12_line_ending_robustness(variant_scalar_rgb, tmp_path):
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
    with pytest.raises(RuntimeError):
        load_obj(tmp_path, "v 0 0\nf 1 2 3\n")  # missing coordinate
    with pytest.raises(RuntimeError):
        load_obj(tmp_path, "v a b c\n")  # non-numeric coordinates


def test15_degenerate_face_lines(variant_scalar_rgb, tmp_path):
    # A face with fewer than three vertices contributes no triangle
    m = load_obj(tmp_path,
                 "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2\nf 1 2 3\n")
    assert m.face_count() == 1


def test16_ray_intersect(variant_scalar_rgb, tmp_path):
    m_file = tmp_path / "mesh.obj"
    m_file.write_text("v -1 -1 0\nv 1 -1 0\nv 1 1 0\nv -1 1 0\n"
                      "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
                      "f 1/1 2/2 3/3 4/4\n")
    scene = mi.load_dict({
        "type": "scene",
        "mesh": {"type": "obj", "filename": str(m_file),
                 "flip_tex_coords": False},
    })
    si = scene.ray_intersect(mi.Ray3f([0.25, -0.5, -5], [0, 0, 1]))
    assert si.is_valid()
    assert dr.allclose(si.t, 5)
    assert dr.allclose(si.uv, [0.625, 0.25])


def test17_golden_reference(variant_scalar_rgb, tmp_path):
    # Golden data captured from the loader before it switched to the shared
    # Mesh.build_from_corners welding backend. The comparison uses per-corner
    # expanded buffers because the two implementations number the welded
    # vertices differently.
    m = load_obj(tmp_path,
                 "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nv 2 0 0\nv 2 1 0\n"
                 "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\nvt 0.25 0.25\n"
                 "vn 0 0 1\nvn 0 1 0\n"
                 "f 1/1/1 2/2/1 3/3/1 4/4/1\n"
                 "f 2/2/1 5/5/1 6/3/2 3/3/1\n"
                 "f 1/5/2 2/2/1 4/4/2\n")
    assert m.vertex_count() == 8 and m.face_count() == 5
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


def test18_large_grid_file(variant_scalar_rgb):
    fn = os.environ.get("MITSUBA_TEST_GRID_OBJ")
    if fn is None or not os.path.exists(fn):
        pytest.skip("set MITSUBA_TEST_GRID_OBJ to run the large-file test")
    m = mi.load_dict({"type": "obj", "filename": fn})
    assert m.vertex_count() == 1440000
    assert m.face_count() == 2875202
