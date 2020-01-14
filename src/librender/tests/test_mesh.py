import mitsuba
import pytest
import enoki as ek
from enoki.dynamic import Float32 as Float

from mitsuba.python.test import variant_scalar
from mitsuba.python.test.util import fresolver_append_path


def test01_create_mesh(variant_scalar):
    from mitsuba.core import Struct, float_dtype
    from mitsuba.render import Mesh

    vertex_struct = Struct() \
        .append("x", Struct.Type.Float32) \
        .append("y", Struct.Type.Float32) \
        .append("z", Struct.Type.Float32)

    index_struct = Struct() \
        .append("i0", Struct.Type.UInt32) \
        .append("i1", Struct.Type.UInt32) \
        .append("i2", Struct.Type.UInt32)
    m = Mesh("MyMesh", vertex_struct, 3, index_struct, 2)
    v = m.vertices()
    v[0] = (0.0, 0.0, 0.0)
    v[1] = (0.0, 0.0, 1.0)
    v[2] = (0.0, 1.0, 0.0)
    m.recompute_bbox()

    if float_dtype == 'f':
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
  face_struct = Struct<12>[
    uint32 i0; // @0
    uint32 i1; // @4
    uint32 i2; // @8
  ],
  face_count = 2,
  faces = [24 B of face data],
  disable_vertex_normals = 0,
  surface_area = 0
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
  face_struct = Struct<12>[
    uint32 i0; // @0
    uint32 i1; // @4
    uint32 i2; // @8
  ],
  face_count = 2,
  faces = [24 B of face data],
  disable_vertex_normals = 0,
  surface_area = 0
]"""


@fresolver_append_path
def test02_ply_triangle(variant_scalar):
    from mitsuba.core import UInt32, Vector3f
    from mitsuba.core.xml import load_string

    shape = load_string("""
        <shape type="ply" version="0.5.0">
            <string name="filename" value="data/triangle.ply"/>
            <boolean name="face_normals" value="true"/>
        </shape>
    """)

    vertices, faces = shape.vertices(), shape.faces()
    assert not shape.has_vertex_normals()
    assert vertices.ndim == 1
    assert vertices.shape == (3, )
    assert ek.allclose(vertices['x'], [0, 0, 0])
    assert ek.allclose(vertices['y'], [0, 0, 1])
    assert ek.allclose(vertices['z'], [0, 1, 0])
    assert faces.ndim == 1
    assert faces.shape == (1, )
    assert faces[0]['i0'] == UInt32(0)
    assert faces[0]['i1'] == UInt32(1)
    assert faces[0]['i2'] == UInt32(2)

@fresolver_append_path
def test03_ply_computed_normals(variant_scalar):
    from mitsuba.core import Vector3f
    from mitsuba.core.xml import load_string

    """Checks(automatic) vertex normal computation for a PLY file that
    doesn't have them."""
    shape = load_string("""
        <shape type="ply" version="0.5.0">
            <string name="filename" value="data/triangle.ply"/>
        </shape>
    """)
    vertices = shape.vertices()
    assert shape.has_vertex_normals()
    # Normals are stored in half precision
    assert ek.allclose(vertices['nx'], [-1, -1, -1])
    assert ek.allclose(vertices['ny'], [0, 0, 0])
    assert ek.allclose(vertices['nz'], [0, 0, 0])


