import pytest
import drjit as dr
import mitsuba as mi

from mitsuba.scalar_rgb.test.util import fresolver_append_path


def test01_create_mesh(variant_scalar_rgb):
    m = mi.Mesh("MyMesh", 3, 2)

    params = mi.traverse(m)
    params['vertex_positions'] = [0.0, 0.0, 0.0, 1.0, 0.2, 0.0, 0.2, 1.0, 0.0]
    params['faces'] = [0, 1, 2, 1, 2, 0]
    params.update()

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
  face_normals = 0
]"""


@fresolver_append_path
def test02_ply_triangle(variant_scalar_rgb):
    m = mi.load_dict({
        "type" : "ply",
        "filename" : "resources/data/tests/ply/triangle.ply",
        "face_normals" : True
    })

    params = mi.traverse(m)
    positions = params['vertex_positions']
    faces = params['faces']

    assert not m.has_vertex_normals()
    assert dr.width(positions) == 9
    assert dr.allclose(positions[0:3], [0, 0, 0])
    assert dr.allclose(positions[3:6], [0, 0, 1])
    assert dr.allclose(positions[6:9], [0, 1, 0])
    assert dr.width(faces) == 3
    assert faces[0] == mi.UInt32(0)
    assert faces[1] == mi.UInt32(1)
    assert faces[2] == mi.UInt32(2)


@fresolver_append_path
def test03_ply_computed_normals(variant_scalar_rgb):
    """Checks(automatic) vertex normal computation for a PLY file that
    doesn't have them."""
    shape = mi.load_dict({
        "type" : "ply",
        "filename" : "resources/data/tests/ply/triangle.ply",
    })
    params = mi.traverse(shape)
    normals = params['vertex_normals']

    assert shape.has_vertex_normals()
    # Normals are stored in half precision
    assert dr.allclose(normals[0:3], [-1, 0, 0])
    assert dr.allclose(normals[3:6], [-1, 0, 0])
    assert dr.allclose(normals[6:9], [-1, 0, 0])


def test04_normal_weighting_scheme(variant_scalar_rgb):
    import numpy as np

    """Tests the weighting scheme that is used to compute surface normals."""
    m = mi.Mesh("MyMesh", 5, 2, has_vertex_normals=True)

    params = mi.traverse(m)

    a, b = 1.0, 0.5
    params['vertex_positions'] = [0, 0, 0, -a, 1, 0, a, 1, 0, -b, 0, 1, b, 0, 1]

    n0 = mi.Vector3f(0.0, 0.0, -1.0)
    n1 = mi.Vector3f(0.0, 1.0, 0.0)
    angle_0 = dr.pi / 2.0
    angle_1 = dr.acos(3.0 / 5.0)
    n2 = n0 * angle_0 + n1 * angle_1
    n2 /= dr.norm(n2)
    n = np.vstack([n2, n0, n0, n1, n1]).transpose()

    params['faces'] = [0, 1, 2, 0, 3, 4]
    params.update()

    for i in range(5):
        assert dr.allclose(params['vertex_normals'][i*3:(i+1)*3], n[:, i], 5e-4)


@fresolver_append_path
def test05_load_simple_mesh(variant_scalar_rgb):
    """Tests the OBJ and PLY loaders on a simple example."""
    for mesh_format in ["obj", "ply"]:
        shape = mi.load_dict({
            "type" : mesh_format,
            "filename" : f"resources/data/tests/{mesh_format}/cbox_smallbox.{mesh_format}",
        })

        params = mi.traverse(shape)
        positions = params['vertex_positions']
        faces = params['faces']

        assert shape.has_vertex_normals()
        assert dr.width(positions) == 72
        assert dr.width(faces) == 36
        assert dr.allclose(faces[6:9], [4, 5, 6])
        assert dr.allclose(positions[:5], [130, 165, 65, 82, 165])


@pytest.mark.parametrize('mesh_format', ['obj', 'ply', 'serialized'])
@pytest.mark.parametrize('features', ['normals', 'uv', 'normals_uv'])
@pytest.mark.parametrize('face_normals', [True, False])
def test06_load_various_features(variant_scalar_rgb, mesh_format, features, face_normals):
    """Tests the OBJ & PLY loaders with combinations of vertex / face normals,
    presence and absence of UVs, etc.
    """
    def test():
        shape = mi.load_dict({
            "type" : mesh_format,
            "filename" : f"resources/data/tests/{mesh_format}/rectangle_{features}.{mesh_format}",
            "face_normals" : face_normals,
        })
        assert shape.has_vertex_normals() == (not face_normals)

        params = mi.traverse(shape)
        positions = params['vertex_positions']
        normals = params['vertex_normals']
        texcoords = params['vertex_texcoords']
        faces = params['faces']

        (v0, v2, v3) = [positions[i*3:(i+1)*3] for i in [0, 2, 3]]

        assert dr.allclose(v0, [-2.85, 0.0, -7.600000], atol=1e-3)
        assert dr.allclose(v2, [ 2.85, 0.0,  0.599999], atol=1e-3)
        assert dr.allclose(v3, [ 2.85, 0.0, -7.600000], atol=1e-3)

        if 'uv' in features:
            assert shape.has_vertex_texcoords()
            (uv0, uv2, uv3) = [texcoords[i*2:(i+1)*2] for i in [0, 2, 3]]
            # For OBJs (and .serialized generated from OBJ), UV.y is flipped.
            if mesh_format in ['obj', 'serialized']:
                assert dr.allclose(uv0, [0.950589, 1-0.988416], atol=1e-3)
                assert dr.allclose(uv2, [0.025105, 1-0.689127], atol=1e-3)
                assert dr.allclose(uv3, [0.950589, 1-0.689127], atol=1e-3)
            else:
                assert dr.allclose(uv0, [0.950589, 0.988416], atol=1e-3)
                assert dr.allclose(uv2, [0.025105, 0.689127], atol=1e-3)
                assert dr.allclose(uv3, [0.950589, 0.689127], atol=1e-3)

        if shape.has_vertex_normals():
            for n in [normals[i*3:(i+1)*3] for i in [0, 2, 3]]:
                assert dr.allclose(n, [0.0, 1.0, 0.0])

    return fresolver_append_path(test)()


