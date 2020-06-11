import mitsuba
import pytest
import enoki as ek
from enoki.dynamic import Float32 as Float

from mitsuba.python.test.util import fresolver_append_path
from mitsuba.python.util import traverse


def test01_create_mesh(variant_scalar_rgb):
    from mitsuba.render import Mesh

    m = Mesh("MyMesh", 3, 2)
    m.vertex_positions_buffer()[:] = [0.0, 0.0, 0.0, 1.0, 0.2, 0.0, 0.2, 1.0, 0.0]
    m.faces_buffer()[:] = [0, 1, 2, 1, 2, 0]
    m.parameters_changed()
    m.surface_area()  # Ensure surface area computed

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
  surface_area = 0.96,
  disable_vertex_normals = 0
]"""


@fresolver_append_path
def test02_ply_triangle(variant_scalar_rgb):
    from mitsuba.core import UInt32
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
    from mitsuba.core import Vector3f
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

    m.add_attribute("vertex_color", 3, [0.0, 1.0, 1.0, 0.0, 0.0, 0.0, 1.0, 1.0, 0.0])

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
  mesh attributes = [
    vertex_color: 3 floats
  ]
]"""

@fresolver_append_path
def test09_eval_parameterization(variant_scalar_rgb, variant_packet_rgb):
    from mitsuba.core.xml import load_string
    shape = load_string('''
        <shape type="obj" version="2.0.0">
            <string name="filename" value="resources/data/common/meshes/rectangle.obj"/>
        </shape>
    ''')

    si = shape.eval_parameterization([-0.01, 0.5])
    assert not ek.any(si.is_valid())
    si = shape.eval_parameterization([1.0 - 1e-7, 1.0 - 1e-7])
    assert ek.all(si.is_valid())
    assert ek.allclose(si.p, [1, 1, 0])
    si = shape.eval_parameterization([1e-7, 1e-7])
    assert ek.all(si.is_valid())
    assert ek.allclose(si.p, [-1, -1, 0])
    si = shape.eval_parameterization([.2, .3])
    assert ek.all(si.is_valid())
    assert ek.allclose(si.p, [-.6, -.4, 0])


@fresolver_append_path
def test10_ray_intersect_preliminary(variants_all_rgb):

    if 'packet' in mitsuba.variant():
        pytest.skip("pi.compute_surface_interaction isn't bound for packet modes")

    from mitsuba.core import xml, Ray3f, Vector3f, UInt32
    from mitsuba.render import HitComputeFlags

    scene = xml.load_string('''
        <scene version="2.0.0">
            <shape type="obj">
                <string name="filename" value="resources/data/common/meshes/rectangle.obj"/>
            </shape>
        </scene>
    ''')

    ray = Ray3f(Vector3f(-0.3, -0.3, -10.0), Vector3f(0.0, 0.0, 1.0), 0, [])
    pi = scene.ray_intersect_preliminary(ray)

    assert ek.allclose(pi.t, 10)
    assert pi.prim_index == 0
    assert ek.allclose(pi.prim_uv, [0.35, 0.3])

    si = pi.compute_surface_interaction(ray)
    assert ek.allclose(si.t, 10)
    assert ek.allclose(si.p, [-0.3, -0.3, 0.0])
    assert ek.allclose(si.uv, [0.35, 0.35])
    assert ek.allclose(si.dp_du, [2.0, 0.0, 0.0])
    assert ek.allclose(si.dp_dv, [0.0, 2.0, 0.0])

    ray = Ray3f(Vector3f(0.3, 0.3, -10.0), Vector3f(0.0, 0.0, 1.0), 0, [])
    pi = scene.ray_intersect_preliminary(ray)
    assert ek.allclose(pi.t, 10)
    assert pi.prim_index == 1
    assert ek.allclose(pi.prim_uv, [0.3, 0.35])

    si = pi.compute_surface_interaction(ray)
    assert ek.allclose(si.t, 10)
    assert ek.allclose(si.p, [0.3, 0.3, 0.0])
    assert ek.allclose(si.uv, [0.65, 0.65])
    assert ek.allclose(si.dp_du, [2.0, 0.0, 0.0])
    assert ek.allclose(si.dp_dv, [0.0, 2.0, 0.0])