def test04_normal_weighting_scheme(variant_scalar):
    from mitsuba.core import Struct, float_dtype, Vector3f
    from mitsuba.render import Mesh
    import numpy as np

    """Tests the weighting scheme that is used to compute surface normals."""
    vertex_struct = Struct() \
        .append("x", Struct.Type.Float32) \
        .append("y", Struct.Type.Float32) \
        .append("z", Struct.Type.Float32) \
        .append("nx", Struct.Type.Float32) \
        .append("ny", Struct.Type.Float32) \
        .append("nz", Struct.Type.Float32)

    index_struct = Struct() \
        .append("i0", Struct.Type.UInt32) \
        .append("i1", Struct.Type.UInt32) \
        .append("i2", Struct.Type.UInt32)

    m = Mesh("MyMesh", vertex_struct, 5, index_struct, 2)
    v = m.vertices()

    a, b = 1.0, 0.5
    v['x'] = Float([0, -a, a, -b, b])
    v['y'] = Float([0,  1, 1,  0, 0])
    v['z'] = Float([0,  0, 0,  1, 1])

    n0 = Vector3f(0.0, 0.0, -1.0)
    n1 = Vector3f(0.0, 1.0, 0.0)
    angle_0 = ek.pi / 2.0
    angle_1 = ek.acos(3.0 / 5.0)
    n2 = n0 * angle_0 + n1 * angle_1
    n2 /= ek.norm(n2)
    n = np.vstack([n2, n0, n0, n1, n1])

    f = m.faces()
    f[0] = (0, 1, 2)
    f[1] = (0, 3, 4)
    m.recompute_vertex_normals()
    assert ek.allclose(v['nx'], n[:, 0], 5e-4)
    assert ek.allclose(v['ny'], n[:, 1], 5e-4)
    assert ek.allclose(v['nz'], n[:, 2], 5e-4)


@fresolver_append_path
def test05_load_simple_mesh(variant_scalar):
    from mitsuba.core.xml import load_string

    """Tests the OBJ and PLY loaders on a simple example."""
    for mesh_format in ["obj", "ply"]:
        shape = load_string("""
            <shape type="{0}" version="2.0.0">
                <string name="filename" value="resources/data/tests/{0}/cbox_smallbox.{0}"/>
            </shape>
        """.format(mesh_format))

        vertices, faces = shape.vertices(), shape.faces()
        assert shape.has_vertex_normals()
        assert vertices.ndim == 1
        assert vertices.shape == (24,)
        assert faces.ndim == 1
        assert faces.shape == (12,)
        assert ek.allclose(faces[2], [4, 5, 6])
        assert ek.allclose(vertices[0].tolist(), [130, 165, 65, 0, 1, 0], atol=1e-3)
        assert ek.allclose(vertices[4].tolist(), [290, 0, 114, 0.9534, 0, 0.301709], atol=1e-3)


@pytest.mark.parametrize('mesh_format', ['obj', 'ply', 'serialized'])
@pytest.mark.parametrize('features', ['normals', 'uv', 'normals_uv'])
@pytest.mark.parametrize('face_normals', [True, False])
def test06_load_various_features(variant_scalar, mesh_format, features, face_normals):
    """Tests the OBJ & PLY loaders with combinations of vertex / face normals,
    presence and absence of UVs, etc.
    """
    from mitsuba.core.xml import load_string

    def test():
        shape = load_string("""
            <shape type="{0}" version="2.0.0">
                <string name="filename" value="resources/data/tests/{0}/rectangle_{1}.{0}" />
                <boolean name="face_normals" value="{2}" />
            </shape>
        """.format(mesh_format, features, str(face_normals).lower()))
        assert shape.has_vertex_normals() == (not face_normals)

        vertices = shape.vertices()
        (v0, v2, v3) = [vertices[i].tolist() for i in [0, 2, 3]]

        assert ek.allclose(v0[:3], [-2.85, 0.0, -7.600000], atol=1e-3)
        assert ek.allclose(v2[:3], [ 2.85, 0.0,  0.599999], atol=1e-3)
        assert ek.allclose(v3[:3], [ 2.85, 0.0, -7.600000], atol=1e-3)
        if 'uv' in features:
            assert shape.has_vertex_texcoords()
            # For OBJs (and .serialized generated from OBJ), UV.y is flipped.
            if mesh_format in ['obj', 'serialized']:
                assert ek.allclose(v0[-2:], [0.950589, 1-0.988416], atol=1e-3)
                assert ek.allclose(v2[-2:], [0.025105, 1-0.689127], atol=1e-3)
                assert ek.allclose(v3[-2:], [0.950589, 1-0.689127], atol=1e-3)
            else:
                assert ek.allclose(v0[-2:], [0.950589, 0.988416], atol=1e-3)
                assert ek.allclose(v2[-2:], [0.025105, 0.689127], atol=1e-3)
                assert ek.allclose(v3[-2:], [0.950589, 0.689127], atol=1e-3)
        if shape.has_vertex_normals():
            for v in [v0, v2, v3]:
                assert ek.allclose(v[3:6], [0.0, 1.0, 0.0])

    return fresolver_append_path(test)()