@fresolver_append_path
def test07_ply_stored_attribute(variant_scalar_rgb):
    m = mi.load_dict({
        "type" : "ply",
        "filename" : "resources/data/tests/ply/triangle_face_colors.ply",
    })

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
  face_normals = 0,
  mesh attributes = [
    face_color: 3 floats
  ]
]"""


def test08_mesh_add_attribute(variant_scalar_rgb):
    m = mi.Mesh("MyMesh", 3, 2)

    params = mi.traverse(m)
    params['vertex_positions'] = [0.0, 0.0, 0.0, 1.0, 0.2, 0.0, 0.2, 1.0, 0.0]
    params['faces'] = [0, 1, 2, 1, 2, 0]
    params.update()

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
  face_normals = 0,
  mesh attributes = [
    vertex_color: 3 floats
  ]
]"""


@fresolver_append_path
def test09_eval_parameterization(variants_all_rgb):
    shape = mi.load_dict({
        "type" : "obj",
        "filename" : "resources/data/common/meshes/rectangle.obj",
        "emitter": {
            "type": "area",
            "radiance": {
                "type": "checkerboard",
            }
        }
    })

    si = shape.eval_parameterization([-0.01, 0.5])
    assert not dr.any(si.is_valid())
    si = shape.eval_parameterization([1.0 - 1e-7, 1.0 - 1e-7])
    assert dr.all(si.is_valid())
    assert dr.allclose(si.p, [1, 1, 0])
    si = shape.eval_parameterization([1e-7, 1e-7])
    assert dr.all(si.is_valid())
    assert dr.allclose(si.p, [-1, -1, 0])
    si = shape.eval_parameterization([.2, .3])
    assert dr.all(si.is_valid())
    assert dr.allclose(si.p, [-.6, -.4, 0])

    # Test with symbolic virtual function call
    if not 'scalar' in mi.variant():
        emitter = shape.emitter()
        N = 4
        mask = dr.eq(dr.arange(mi.UInt32, N) & 1, 0)
        emitters = dr.select(mask, mi.EmitterPtr(emitter), dr.zeros(mi.EmitterPtr))
        it = dr.zeros(mi.Interaction3f, N)
        it.p = [0, 0, -3]
        it.t = 0
        uv = emitters.sample_direction(it, [0.5, 0.5])[0].uv
        assert dr.allclose(uv, dr.select(mask, mi.Point2f(0.5), mi.Point2f(0.0)))


@fresolver_append_path
def test10_ray_intersect_preliminary(variants_all_rgb):
    scene = mi.load_dict({
        "type" : "scene",
        "meshes": {
            "type" : "obj",
            "id" : "rect",
            "filename" : "resources/data/common/meshes/rectangle.obj",
        }
    })

    ray = mi.Ray3f(mi.Vector3f(-0.3, -0.3, -10.0), mi.Vector3f(0.0, 0.0, 1.0))
    pi = scene.ray_intersect_preliminary(ray, coherent=True)

    assert dr.allclose(pi.t, 10)
    assert pi.prim_index == 0
    assert dr.allclose(pi.prim_uv, [0.35, 0.3])

    si = pi.compute_surface_interaction(ray)
    assert dr.allclose(si.t, 10)
    assert dr.allclose(si.p, [-0.3, -0.3, 0.0])
    assert dr.allclose(si.uv, [0.35, 0.35])
    assert dr.allclose(si.dp_du, [2.0, 0.0, 0.0])
    assert dr.allclose(si.dp_dv, [0.0, 2.0, 0.0])

    ray = mi.Ray3f(mi.Vector3f(0.3, 0.3, -10.0), mi.Vector3f(0.0, 0.0, 1.0))
    pi = scene.ray_intersect_preliminary(ray, coherent=True)
    assert dr.allclose(pi.t, 10)
    assert pi.prim_index == 1
    assert dr.allclose(pi.prim_uv, [0.3, 0.35])

    si = pi.compute_surface_interaction(ray)
    assert dr.allclose(si.t, 10)
    assert dr.allclose(si.p, [0.3, 0.3, 0.0])
    assert dr.allclose(si.uv, [0.65, 0.65])
    assert dr.allclose(si.dp_du, [2.0, 0.0, 0.0])
    assert dr.allclose(si.dp_dv, [0.0, 2.0, 0.0])