@fresolver_append_path
def test11_parameters_grad_enabled(variant_gpu_autodiff_rgb):
    from mitsuba.core.xml import load_string

    shape = load_string('''
        <shape type="obj" version="2.0.0">
            <string name="filename" value="resources/data/common/meshes/rectangle.obj"/>
        </shape>
    ''')

    assert shape.parameters_grad_enabled() == False

    # Get the shape's parameters
    params = traverse(shape)

    # Only parameters of the shape should affect the result of that method
    bsdf_param_key = 'bsdf.reflectance.value'
    ek.set_requires_gradient(params[bsdf_param_key])
    params.set_dirty(bsdf_param_key)
    params.update()
    assert shape.parameters_grad_enabled() == False

    # When setting one of the shape's param to require gradient, method should return True
    shape_param_key = 'vertex_positions_buf'
    ek.set_requires_gradient(params[shape_param_key])
    params.set_dirty(shape_param_key)
    params.update()
    assert shape.parameters_grad_enabled() == True


@fresolver_append_path
def test12_differentiable_surface_interaction_automatic(variant_gpu_autodiff_rgb):
    from mitsuba.core import xml, Ray3f, Vector3f, UInt32
    from mitsuba.render import HitComputeFlags

    scene = xml.load_string('''
        <scene version="2.0.0">
            <shape type="obj" id="rect">
                <string name="filename" value="resources/data/common/meshes/rectangle.obj"/>
            </shape>
        </scene>
    ''')

    ray = Ray3f(Vector3f(-0.3, -0.3, -10.0), Vector3f(0.0, 0.0, 1.0), 0, [])
    pi = scene.ray_intersect_preliminary(ray)

    # si should not be attached if not necessary
    si = pi.compute_surface_interaction(ray)
    assert not ek.requires_gradient(si.t)
    assert not ek.requires_gradient(si.p)

    # si should be attached if ray is attached
    ek.set_requires_gradient(ray.o)
    si = pi.compute_surface_interaction(ray)
    assert ek.requires_gradient(si.t)
    assert ek.requires_gradient(si.p)

    # si should not be attached if falgs says so
    ek.set_requires_gradient(ray.o)
    si = pi.compute_surface_interaction(ray, HitComputeFlags.NonDifferentiable)
    assert not ek.requires_gradient(si.t)
    assert not ek.requires_gradient(si.p)

    # si should be attached if shape parameters are attached
    params = traverse(scene)
    shape_param_key = 'rect.vertex_positions_buf'
    ek.set_requires_gradient(params[shape_param_key])
    params.set_dirty(shape_param_key)
    params.update()

    ek.set_requires_gradient(ray.o, False)
    si = pi.compute_surface_interaction(ray)
    assert ek.requires_gradient(si.t)
    assert ek.requires_gradient(si.p)


@fresolver_append_path
def test13_differentiable_surface_interaction_ray_forward(variant_gpu_autodiff_rgb):
    from mitsuba.core import xml, Ray3f, Vector3f, UInt32

    scene = xml.load_string('''
        <scene version="2.0.0">
            <shape type="obj" id="rect">
                <string name="filename" value="resources/data/common/meshes/rectangle.obj"/>
            </shape>
        </scene>
    ''')

    ray = Ray3f(Vector3f(-0.3, -0.4, -10.0), Vector3f(0.0, 0.0, 1.0), 0, [])
    pi = scene.ray_intersect_preliminary(ray)

    ek.set_requires_gradient(ray.o)
    ek.set_requires_gradient(ray.d)

    # If the ray origin is shifted along the x-axis, so does si.p
    si = pi.compute_surface_interaction(ray)
    ek.forward(ray.o.x)
    assert ek.allclose(ek.gradient(si.p), [1, 0, 0])

    # If the ray origin is shifted along the x-axis, so does si.uv
    si = pi.compute_surface_interaction(ray)
    ek.forward(ray.o.x)
    assert ek.allclose(ek.gradient(si.uv), [0.5, 0])

    # If the ray origin is shifted along the z-axis, so does si.t
    si = pi.compute_surface_interaction(ray)
    ek.forward(ray.o.z)
    assert ek.allclose(ek.gradient(si.t), -1)

    # If the ray direction is shifted along the x-axis, so does si.p
    si = pi.compute_surface_interaction(ray)
    ek.forward(ray.d.x)
    assert ek.allclose(ek.gradient(si.p), [10, 0, 0])


