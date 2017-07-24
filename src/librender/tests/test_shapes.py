from mitsuba.core import Thread, FileResolver, Struct, float_dtype
from mitsuba.core.xml import load_string
from mitsuba.render import Mesh
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
  bbox = BoundingBox3f[min = [0, 0, 0], max = [0, 1, 1]],
  vertex_struct = Struct[
    float32 x; // @0
    float32 y; // @4
    float32 z; // @8
  ],
  vertex_count = 3,
  vertices = [36 B of vertex data],
  face_struct = Struct[
    uint32 i0; // @0
    uint32 i1; // @4
    uint32 i2; // @8
  ],
  face_count = 2,
  faces = [24 B of vertex data]
]"""
    else:
        assert str(m) == """Mesh[
  name = "MyMesh",
  bbox = BoundingBox3f[min = [0, 0, 0], max = [0, 1, 1]],
  vertex_struct = Struct[
    float64 x; // @0
    float64 y; // @8
    float64 z; // @16
  ],
  vertex_count = 3,
  vertices = [72 B of vertex data],
  face_struct = Struct[
    uint32 i0; // @0
    uint32 i1; // @4
    uint32 i2; // @8
  ],
  face_count = 2,
  faces = [24 B of vertex data]
]"""

def test02_ply_triangle():
    thread = Thread.thread()
    fres_old = thread.fileResolver()
    fres = FileResolver(fres_old)
    fres.append(os.path.dirname(os.path.realpath(__file__)))
    thread.set_file_resolver(fres)

    shape = load_string("""
        <scene version="0.5.0">
            <shape type="ply">
                <string name="filename" value="data/triangle.ply"/>
            </shape>
        </scene>
    """).kdtree()[0]

    vertices, faces = shape.vertices(), shape.faces()
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
    thread.set_file_resolver(fres_old)