@fresolver_append_path
def test11_parameters_grad_enabled(variants_all_ad_rgb):
    shape = mi.load_dict({
        "type" : "obj",
        "filename" : "resources/data/common/meshes/rectangle.obj",
    })

    assert shape.parameters_grad_enabled() == False

    # Get the shape's parameters
    params = mi.traverse(shape)

    # Only parameters of the shape should affect the result of that method
    bsdf_param_key = 'bsdf.reflectance.value'
    dr.enable_grad(params[bsdf_param_key])
    params.set_dirty(bsdf_param_key)
    params.update()
    assert shape.parameters_grad_enabled() == False

    # When setting one of the shape's param to require gradient, method should return True
    shape_param_key = 'vertex_positions'
    dr.enable_grad(params[shape_param_key])
    params.set_dirty(shape_param_key)
    params.update()
    assert shape.parameters_grad_enabled() == True

if hasattr(dr, 'JitFlag'):
    jit_flags_options = [
        {dr.JitFlag.VCallRecord : 0, dr.JitFlag.VCallOptimize : 0, dr.JitFlag.LoopRecord : 0},
        {dr.JitFlag.VCallRecord : 1, dr.JitFlag.VCallOptimize : 0, dr.JitFlag.LoopRecord : 0},
        {dr.JitFlag.VCallRecord : 1, dr.JitFlag.VCallOptimize : 1, dr.JitFlag.LoopRecord : 0},
    ]
else:
    jit_flags_options = []

@fresolver_append_path
def test12_differentiable_surface_interaction_automatic(variants_all_ad_rgb):
    dr.set_flag(dr.JitFlag.VCallRecord, False)

    scene = mi.load_dict({
        "type" : "scene",
        "meshes": {
            "type" : "obj",
            "id" : "rect",
            "filename" : "resources/data/common/meshes/rectangle.obj",
        }
    })

    ray = mi.Ray3f(mi.Vector3f(-0.3, -0.3, -10.0), mi.Vector3f(0.0, 0.0, 1.0))
    pi = scene.ray_intersect_preliminary(ray, coherent=True)

    # si should not be attached if not necessary
    si = pi.compute_surface_interaction(ray)
    assert not dr.grad_enabled(si.t)
    assert not dr.grad_enabled(si.p)

    # si should be attached if ray is attached
    dr.enable_grad(ray.o)
    si = pi.compute_surface_interaction(ray)
    assert dr.grad_enabled(si.t)
    assert dr.grad_enabled(si.p)
    assert not dr.grad_enabled(si.n) # Face normal doesn't depend on ray

    # si should be attached if ray is attached (even when we pass RayFlags.DetachShape)
    dr.enable_grad(ray.o)
    si = pi.compute_surface_interaction(ray, mi.RayFlags.DetachShape)
    assert dr.grad_enabled(si.p)
    assert dr.grad_enabled(si.uv)
    assert not dr.grad_enabled(si.n) # Face normal doesn't depend on ray

    # si should be attached if shape parameters are attached
    params = mi.traverse(scene)
    shape_param_key = 'rect.vertex_positions'
    dr.enable_grad(params[shape_param_key])
    params.set_dirty(shape_param_key)
    params.update()

    dr.disable_grad(ray.o)
    si = pi.compute_surface_interaction(ray)
    assert dr.grad_enabled(si.t)
    assert dr.grad_enabled(si.p)


@fresolver_append_path
@pytest.mark.parametrize("jit_flags", jit_flags_options)
def test13_differentiable_surface_interaction_ray_forward(variants_all_ad_rgb, jit_flags):
    # Set drjit JIT flags
    for k, v in jit_flags.items():
        dr.set_flag(k, v)

    scene = mi.load_dict({
        "type" : "scene",
        "meshes": {
            "type" : "obj",
            "id" : "rect",
            "filename" : "resources/data/common/meshes/rectangle.obj",
        }
    })

    ray = mi.Ray3f(mi.Vector3f(-0.3, -0.4, -10.0), mi.Vector3f(0.0, 0.0, 1.0))
    pi = scene.ray_intersect_preliminary(ray, coherent=True)

    dr.enable_grad(ray.o)
    dr.enable_grad(ray.d)

    # If the ray origin is shifted along the x-axis, so does si.p
    si = pi.compute_surface_interaction(ray)
    p = si.p + mi.Float(0.001) # Ensure p is a AD leaf node
    dr.forward(ray.o.x)
    assert dr.allclose(dr.grad(p), [1, 0, 0])

    # If the ray origin is shifted along the x-axis, so does si.uv
    si = pi.compute_surface_interaction(ray)
    dr.forward(ray.o.x)
    assert dr.allclose(dr.grad(si.uv), [0.5, 0])

    # If the ray origin is shifted along the z-axis, so does si.t
    si = pi.compute_surface_interaction(ray)
    dr.forward(ray.o.z)
    assert dr.allclose(dr.grad(si.t), -1)

    # If the ray direction is shifted along the x-axis, so does si.p
    si = pi.compute_surface_interaction(ray)
    p = si.p + mi.Float(0.001) # Ensure p is a AD leaf node
    dr.forward(ray.d.x)
    assert dr.allclose(dr.grad(p), [10, 0, 0])


