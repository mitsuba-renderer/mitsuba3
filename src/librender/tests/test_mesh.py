import mitsuba
import pytest
import enoki as ek
from enoki.dynamic import Float32 as Float

from mitsuba.python.test.util import fresolver_append_path
from mitsuba.python.util import traverse


def test01_create_mesh(variant_scalar_rgb):
    from mitsuba.core import Struct, float_dtype
    from mitsuba.render import Mesh

    m = Mesh("MyMesh", 3, 2)
    m.vertex_positions_buffer()[:] = [0.0, 0.0, 0.0, 1.0, 0.2, 0.0, 0.2, 1.0, 0.0]
    m.faces_buffer()[:] = [0, 1, 2, 1, 2, 0]
    m.parameters_changed()

    assert str(m) == """Mesh[
  name = "MyMesh",
  bbox = BoundingBox3f[
    min = [0, 0, 0],
    max = [1, 1, 0]
  ],
  vertex_count = 3,
  vertices = [36 B of vertex data],
  face_count = 2,
  faces = [24 B of face data],
  disable_vertex_normals = 0,
  surface_area = 0.96
]"""


@fresolver_append_path
def test02_ply_triangle(variant_scalar_rgb):
    from mitsuba.core import UInt32, Vector3f
    from mitsuba.core.xml import load_string

    m = load_string("""
        <shape type="ply" version="0.5.0">
            <string name="filename" value="data/triangle.ply"/>
            <boolean name="face_normals" value="true"/>
        </shape>
    """)

    positions = m.vertex_positions_buffer()
    faces = m.faces_buffer()

    assert not m.has_vertex_normals()
    assert ek.slices(positions) == 9
    assert ek.allclose(positions[0:3], [0, 0, 0])
    assert ek.allclose(positions[3:6], [0, 0, 1])
    assert ek.allclose(positions[6:9], [0, 1, 0])
    assert ek.slices(faces) == 3
    assert faces[0] == UInt32(0)
    assert faces[1] == UInt32(1)
    assert faces[2] == UInt32(2)


@fresolver_append_path
def test03_ply_computed_normals(variant_scalar_rgb):
    from mitsuba.core import Vector3f
    from mitsuba.core.xml import load_string

    """Checks(automatic) vertex normal computation for a PLY file that
    doesn't have them."""
    shape = load_string("""
        <shape type="ply" version="0.5.0">
            <string name="filename" value="data/triangle.ply"/>
        </shape>
    """)
    normals = shape.vertex_normals_buffer()
    assert shape.has_vertex_normals()
    # Normals are stored in half precision
    assert ek.allclose(normals[0:3], [-1, 0, 0])
    assert ek.allclose(normals[3:6], [-1, 0, 0])
    assert ek.allclose(normals[6:9], [-1, 0, 0])


def test04_normal_weighting_scheme(variant_scalar_rgb):
    from mitsuba.core import Struct, float_dtype, Vector3f
    from mitsuba.render import Mesh
    import numpy as np

    """Tests the weighting scheme that is used to compute surface normals."""
    m = Mesh("MyMesh", 5, 2, has_vertex_normals=True)

    vertices = m.vertex_positions_buffer()
    normals = m.vertex_normals_buffer()

    a, b = 1.0, 0.5
    vertices[:] = [0, 0, 0, -a, 1, 0, a, 1, 0, -b, 0, 1, b, 0, 1]

    n0 = Vector3f(0.0, 0.0, -1.0)
    n1 = Vector3f(0.0, 1.0, 0.0)
    angle_0 = ek.pi / 2.0
    angle_1 = ek.acos(3.0 / 5.0)
    n2 = n0 * angle_0 + n1 * angle_1
    n2 /= ek.norm(n2)
    n = np.vstack([n2, n0, n0, n1, n1]).transpose()

    m.faces_buffer()[:] = [0, 1, 2, 0, 3, 4]

    m.recompute_vertex_normals()
    for i in range(5):
        assert ek.allclose(normals[i*3:(i+1)*3], n[:, i], 5e-4)