@fresolver_append_path
def test14_differentiable_surface_interaction_ray_backward(variant_gpu_autodiff_rgb):
    from mitsuba.core import xml, Ray3f, Vector3f, UInt32

    scene = xml.load_string('''
        <scene version="2.0.0">
            <shape type="obj" id="rect">
                <string name="filename" value="resources/data/common/meshes/rectangle.obj"/>
            </shape>
        </scene>
    ''')

    ray = Ray3f(Vector3f(-0.3, -0.4, -10.0), Vector3f(0.0, 0.0, 1.0), 0, [])
    pi = scene.ray_intersect_preliminary(ray)

    ek.set_requires_gradient(ray.o)

    # If si.p is shifted along the x-axis, so does the ray origin
    si = pi.compute_surface_interaction(ray)
    ek.backward(si.p.x)
    assert ek.allclose(ek.gradient(ray.o), [1, 0, 0])

    # If si.t is changed, so does the ray origin along the z-axis
    si = pi.compute_surface_interaction(ray)
    ek.backward(si.t)
    assert ek.allclose(ek.gradient(ray.o), [0, 0, -1])


@fresolver_append_path
def test15_differentiable_surface_interaction_params_forward(variant_gpu_autodiff_rgb):
    from mitsuba.core import xml, Float, Ray3f, Vector3f, UInt32, Transform4f

    # Convert flat array into a vector of arrays (will be included in next enoki release)
    def ravel(buf, dim = 3):
        idx = dim * UInt32.arange(ek.slices(buf) // dim)
        if dim == 2:
            return Vector2f(ek.gather(buf, idx), ek.gather(buf, idx + 1))
        elif dim == 3:
            return Vector3f(ek.gather(buf, idx), ek.gather(buf, idx + 1), ek.gather(buf, idx + 2))

    # Return contiguous flattened array (will be included in next enoki release)
    def unravel(source, target, dim = 3):
        idx = UInt32.arange(ek.slices(source))
        for i in range(dim):
            ek.scatter(target, source[i], dim * idx + i)

    scene = xml.load_string('''
        <scene version="2.0.0">
            <shape type="obj" id="rect">
                <string name="filename" value="resources/data/common/meshes/rectangle.obj"/>
            </shape>
        </scene>
    ''')

    params = traverse(scene)
    shape_param_key = 'rect.vertex_positions_buf'
    positions_buf = params[shape_param_key]
    positions_initial = ravel(positions_buf)

    # Create differential parameter to be optimized
    diff_vector = Vector3f(0.0)
    ek.set_requires_gradient(diff_vector)

    # Apply the transformation to mesh vertex position and update scene
    def apply_transformation(trasfo):
        trasfo = trasfo(diff_vector)
        new_positions = trasfo.transform_point(positions_initial)
        unravel(new_positions, params[shape_param_key])
        params.set_dirty(shape_param_key)
        params.update()

    # ---------------------------------------
    # Test translation

    ray = Ray3f(Vector3f(-0.2, -0.3, -10.0), Vector3f(0.0, 0.0, 1.0), 0, [])
    pi = scene.ray_intersect_preliminary(ray)

    # # If the vertices are shifted along z-axis, so does si.t
    apply_transformation(lambda v : Transform4f.translate(v))
    si = pi.compute_surface_interaction(ray)
    ek.forward(diff_vector.z)
    assert ek.allclose(ek.gradient(si.t), 1)

    # If the vertices are shifted along z-axis, so does si.p
    apply_transformation(lambda v : Transform4f.translate(v))
    si = pi.compute_surface_interaction(ray)
    ek.forward(diff_vector.z)
    assert ek.allclose(ek.gradient(si.p), [0.0, 0.0, 1.0])

    # If the vertices are shifted along x-axis, so does si.uv (times 0.5)
    apply_transformation(lambda v : Transform4f.translate(v))
    si = pi.compute_surface_interaction(ray)
    ek.forward(diff_vector.x)
    assert ek.allclose(ek.gradient(si.uv), [-0.5, 0.0])

    # If the vertices are shifted along y-axis, so does si.uv (times 0.5)
    apply_transformation(lambda v : Transform4f.translate(v))
    si = pi.compute_surface_interaction(ray)
    ek.forward(diff_vector.y)
    assert ek.allclose(ek.gradient(si.uv), [0.0, -0.5])

    # ---------------------------------------
    # Test rotation

    ray = Ray3f(Vector3f(-0.99999, -0.99999, -10.0), Vector3f(0.0, 0.0, 1.0), 0, [])
    pi = scene.ray_intersect_preliminary(ray)

    # If the vertices are rotated around the center, so does si.uv (times 0.5)
    apply_transformation(lambda v : Transform4f.rotate([0, 0, 1], v.x))
    si = pi.compute_surface_interaction(ray)
    ek.forward(diff_vector.x)
    du = 0.5 * ek.sin(2 * ek.pi / 360.0)
    assert ek.allclose(ek.gradient(si.uv), [-du, du], atol=1e-6)


@fresolver_append_path
def test16_differentiable_surface_interaction_params_backward(variant_gpu_autodiff_rgb):
    from mitsuba.core import xml, Float, Ray3f, Vector3f, UInt32, Transform4f

    scene = xml.load_string('''
        <scene version="2.0.0">
            <shape type="obj" id="rect">
                <string name="filename" value="resources/data/common/meshes/rectangle.obj"/>
            </shape>
        </scene>
    ''')

    params = traverse(scene)
    vertex_pos_key = 'rect.vertex_positions_buf'
    vertex_normals_key = 'rect.vertex_normals_buf'
    vertex_texcoords_key = 'rect.vertex_texcoords_buf'
    ek.set_requires_gradient(params[vertex_pos_key])
    ek.set_requires_gradient(params[vertex_normals_key])
    ek.set_requires_gradient(params[vertex_texcoords_key])
    params.set_dirty(vertex_pos_key)
    params.set_dirty(vertex_normals_key)
    params.set_dirty(vertex_texcoords_key)
    params.update()

    # Hit the upper right corner of the rectancle (the 4th vertex)
    ray = Ray3f(Vector3f(0.99999, 0.99999, -10.0), Vector3f(0.0, 0.0, 1.0), 0, [])
    pi = scene.ray_intersect_preliminary(ray)

    # ---------------------------------------
    # Test vertex posistions

    # If si.t changes, so the 4th vertex should move along the z-axis
    si = pi.compute_surface_interaction(ray)
    ek.backward(si.t)
    assert ek.allclose(ek.gradient(params[vertex_pos_key]),
                       [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1], atol=1e-5)

    # If si.p moves along the z-axis, so does the 4th vertex
    si = pi.compute_surface_interaction(ray)
    ek.backward(si.p.z)
    assert ek.allclose(ek.gradient(params[vertex_pos_key]),
                       [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1], atol=1e-5)

    # To increase si.dp_du along the x-axis, we need to strech the upper edge of the rectangle
    si = pi.compute_surface_interaction(ray)
    ek.backward(si.dp_du.x)
    assert ek.allclose(ek.gradient(params[vertex_pos_key]),
                       [0, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0], atol=1e-5)

    # To increase si.dp_du along the y-axis, we need to transform the rectangle into a trapezoid
    si = pi.compute_surface_interaction(ray)
    ek.backward(si.dp_du.y)
    assert ek.allclose(ek.gradient(params[vertex_pos_key]),
                       [0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0], atol=1e-5)

    # To increase si.dp_dv along the x-axis, we need to transform the rectangle into a trapezoid
    si = pi.compute_surface_interaction(ray)
    ek.backward(si.dp_dv.x)
    assert ek.allclose(ek.gradient(params[vertex_pos_key]),
                       [-1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0], atol=1e-5)

    # To increase si.dp_dv along the y-axis, we need to strech the right edge of the rectangle
    si = pi.compute_surface_interaction(ray)
    ek.backward(si.dp_dv.y)
    assert ek.allclose(ek.gradient(params[vertex_pos_key]),
                       [0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0], atol=1e-5)

    # To increase si.n along the x-axis, we need to rotate the right edge around the y axis
    si = pi.compute_surface_interaction(ray)
    ek.backward(si.n.x)
    assert ek.allclose(ek.gradient(params[vertex_pos_key]),
                       [0, 0, 0, 0, 0, 0.5, 0, 0, 0, 0, 0, -0.5], atol=1e-5)

    # To increase si.n along the y-axis, we need to rotate the top edge around the x axis
    si = pi.compute_surface_interaction(ray)
    ek.backward(si.n.y)
    assert ek.allclose(ek.gradient(params[vertex_pos_key]),
                       [0, 0, 0.5, 0, 0, 0, 0, 0, 0, 0, 0, -0.5], atol=1e-5)

    # To increase si.sh_frame.n along the x-axis, we need to rotate the right edge around the y axis
    params.set_dirty(vertex_pos_key); params.update()
    si = pi.compute_surface_interaction(ray)
    ek.backward(si.sh_frame.n.x)
    assert ek.allclose(ek.gradient(params[vertex_pos_key]),
                       [0, 0, 0, 0, 0, 0.5, 0, 0, 0, 0, 0, -0.5], atol=1e-5)

    # To increase si.sh_frame.n along the y-axis, we need to rotate the top edge around the x axis
    params.set_dirty(vertex_pos_key); params.update()
    si = pi.compute_surface_interaction(ray)
    ek.backward(si.sh_frame.n.y)
    assert ek.allclose(ek.gradient(params[vertex_pos_key]),
                       [0, 0, 0.5, 0, 0, 0, 0, 0, 0, 0, 0, -0.5], atol=1e-5)

    # ---------------------------------------
    # Test vertex texcoords

    # To increase si.uv along the x-axis, we need to move the uv of the 4th vertex along the x-axis
    si = pi.compute_surface_interaction(ray)
    ek.backward(si.uv.x)
    assert ek.allclose(ek.gradient(params[vertex_texcoords_key]),
                       [0, 0, 0, 0, 0, 0, 1, 0], atol=1e-5)

    # To increase si.uv along the y-axis, we need to move the uv of the 4th vertex along the y-axis
    si = pi.compute_surface_interaction(ray)
    ek.backward(si.uv.y)
    assert ek.allclose(ek.gradient(params[vertex_texcoords_key]),
                       [0, 0, 0, 0, 0, 0, 0, 1], atol=1e-5)

    # To increase si.dp_du along the x-axis, we need to shrink the uv along the top edge of the rectangle
    si = pi.compute_surface_interaction(ray)
    ek.backward(si.dp_du.x)
    assert ek.allclose(ek.gradient(params[vertex_texcoords_key]),
                       [0, 0, 2, 0, 0, 0, -2, 0], atol=1e-5)

    # To increase si.dp_du along the y-axis, we need to shrink the uv along the right edge of the rectangle
    si = pi.compute_surface_interaction(ray)
    ek.backward(si.dp_dv.y)
    assert ek.allclose(ek.gradient(params[vertex_texcoords_key]),
                       [0, 2, 0, 0, 0, 0, 0, -2], atol=1e-5)