@fresolver_append_path
@pytest.mark.parametrize("jit_flags", jit_flags_options)
def test14_differentiable_surface_interaction_ray_backward(variants_all_ad_rgb, jit_flags):
    # Set drjit JIT flags
    for k, v in jit_flags.items():
        dr.set_flag(k, v)

    scene = mi.load_dict({
        "type" : "scene",
        "meshes": {
            "type" : "obj",
            "id" : "rect",
            "filename" : "resources/data/common/meshes/rectangle.obj",
        }
    })

    ray = mi.Ray3f(mi.Vector3f(-0.3, -0.4, -10.0), mi.Vector3f(0.0, 0.0, 1.0))
    pi = scene.ray_intersect_preliminary(ray, coherent=True)

    dr.enable_grad(ray.o)

    # If si.p is shifted along the x-axis, so does the ray origin
    si = pi.compute_surface_interaction(ray)
    dr.backward(si.p.x)
    assert dr.allclose(dr.grad(ray.o), [1, 0, 0])

    # If si.t is changed, so does the ray origin along the z-axis
    dr.set_grad(ray.o, 0.0)
    si = pi.compute_surface_interaction(ray)
    dr.backward(si.t)
    assert dr.allclose(dr.grad(ray.o), [0, 0, -1])


@fresolver_append_path
@pytest.mark.parametrize("jit_flags", jit_flags_options)
def test15_differentiable_surface_interaction_params_forward(variants_all_ad_rgb, jit_flags):
    # Set drjit JIT flags
    for k, v in jit_flags.items():
        dr.set_flag(k, v)

    scene = mi.load_dict({
        "type" : "scene",
        "meshes": {
            "type" : "obj",
            "id" : "rect",
            "filename" : "resources/data/common/meshes/rectangle.obj",
        }
    })

    params = mi.traverse(scene)
    shape_param_key = 'rect.vertex_positions'
    positions_buf = params[shape_param_key]
    positions_initial = dr.unravel(mi.Point3f, positions_buf)

    # Create differential parameter to be optimized
    diff_vector = mi.Vector3f(0.0)
    dr.enable_grad(diff_vector)

    # Apply the transformation to mesh vertex position and update scene
    def apply_transformation(trasfo):
        trasfo = trasfo(diff_vector)
        new_positions = trasfo @ positions_initial
        params[shape_param_key] = dr.ravel(new_positions)
        params.set_dirty(shape_param_key)
        params.update()

    # ---------------------------------------
    # Test translation

    ray = mi.Ray3f(mi.Vector3f(-0.2, -0.3, -10.0), mi.Vector3f(0.0, 0.0, 1.0))
    pi = scene.ray_intersect_preliminary(ray, coherent=True)

    # # If the vertices are shifted along z-axis, so does si.t
    apply_transformation(lambda v : mi.Transform4f.translate(v))
    si = pi.compute_surface_interaction(ray)
    dr.forward(diff_vector.z)
    assert dr.allclose(dr.grad(si.t), 1)

    # If the vertices are shifted along z-axis, so does si.p
    apply_transformation(lambda v : mi.Transform4f.translate(v))
    si = pi.compute_surface_interaction(ray)
    p = si.p + mi.Float(0.001) # Ensure p is a AD leaf node
    dr.forward(diff_vector.z)
    assert dr.allclose(dr.grad(p), [0.0, 0.0, 1.0])

    # If the vertices are shifted along x-axis, so does si.uv (times 0.5)
    apply_transformation(lambda v : mi.Transform4f.translate(v))
    si = pi.compute_surface_interaction(ray)
    dr.forward(diff_vector.x)
    assert dr.allclose(dr.grad(si.uv), [-0.5, 0.0])

    # If the vertices are shifted along y-axis, so does si.uv (times 0.5)
    apply_transformation(lambda v : mi.Transform4f.translate(v))
    si = pi.compute_surface_interaction(ray)
    dr.forward(diff_vector.y)
    assert dr.allclose(dr.grad(si.uv), [0.0, -0.5])

    # ---------------------------------------
    # Test rotation

    ray = mi.Ray3f(mi.Vector3f(-0.99999, -0.99999, -10.0), mi.Vector3f(0.0, 0.0, 1.0))
    pi = scene.ray_intersect_preliminary(ray, coherent=True)

    # If the vertices are rotated around the center, so does si.uv (times 0.5)
    apply_transformation(lambda v : mi.Transform4f.rotate([0, 0, 1], v.x))
    si = pi.compute_surface_interaction(ray)
    dr.forward(diff_vector.x)
    du = 0.5 * dr.sin(2 * dr.pi / 360.0)
    assert dr.allclose(dr.grad(si.uv), [-du, du], atol=1e-6)