@fresolver_append_path
def test05_load_simple_mesh(variant_scalar_rgb):
    from mitsuba.core.xml import load_string

    """Tests the OBJ and PLY loaders on a simple example."""
    for mesh_format in ["obj", "ply"]:
        shape = load_string("""
            <shape type="{0}" version="2.0.0">
                <string name="filename" value="resources/data/tests/{0}/cbox_smallbox.{0}"/>
            </shape>
        """.format(mesh_format))

        positions = shape.vertex_positions_buffer()
        faces = shape.faces_buffer()

        assert shape.has_vertex_normals()
        assert ek.slices(positions) == 72
        assert ek.slices(faces) == 36
        assert ek.allclose(faces[6:9], [4, 5, 6])
        assert ek.allclose(positions[:5], [130, 165, 65, 82, 165])


@pytest.mark.parametrize('mesh_format', ['obj', 'ply', 'serialized'])
@pytest.mark.parametrize('features', ['normals', 'uv', 'normals_uv'])
@pytest.mark.parametrize('face_normals', [True, False])
def test06_load_various_features(variant_scalar_rgb, mesh_format, features, face_normals):
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

        positions = shape.vertex_positions_buffer()
        normals = shape.vertex_normals_buffer()
        texcoords = shape.vertex_texcoords_buffer()
        faces = shape.faces_buffer()

        (v0, v2, v3) = [positions[i*3:(i+1)*3] for i in [0, 2, 3]]

        assert ek.allclose(v0, [-2.85, 0.0, -7.600000], atol=1e-3)
        assert ek.allclose(v2, [ 2.85, 0.0,  0.599999], atol=1e-3)
        assert ek.allclose(v3, [ 2.85, 0.0, -7.600000], atol=1e-3)

        if 'uv' in features:
            assert shape.has_vertex_texcoords()
            (uv0, uv2, uv3) = [texcoords[i*2:(i+1)*2] for i in [0, 2, 3]]
            # For OBJs (and .serialized generated from OBJ), UV.y is flipped.
            if mesh_format in ['obj', 'serialized']:
                assert ek.allclose(uv0, [0.950589, 1-0.988416], atol=1e-3)
                assert ek.allclose(uv2, [0.025105, 1-0.689127], atol=1e-3)
                assert ek.allclose(uv3, [0.950589, 1-0.689127], atol=1e-3)
            else:
                assert ek.allclose(uv0, [0.950589, 0.988416], atol=1e-3)
                assert ek.allclose(uv2, [0.025105, 0.689127], atol=1e-3)
                assert ek.allclose(uv3, [0.950589, 0.689127], atol=1e-3)

        if shape.has_vertex_normals():
            for n in [normals[i*3:(i+1)*3] for i in [0, 2, 3]]:
                assert ek.allclose(n, [0.0, 1.0, 0.0])

    return fresolver_append_path(test)()


@fresolver_append_path
def test07_ply_stored_attribute(variant_scalar_rgb):
    from mitsuba.core import Vector3f
    from mitsuba.core.xml import load_string

    m = load_string("""
        <shape type="ply" version="0.5.0">
            <string name="filename" value="data/triangle_face_colors.ply"/>
        </shape>
    """)

    assert str(m) == """PLYMesh[
  name = "triangle_face_colors.ply",
  bbox = BoundingBox3f[
    min = [0, 0, 0],
    max = [0, 1, 1]
  ],
  vertex_count = 3,
  vertices = [72 B of vertex data],
  face_count = 1,
  faces = [24 B of face data],
  disable_vertex_normals = 0,
  surface_area = 0,
  mesh attributes = [
    face_color: 3 floats
  ]
]"""


def test08_mesh_add_attribute(variant_scalar_rgb):
    from mitsuba.core import Struct, float_dtype
    from mitsuba.render import Mesh

    m = Mesh("MyMesh", 3, 2)
    m.vertex_positions_buffer()[:] = [0.0, 0.0, 0.0, 1.0, 0.2, 0.0, 0.2, 1.0, 0.0]
    m.faces_buffer()[:] = [0, 1, 2, 1, 2, 0]
    m.parameters_changed()

    m.add_attribute("vertex_color", 3)[:] = [0.0, 1.0, 1.0, 0.0, 0.0, 0.0, 1.0, 1.0, 0.0]

    assert str(m) == """Mesh[
  name = "MyMesh",
  bbox = BoundingBox3f[
    min = [0, 0, 0],
    max = [1, 1, 0]
  ],
  vertex_count = 3,
  vertices = [72 B of vertex data],
  face_count = 2,
  faces = [24 B of face data],
  disable_vertex_normals = 0,
  surface_area = 0.96,
  mesh attributes = [
    vertex_color: 3 floats
  ]
]"""