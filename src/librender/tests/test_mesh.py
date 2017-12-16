from mitsuba.core import Struct, float_dtype
from mitsuba.core.xml import load_string
from mitsuba.render import Mesh
from mitsuba.test.util import fresolver_append_path
import numpy as np
import os


def test01_create_mesh():
    vertex_struct = Struct() \
        .append("x", float_dtype) \
        .append("y", float_dtype) \
        .append("z", float_dtype)

    index_struct = Struct() \
        .append("i0", Struct.EUInt32) \
        .append("i1", Struct.EUInt32) \
        .append("i2", Struct.EUInt32)
    m = Mesh("MyMesh", vertex_struct, 3, index_struct, 2)
    v = m.vertices()
    v[0] = np.array([0, 0, 0], dtype=float_dtype)
    v[1] = np.array([0, 0, 1], dtype=float_dtype)
    v[2] = np.array([0, 1, 0], dtype=float_dtype)
    m.recompute_bbox()

    if float_dtype == np.float32:
        assert str(m) == """Mesh[
  name = "MyMesh",
  bbox = BoundingBox3f[
    min = [0, 0, 0],
    max = [0, 1, 1]
  ],
  vertex_struct = Struct<12>[
    float32 x; // @0
    float32 y; // @4
    float32 z; // @8
  ],
  vertex_count = 3,
  vertices = [36 B of vertex data],
  vertex_normals = 0,
  face_struct = Struct<12>[
    uint32 i0; // @0
    uint32 i1; // @4
    uint32 i2; // @8
  ],
  face_count = 2,
  faces = [24 B of face data]
]"""
    else:
        assert str(m) == """Mesh[
  name = "MyMesh",
  bbox = BoundingBox3f[
    min = [0, 0, 0],
    max = [0, 1, 1]
  ],
  vertex_struct = Struct<24>[
    float64 x; // @0
    float64 y; // @8
    float64 z; // @16
  ],
  vertex_count = 3,
  vertices = [72 B of vertex data],
  vertex_normals = 0,
  face_struct = Struct<12>[
    uint32 i0; // @0
    uint32 i1; // @4
    uint32 i2; // @8
  ],
  face_count = 2,
  faces = [24 B of face data]
]"""


@fresolver_append_path
def test02_ply_triangle():
    shape = load_string("""
        <scene version="0.5.0">
            <shape type="ply">
                <string name="filename" value="data/triangle.ply"/>
                <boolean name="face_normals" value="true"/>
            </shape>
        </scene>
    """).kdtree()[0]

    vertices, faces = shape.vertices(), shape.faces()
    assert not shape.has_vertex_normals()
    assert vertices.ndim == 1
    assert vertices.shape == (3, )
    assert np.all(vertices['x'] == np.float32([0, 0, 0]))
    assert np.all(vertices['y'] == np.float32([0, 0, 1]))
    assert np.all(vertices['z'] == np.float32([0, 1, 0]))
    assert faces.ndim == 1
    assert faces.shape == (1, )
    assert faces[0]['i0'] == np.uint32(0)
    assert faces[0]['i1'] == np.uint32(1)
    assert faces[0]['i2'] == np.uint32(2)

@fresolver_append_path
def test03_ply_computed_normals():
    """Checks(automatic) vertex normal computation for a PLY file that
    doesn't have them."""
    shape = load_string("""
        <scene version="0.5.0">
            <shape type="ply">
                <string name="filename" value="data/triangle.ply"/>
            </shape>
        </scene>
    """).kdtree()[0]
    vertices = shape.vertices()
    assert shape.has_vertex_normals()
    # Normals are stored in half precision
    assert np.all(vertices['nx'] == np.float16([-1, -1, -1]))
    assert np.all(vertices['ny'] == np.float16([0, 0, 0]))
    assert np.all(vertices['nz'] == np.float16([0, 0, 0]))


def test04_normal_weighting_scheme():
    """Tests the weighting scheme that is used to compute surface normals."""
    vertex_struct = Struct() \
        .append("x", float_dtype) \
        .append("y", float_dtype) \
        .append("z", float_dtype) \
        .append("nx", Struct.EFloat16) \
        .append("ny", Struct.EFloat16) \
        .append("nz", Struct.EFloat16)

    index_struct = Struct() \
        .append("i0", Struct.EUInt32) \
        .append("i1", Struct.EUInt32) \
        .append("i2", Struct.EUInt32)

    m = Mesh("MyMesh", vertex_struct, 5, index_struct, 2)
    v = m.vertices()

    a, b = 1.0, 0.5
    v['x'] = np.array([0, -a, a, -b, b], dtype=float_dtype)
    v['y'] = np.array([0,  1, 1,  0, 0], dtype=float_dtype)
    v['z'] = np.array([0,  0, 0,  1, 1], dtype=float_dtype)

    n0 = np.array([0.0, 0.0, -1.0])
    n1 = np.array([0.0, 1.0, 0.0])
    angle_0 = np.pi / 2.0
    angle_1 = np.arccos(3.0 / 5.0)
    n2 = n0 * angle_0 + n1 * angle_1
    n2 /= np.linalg.norm(n2)
    n = np.vstack([n2, n0, n0, n1, n1])

    f = m.faces()
    f[0] = np.array([0, 1, 2], dtype=np.uint32)
    f[1] = np.array([0, 3, 4], dtype=np.uint32)
    m.recompute_vertex_normals()
    assert np.allclose(v['nx'], n[:, 0], 5e-4)
    assert np.allclose(v['ny'], n[:, 1], 5e-4)
    assert np.allclose(v['nz'], n[:, 2], 5e-4)


@fresolver_append_path
def test05_obj():
    """Tests the OBJ loader on a simple example """
    shape = load_string("""
        <scene version="0.5.0">
            <shape type="obj">
                <string name="filename" value="resources/data/tests/obj/cbox_smallbox.obj"/>
            </shape>
        </scene>
    """).kdtree()[0]

    vertices, faces = shape.vertices(), shape.faces()
    assert shape.has_vertex_normals()
    assert vertices.ndim == 1
    assert vertices.shape == (24, )
    assert faces.ndim == 1
    assert faces.shape == (12, )
    assert np.all(np.array(faces[2].tolist()) == [4, 5, 6])
    assert np.allclose(vertices[0].tolist(), [130, 165, 65, 0, 1, 0], atol=1e-3)
    assert np.allclose(vertices[4].tolist(), [290, 0, 114, 0.9534, 0, 0.301709], atol=1e-3)