@fresolver_append_path
@pytest.mark.parametrize("jit_flags", jit_flags_options)
def test16_differentiable_surface_interaction_params_backward(variants_all_ad_rgb, jit_flags):
    # Set drjit JIT flags
    for k, v in jit_flags.items():
        dr.set_flag(k, v)

    scene = mi.load_dict({
        "type" : "scene",
        "meshes": {
            "type" : "obj",
            "id" : "rect",
            "filename" : "resources/data/common/meshes/rectangle.obj",
        }
    })

    params = mi.traverse(scene)
    vertex_pos_key       = 'rect.vertex_positions'
    vertex_normals_key   = 'rect.vertex_normals'
    vertex_texcoords_key = 'rect.vertex_texcoords'
    dr.enable_grad(params[vertex_pos_key])
    dr.enable_grad(params[vertex_normals_key])
    dr.enable_grad(params[vertex_texcoords_key])
    params.set_dirty(vertex_pos_key)
    params.set_dirty(vertex_normals_key)
    params.set_dirty(vertex_texcoords_key)
    params.update()

    # Hit the upper right corner of the rectangle (the 4th vertex)
    ray = mi.Ray3f(mi.Vector3f(0.99999, 0.99999, -10.0), mi.Vector3f(0.0, 0.0, 1.0))
    pi = scene.ray_intersect_preliminary(ray, coherent=True)

    # ---------------------------------------
    # Test vertex positions

    # If si.t changes, so the 4th vertex should move along the z-axis
    si = pi.compute_surface_interaction(ray)
    dr.backward(si.t)
    assert dr.allclose(dr.grad(params[vertex_pos_key]),
                       [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1], atol=1e-5)

    # If si.p moves along the z-axis, so does the 4th vertex
    dr.set_grad(params[vertex_pos_key], 0.0)
    si = pi.compute_surface_interaction(ray)
    dr.backward(si.p.z)
    assert dr.allclose(dr.grad(params[vertex_pos_key]),
                       [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1], atol=1e-5)

    # To increase si.dp_du along the x-axis, we need to stretch the upper edge of the rectangle
    dr.set_grad(params[vertex_pos_key], 0.0)
    si = pi.compute_surface_interaction(ray)
    dr.backward(si.dp_du.x)
    assert dr.allclose(dr.grad(params[vertex_pos_key]),
                       [0, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0], atol=1e-5)

    # To increase si.dp_du along the y-axis, we need to transform the rectangle into a trapezoid
    dr.set_grad(params[vertex_pos_key], 0.0)
    si = pi.compute_surface_interaction(ray)
    dr.backward(si.dp_du.y)
    assert dr.allclose(dr.grad(params[vertex_pos_key]),
                       [0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0], atol=1e-5)

    # To increase si.dp_dv along the x-axis, we need to transform the rectangle into a trapezoid
    dr.set_grad(params[vertex_pos_key], 0.0)
    si = pi.compute_surface_interaction(ray)
    dr.backward(si.dp_dv.x)
    assert dr.allclose(dr.grad(params[vertex_pos_key]),
                       [-1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0], atol=1e-5)

    # To increase si.dp_dv along the y-axis, we need to stretch the right edge of the rectangle
    dr.set_grad(params[vertex_pos_key], 0.0)
    si = pi.compute_surface_interaction(ray)
    dr.backward(si.dp_dv.y)
    assert dr.allclose(dr.grad(params[vertex_pos_key]),
                       [0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0], atol=1e-5)

    # To increase si.n along the x-axis, we need to rotate the right edge around the y axis
    dr.set_grad(params[vertex_pos_key], 0.0)
    si = pi.compute_surface_interaction(ray)
    dr.backward(si.n.x)
    assert dr.allclose(dr.grad(params[vertex_pos_key]),
                       [0, 0, 0, 0, 0, 0.5, 0, 0, 0, 0, 0, -0.5], atol=1e-5)

    # To increase si.n along the y-axis, we need to rotate the top edge around the x axis
    dr.set_grad(params[vertex_pos_key], 0.0)
    si = pi.compute_surface_interaction(ray)
    dr.backward(si.n.y)
    assert dr.allclose(dr.grad(params[vertex_pos_key]),
                       [0, 0, 0.5, 0, 0, 0, 0, 0, 0, 0, 0, -0.5], atol=1e-5)

    # To increase si.sh_frame.n along the x-axis, we need to rotate the right edge around the y axis
    dr.set_grad(params[vertex_pos_key], 0.0)
    params.set_dirty(vertex_pos_key); params.update()
    si = pi.compute_surface_interaction(ray)
    dr.backward(si.sh_frame.n.x)
    assert dr.allclose(dr.grad(params[vertex_pos_key]),
                       [0, 0, 0, 0, 0, 0.5, 0, 0, 0, 0, 0, -0.5], atol=1e-5)

    # To increase si.sh_frame.n along the y-axis, we need to rotate the top edge around the x axis
    dr.set_grad(params[vertex_pos_key], 0.0)
    params.set_dirty(vertex_pos_key); params.update()
    si = pi.compute_surface_interaction(ray)
    dr.backward(si.sh_frame.n.y)
    assert dr.allclose(dr.grad(params[vertex_pos_key]),
                       [0, 0, 0.5, 0, 0, 0, 0, 0, 0, 0, 0, -0.5], atol=1e-5)

    # ---------------------------------------
    # Test vertex texcoords

    # To increase si.uv along the x-axis, we need to move the uv of the 4th vertex along the x-axis
    dr.set_grad(params[vertex_texcoords_key], 0.0)
    si = pi.compute_surface_interaction(ray)
    dr.backward(si.uv.x)
    assert dr.allclose(dr.grad(params[vertex_texcoords_key]),
                       [0, 0, 0, 0, 0, 0, 1, 0], atol=1e-5)

    # To increase si.uv along the y-axis, we need to move the uv of the 4th vertex along the y-axis
    dr.set_grad(params[vertex_texcoords_key], 0.0)
    si = pi.compute_surface_interaction(ray)
    dr.backward(si.uv.y)
    assert dr.allclose(dr.grad(params[vertex_texcoords_key]),
                       [0, 0, 0, 0, 0, 0, 0, 1], atol=1e-5)

    # To increase si.dp_du along the x-axis, we need to shrink the uv along the top edge of the rectangle
    dr.set_grad(params[vertex_texcoords_key], 0.0)
    si = pi.compute_surface_interaction(ray)
    dr.backward(si.dp_du.x)
    assert dr.allclose(dr.grad(params[vertex_texcoords_key]),
                       [0, 0, 2, 0, 0, 0, -2, 0], atol=1e-5)

    # To increase si.dp_du along the y-axis, we need to shrink the uv along the right edge of the rectangle
    dr.set_grad(params[vertex_texcoords_key], 0.0)
    si = pi.compute_surface_interaction(ray)
    dr.backward(si.dp_dv.y)
    assert dr.allclose(dr.grad(params[vertex_texcoords_key]),
                       [0, 2, 0, 0, 0, 0, 0, -2], atol=1e-5)


@fresolver_append_path
@pytest.mark.parametrize("jit_flags", jit_flags_options)
def test17_sticky_differentiable_surface_interaction_params_forward(variants_all_ad_rgb, jit_flags):
    # Set drjit JIT flags
    for k, v in jit_flags.items():
        dr.set_flag(k, v)

    scene = mi.load_dict({
        "type" : "scene",
        "meshes": {
            "type" : "obj",
            "id" : "rect",
            "filename" : "resources/data/common/meshes/rectangle.obj",
        }
    })

    params = mi.traverse(scene)
    shape_param_key = 'rect.vertex_positions'
    positions_buf = params[shape_param_key]
    positions_initial = dr.unravel(mi.Point3f, positions_buf)

    # Create differential parameter to be optimized
    diff_vector = mi.Vector3f(0.0)
    dr.enable_grad(diff_vector)

    # Apply the transformation to mesh vertex position and update scene
    def apply_transformation(trasfo):
        trasfo = trasfo(diff_vector)
        new_positions = trasfo @ positions_initial
        params[shape_param_key] = dr.ravel(new_positions)
        params.set_dirty(shape_param_key)
        params.update()

    # ---------------------------------------
    # Test translation

    ray = mi.Ray3f(mi.Vector3f(0.2, 0.3, -5.0), mi.Vector3f(0.0, 0.0, 1.0))
    pi = scene.ray_intersect_preliminary(ray, coherent=True)

    # If the vertices are shifted along x-axis, si.p won't move
    apply_transformation(lambda v : mi.Transform4f.translate(v))
    si = pi.compute_surface_interaction(ray)
    p = si.p + mi.Float(0.001) # Ensure p is a AD leaf node
    dr.forward(diff_vector.x)
    assert dr.allclose(dr.grad(p), [0.0, 0.0, 0.0], atol=1e-5)

    # If the vertices are shifted along x-axis, sticky si.p should follow
    apply_transformation(lambda v : mi.Transform4f.translate(v))
    si = pi.compute_surface_interaction(ray, mi.RayFlags.All | mi.RayFlags.FollowShape)
    p = si.p + mi.Float(0.001) # Ensure p is a AD leaf node
    dr.forward(diff_vector.x)
    assert dr.allclose(dr.grad(p), [1.0, 0.0, 0.0], atol=1e-5)

    # If the vertices are shifted along x-axis, si.uv should move
    apply_transformation(lambda v : mi.Transform4f.translate(v))
    si = pi.compute_surface_interaction(ray)
    dr.forward(diff_vector.x)
    assert dr.allclose(dr.grad(si.uv), [-0.5, 0.0], atol=1e-5)

    # If the vertices are shifted along x-axis, sticky si.uv shouldn't move
    apply_transformation(lambda v : mi.Transform4f.translate(v))
    si = pi.compute_surface_interaction(ray, mi.RayFlags.All | mi.RayFlags.FollowShape)
    dr.forward(diff_vector.x)
    assert dr.allclose(dr.grad(si.uv), [0.0, 0.0], atol=1e-5)

    # TODO fix this!
    # If the vertices are shifted along x-axis, sticky si.t should follow
    # apply_transformation(lambda v : Transform4f.translate(v))
    # si = pi.compute_surface_interaction(ray, RayFlags.All | RayFlags.FollowShape)
    # dr.forward(diff_vector.y)
    # assert dr.allclose(dr.grad(si.t), 10.0, atol=1e-5)

    # TODO add tests for normals on curved mesh (sticky normals shouldn't move)


@fresolver_append_path
@pytest.mark.parametrize("res", [4, 7])
@pytest.mark.parametrize("wall", [False, True])
@pytest.mark.parametrize("jit_flags", jit_flags_options)
def test18_sticky_vcall_ad_fwd(variants_all_ad_rgb, res, wall, jit_flags):
    # Set drjit JIT flags
    for k, v in jit_flags.items():
        dr.set_flag(k, v)

    # Create scene
    scene_dict = {
        'type' : 'scene',
        'sphere' : {
            'type' : 'obj',
            'id' : 'sphere',
            'filename' : 'resources/data/common/meshes/sphere.obj'
        }
    }
    if wall:
        scene_dict['wall'] = {
            'type' : 'obj',
            'id' : 'wall',
            'filename' : 'resources/data/common/meshes/cbox/back.obj'
        }
    scene = mi.load_dict(scene_dict)

    # Get scene parameters
    params = mi.traverse(scene)
    key = 'sphere.vertex_positions'

    # Create differential parameter
    theta = mi.Float(0.0)
    dr.enable_grad(theta)
    dr.set_label(theta, 'theta')

    # Attach object vertices to differential parameter
    with dr.Scope("Attach object vertices"):
        positions_initial = dr.unravel(mi.Point3f, params[key])
        transform = mi.Transform4f.translate(mi.Vector3f(0.0, theta, 0.0))
        positions_new = transform @ positions_initial
        positions_new = dr.ravel(positions_new)
        dr.set_label(positions_new, 'positions_new')
        del transform
        # print(dr.graphviz_str(Float(1)))
        params[key] = positions_new
        params.update()
        dr.set_label(params[key], 'positions_post_update')
        dr.set_label(params['sphere.vertex_normals'], 'vertex_normals')
        # print(dr.graphviz_str(Float(1)))

    spp = 1
    film_size = mi.ScalarVector2i(res)

    # Sample a wavefront of rays (one per pixel and spp)
    total_sample_count = dr.prod(film_size) * spp
    pos = dr.arange(mi.UInt32, total_sample_count)
    pos //= spp
    scale = dr.rcp(mi.Vector2f(film_size))
    pos = mi.Vector2f(mi.Float(pos %  int(film_size[0])),
                   mi.Float(pos // int(film_size[0])))
    pos = 2.0 * (pos / (film_size - 1.0) - 0.5)

    ray = mi.Ray3f([pos[0], pos[1], -5], [0, 0, 1])
    dr.set_label(ray, 'ray')

    # Intersect rays against objects in the scene
    si = scene.ray_intersect(ray, mi.RayFlags.FollowShape, True)
    dr.set_label(si, 'si')

    # print(dr.graphviz_str(Float(1)))

    dr.forward(theta)

    hit_sphere = si.t < 6.0
    assert dr.allclose(dr.grad(si.p), dr.select(hit_sphere, mi.Vector3f(0, 1, 0), mi.Vector3f(0, 0, 0)))


@fresolver_append_path
def test19_update_geometry(variants_vec_rgb):
    scene = mi.load_dict({
        'type': 'scene',
        'rect': {
            'type': 'ply',
            'id': 'rect',
            'filename': 'resources/data/tests/ply/rectangle_normals_uv.ply'
        }
    })

    params = mi.traverse(scene)

    init_vertex_pos = dr.unravel(mi.Point3f, params['rect.vertex_positions'])

    def translate(v):
        transform = mi.Transform4f.translate(mi.Vector3f(v))
        positions_new = transform @ init_vertex_pos
        params['rect.vertex_positions'] = dr.ravel(positions_new)
        params.update()

    film_size = mi.ScalarVector2i([4, 4])
    total_sample_count = dr.prod(film_size)
    pos = dr.arange(mi.UInt32, total_sample_count)
    scale = dr.rcp(mi.Vector2f(film_size))
    pos = mi.Vector2f(mi.Float(pos %  int(film_size[0])),
                      mi.Float(pos // int(film_size[0])))
    pos = 2.0 * (pos / (film_size - 1.0) - 0.5)

    ray = mi.Ray3f([pos[0], -5, pos[1]], [0, 1, 0])
    init_t = scene.ray_intersect_preliminary(ray, coherent=True).t
    dr.eval(init_t)

    v = [0, 0, 10]
    translate(v)
    ray.o += v
    t = scene.ray_intersect_preliminary(ray, coherent=True).t
    ray.o -= v
    assert(dr.allclose(t, init_t))

    v = [-5, 0, 10]
    translate(v)
    ray.o += v
    t = scene.ray_intersect_preliminary(ray, coherent=True).t
    ray.o -= v
    assert(dr.allclose(t, init_t))


@fresolver_append_path
def test20_write_xml(variants_all_rgb, tmp_path):
    filepath = str(tmp_path / 'test_mesh-test20_write_xml.ply')
    print(f"Output temporary file: {filepath}")

    mesh = mi.load_dict({
        'type': 'ply',
        'filename': 'resources/data/tests/ply/rectangle_normals_uv.ply'
    })
    params = mi.traverse(mesh)
    positions = params['vertex_positions'].copy_()

    # Modify one buffer, to check if JIT modes are properly evaluated when saving
    params['vertex_positions'] += 10
    # Add a mesh attribute, to check if they are properly migrated in CUDA modes
    buf_name = 'vertex_test'
    mesh.add_attribute(buf_name, 1, [1,2,3,4])

    mesh.write_ply(filepath)
    mesh_saved = mi.load_dict({
        'type': 'ply',
        'filename': filepath
    })
    params_saved = mi.traverse(mesh_saved)

    assert dr.allclose(params_saved['vertex_positions'], positions + 10.0)
    assert buf_name in params_saved and dr.allclose(params_saved[buf_name], [1, 2, 3, 4])


@fresolver_append_path
def test21_boundary_test_sh_normal(variant_llvm_ad_rgb):
    scene = mi.load_dict({
        'type': 'scene',
        'mesh': {
            'type' : 'obj',
            'filename' : 'resources/data/common/meshes/sphere.obj'
        }
    })

    # Check boundary test value at silhouette
    ray = mi.Ray3f([1.0, 0, -2], [0, 0, 1], 0.0, [])
    B = scene.ray_intersect(ray, mi.RayFlags.BoundaryTest, True).boundary_test

    assert dr.all(B < 1e-6)

    # Check that boundary test value increase as we move away from boundary
    N = 10
    prev = 0.0
    for x in range(N):
        ray = mi.Ray3f([1.0 - float(x) / N, 0, -2], [0, 0, 1], 0.0, [])
        B = scene.ray_intersect(ray, mi.RayFlags.BoundaryTest, True).boundary_test
        assert dr.all(prev < B)
        prev = B


@fresolver_append_path
def test22_boundary_test_face_normal(variants_all_ad_rgb):
    scene = mi.load_dict({
        'type': 'scene',
        'mesh': {
            'type' : 'obj',
            'filename' : 'resources/data/common/meshes/rectangle.obj',
            'face_normals': True
        }
    })

    # Check boundary test value when no intersection
    ray = mi.Ray3f([2, 0, -1], [0, 0, 1], 0.0, [])
    si = scene.ray_intersect(ray, mi.RayFlags.BoundaryTest, True)
    assert dr.all(~si.is_valid())
    B = si.boundary_test
    assert dr.all(B > 1e6)

    # Check boundary test value close to silhouette
    ray = mi.Ray3f([0.9999, 0.9999, -1], [0, 0, 1], 0.0, [])
    B = scene.ray_intersect(ray, mi.RayFlags.BoundaryTest, True).boundary_test
    assert dr.all(B < 1e-3)

    # Check boundary test value close to silhouette
    ray = mi.Ray3f([0.99999, 0.0, -1], [0, 0, 1], 0.0, [])
    B = scene.ray_intersect(ray, mi.RayFlags.BoundaryTest, True).boundary_test
    assert dr.all(B < 1e-4)

    # Check boundary test value close far from silhouette
    ray = mi.Ray3f([0.9, 0.0, -1], [0, 0, 1], 0.0, [])
    B = scene.ray_intersect(ray, mi.RayFlags.BoundaryTest, True).boundary_test
    assert dr.all(B > 1e-1)


@fresolver_append_path
def test23_write_stream(variants_all_rgb, tmp_path):
    filepath = str(tmp_path / 'test_mesh-test23_write_stream.ply')
    mesh = mi.load_dict({
        'type': 'ply',
        'filename': 'resources/data/tests/ply/rectangle_normals_uv.ply'
    })
    params = mi.traverse(mesh)
    fs = mi.FileStream(filepath, mi.FileStream.ETruncReadWrite)
    mesh.write_ply(fs)
    fs.close()

    mesh_saved = mi.load_dict({
        'type': 'ply',
        'filename': filepath
    })
    params_saved = mi.traverse(mesh_saved)
    assert dr.allclose(params_saved['vertex_positions'], params['vertex_positions'])

    ms = mi.MemoryStream()
    fs = mi.FileStream(filepath, mi.FileStream.ERead)
    mesh.write_ply(ms)
    assert fs.size() == ms.size()


@fresolver_append_path
def test24_texture_attributes(variants_all_rgb):

    texture = mi.load_dict({
        "type": "bitmap",
        "filename" : "resources/data/common/textures/flower.bmp",
    })

    mesh = mi.load_dict({
        "type" : "obj",
        "id" : "rect",
        "filename" : "resources/data/common/meshes/rectangle.obj",
        "attribute_1": texture
    })

    assert dr.all(mesh.has_attribute('attribute_1'))
    assert not dr.any(mesh.has_attribute('foo'))

    si = mi.SurfaceInteraction3f()
    si.uv = mi.Point2f(0.5)

    assert dr.allclose(mesh.eval_attribute('attribute_1', si), texture.eval(si))


@fresolver_append_path
def test25_flip_tex_coords_obj(variants_all_rgb, tmp_path):
    mesh = mi.load_dict({
        'type': 'obj',
        'filename': 'resources/data/tests/obj/rectangle_uv.obj',
        'flip_tex_coords': False
    })
    params = mi.traverse(mesh)

    mesh_flipped = mi.load_dict({
        'type': 'obj',
        'filename': 'resources/data/tests/obj/rectangle_uv.obj',
        'flip_tex_coords': True
    })
    params_flipped = mi.traverse(mesh_flipped)

    uv = params['vertex_texcoords'].numpy()
    uv_flipped = params_flipped['vertex_texcoords'].numpy()

    assert (uv[::2] == uv_flipped[::2]).all()
    assert (uv[1::2] == 1 - uv_flipped[1::2]).all()


@fresolver_append_path
def test26_flip_tex_coords_ply(variants_all_rgb, tmp_path):
    mesh = mi.load_dict({
        'type': 'ply',
        'filename': 'resources/data/tests/ply/rectangle_uv.ply',
        'flip_tex_coords': False
    })
    params = mi.traverse(mesh)

    mesh_flipped = mi.load_dict({
        'type': 'ply',
        'filename': 'resources/data/tests/ply/rectangle_uv.ply',
        'flip_tex_coords': True
    })
    params_flipped = mi.traverse(mesh_flipped)

    uv = params['vertex_texcoords'].numpy()
    uv_flipped = params_flipped['vertex_texcoords'].numpy()

    assert (uv[::2] == uv_flipped[::2]).all()
    assert (uv[1::2] == 1 - uv_flipped[1::2]).all()
