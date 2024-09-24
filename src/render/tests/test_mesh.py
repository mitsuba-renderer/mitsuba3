import pytest
import drjit as dr
import mitsuba as mi

from mitsuba.scalar_rgb.test.util import fresolver_append_path


def mixed_shapes_scene():
    return mi.load_dict({
        "type": "scene",
        "shape1": {
            "type" : "ply",
            "filename" : "resources/data/tests/ply/rectangle_uv.ply",
        },
        "shape2": {
            "type" : "rectangle",
        },
        "shape3": {
            "type" : "ply",
            "filename" : "resources/data/tests/ply/rectangle_uv.ply",
        },
    }, parallel=False)



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

    Index = dr.scalar.ArrayXi
    Float = dr.scalar.ArrayXf

    assert not m.has_vertex_normals()
    assert dr.width(positions) == 9
    assert dr.allclose(dr.gather(Float, positions, dr.arange(Index, 0, 3)), [0, 0, 0])
    assert dr.allclose(dr.gather(Float, positions, dr.arange(Index, 3, 6)), [0, 0, 1])
    assert dr.allclose(dr.gather(Float, positions, dr.arange(Index, 6, 9)), [0, 1, 0])
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

    Index = dr.scalar.ArrayXi
    Float = dr.scalar.ArrayXf

    assert shape.has_vertex_normals()
    # Normals are stored in half precision
    assert dr.allclose(dr.gather(Float, normals, dr.arange(Index, 0, 3)), [-1, 0, 0])
    assert dr.allclose(dr.gather(Float, normals, dr.arange(Index, 3, 6)), [-1, 0, 0])
    assert dr.allclose(dr.gather(Float, normals, dr.arange(Index, 6, 9)), [-1, 0, 0])


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

    Index = dr.scalar.ArrayXi
    Float = dr.scalar.ArrayXf

    for i in range(5):
        assert dr.allclose(dr.gather(Float, params['vertex_normals'], dr.arange(Index, i*3, (i+1)*3)), n[:, i], 5e-4)


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

        Index = dr.scalar.ArrayXi
        Faces = dr.scalar.ArrayXu
        Float = dr.scalar.ArrayXf

        assert shape.has_vertex_normals()
        assert dr.width(positions) == 72
        assert dr.width(faces) == 36
        assert dr.allclose(dr.gather(Faces, faces, dr.arange(Index, 6, 9)), [4, 5, 6])
        assert dr.allclose(dr.gather(Float, positions, dr.arange(Index, 5)), [130, 165, 65, 82, 165])


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

        Index = dr.scalar.ArrayXi
        Float = dr.scalar.ArrayXf

        (v0, v2, v3) = [dr.gather(Float, positions, dr.arange(Index, i*3,(i+1)*3)) for i in [0, 2, 3]]

        assert dr.allclose(v0, [-2.85, 0.0, -7.600000], atol=1e-3)
        assert dr.allclose(v2, [ 2.85, 0.0,  0.599999], atol=1e-3)
        assert dr.allclose(v3, [ 2.85, 0.0, -7.600000], atol=1e-3)

        if 'uv' in features:
            assert shape.has_vertex_texcoords()
            (uv0, uv2, uv3) = [dr.gather(Float, texcoords, dr.arange(Index, i*2, (i+1)*2)) for i in [0, 2, 3]]
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
            for n in [dr.gather(Float, normals, dr.arange(Index, i*3, (i+1)*3)) for i in [0, 2, 3]]:
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
    if dr.is_jit_v(mi.Float):
        emitter = shape.emitter()
        N = 4
        mask = mi.Bool(False, True, False, True)
        emitters = mi.EmitterPtr(emitter)
        it = dr.zeros(mi.Interaction3f, N)
        it.p = [0, 0, -3]
        it.t = 0
        ds, _ = emitters.sample_direction(it, [0.5, 0.5], mask)
        assert dr.allclose(ds.uv, dr.select(mask, mi.Point2f(0.5), mi.Point2f(0.0)))


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

    assert not shape.parameters_grad_enabled()

    # Get the shape's parameters
    params = mi.traverse(shape)

    # Only parameters of the shape should affect the result of that method
    bsdf_param_key = 'bsdf.reflectance.value'
    dr.enable_grad(params[bsdf_param_key])
    params.set_dirty(bsdf_param_key)
    params.update()
    assert not shape.parameters_grad_enabled()

    # When setting one of the shape's param to require gradient, method should return True
    shape_param_key = 'vertex_positions'
    dr.enable_grad(params[shape_param_key])
    params.set_dirty(shape_param_key)
    params.update()
    assert shape.parameters_grad_enabled()

if hasattr(dr, 'JitFlag'):
    jit_flags_options = [
        {dr.JitFlag.VCallRecord : False,    dr.JitFlag.VCallOptimize : False,   dr.JitFlag.LoopRecord : False},
        {dr.JitFlag.VCallRecord : True,     dr.JitFlag.VCallOptimize : False,   dr.JitFlag.LoopRecord : False},
        {dr.JitFlag.VCallRecord : True,     dr.JitFlag.VCallOptimize : True,    dr.JitFlag.LoopRecord : False},
    ]
else:
    jit_flags_options = []

@fresolver_append_path
def test12_differentiable_surface_interaction_automatic(variants_all_ad_rgb):
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
    apply_transformation(lambda v : mi.Transform4f().translate(v))
    si = pi.compute_surface_interaction(ray)
    dr.forward(diff_vector.z)
    assert dr.allclose(dr.grad(si.t), 1)

    # If the vertices are shifted along z-axis, so does si.p
    apply_transformation(lambda v : mi.Transform4f().translate(v))
    si = pi.compute_surface_interaction(ray)
    p = si.p + mi.Float(0.001) # Ensure p is a AD leaf node
    dr.forward(diff_vector.z)
    assert dr.allclose(dr.grad(p), [0.0, 0.0, 1.0])

    # If the vertices are shifted along x-axis, so does si.uv (times 0.5)
    apply_transformation(lambda v : mi.Transform4f().translate(v))
    si = pi.compute_surface_interaction(ray)
    dr.forward(diff_vector.x)
    assert dr.allclose(dr.grad(si.uv), [-0.5, 0.0])

    # If the vertices are shifted along y-axis, so does si.uv (times 0.5)
    apply_transformation(lambda v : mi.Transform4f().translate(v))
    si = pi.compute_surface_interaction(ray)
    dr.forward(diff_vector.y)
    assert dr.allclose(dr.grad(si.uv), [0.0, -0.5])

    # ---------------------------------------
    # Test rotation

    ray = mi.Ray3f(mi.Vector3f(-0.99999, -0.99999, -10.0), mi.Vector3f(0.0, 0.0, 1.0))
    pi = scene.ray_intersect_preliminary(ray, coherent=True)

    # If the vertices are rotated around the center, so does si.uv (times 0.5)
    apply_transformation(lambda v : mi.Transform4f().rotate([0, 0, 1], v.x))
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
    apply_transformation(lambda v : mi.Transform4f().translate(v))
    si = pi.compute_surface_interaction(ray)
    p = si.p + mi.Float(0.001) # Ensure p is a AD leaf node
    dr.forward(diff_vector.x)
    assert dr.allclose(dr.grad(p), [0.0, 0.0, 0.0], atol=1e-5)

    # If the vertices are shifted along x-axis, sticky si.p should follow
    apply_transformation(lambda v : mi.Transform4f().translate(v))
    si = pi.compute_surface_interaction(ray, mi.RayFlags.All | mi.RayFlags.FollowShape)
    p = si.p + mi.Float(0.001) # Ensure p is a AD leaf node
    dr.forward(diff_vector.x)
    assert dr.allclose(dr.grad(p), [1.0, 0.0, 0.0], atol=1e-5)

    # If the vertices are shifted along x-axis, si.uv should move
    apply_transformation(lambda v : mi.Transform4f().translate(v))
    si = pi.compute_surface_interaction(ray)
    dr.forward(diff_vector.x)
    assert dr.allclose(dr.grad(si.uv), [-0.5, 0.0], atol=1e-5)

    # If the vertices are shifted along x-axis, sticky si.uv shouldn't move
    apply_transformation(lambda v : mi.Transform4f().translate(v))
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
    positions_initial = dr.unravel(mi.Point3f, params[key])
    transform = mi.Transform4f().translate(mi.Vector3f(0.0, theta, 0.0))
    positions_new = transform @ positions_initial
    positions_new = dr.ravel(positions_new)
    dr.set_label(positions_new, 'positions_new')
    del transform
    params[key] = positions_new
    params.update()
    dr.set_label(params[key], 'positions_post_update')
    dr.set_label(params['sphere.vertex_normals'], 'vertex_normals')

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
        transform = mi.Transform4f().translate(mi.Vector3f(v))
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
    positions = type(params['vertex_positions'])(params['vertex_positions'])

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
def test22_write_stream(variants_all_rgb, tmp_path):
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
def test23_texture_attributes(variants_all_rgb):

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
def test24_flip_tex_coords_obj(variants_all_rgb, tmp_path):
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
def test25_flip_tex_coords_ply(variants_all_rgb, tmp_path):
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


@fresolver_append_path
def test26_sample_silhouette_wrong_type(variants_all_rgb):
    mesh = mi.load_dict({
        "type" : "ply",
        "filename" : "resources/data/tests/ply/triangle.ply",
        "face_normals" : True
    })
    ss = mesh.sample_silhouette([0.1, 0.2, 0.3],
                                mi.DiscontinuityFlags.InteriorType)

    assert ss.discontinuity_type == mi.DiscontinuityFlags.Empty.value


@fresolver_append_path
def test27_sample_silhouette(variants_vec_rgb):
    if not dr.is_diff_v(mi.Float):
        pytest.skip("Only relevant in AD-enabled variants!")

    mesh = mi.load_dict({
        "type" : "ply",
        "filename" : "resources/data/tests/ply/triangle.ply",
        "face_normals" : True
    })
    mesh_ptr = mi.ShapePtr(mesh)

    params = mi.traverse(mesh)
    dr.enable_grad(params['vertex_positions'])
    params.update()

    x = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    y = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    z = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    samples = mi.Point3f(dr.meshgrid(x, y, z))

    # Sphere
    flags = mi.DiscontinuityFlags.PerimeterType | mi.DiscontinuityFlags.DirectionSphere
    ss = mesh.sample_silhouette(samples, flags)

    assert dr.all(ss.discontinuity_type == mi.DiscontinuityFlags.PerimeterType.value)
    assert dr.all(ss.p.x == 0)
    assert dr.all(
        (ss.p.y <= 1) & (ss.p.y >= 0) &
        (ss.p.z <= 1) & (ss.p.z >= 0)
    )
    assert dr.all(ss.flags == flags)
    assert dr.allclose(dr.dot(ss.n, ss.d), 0, atol=1e-6)
    perimeter = 2 + dr.sqrt(2)
    assert dr.allclose(ss.pdf, dr.rcp(perimeter) * dr.inv_four_pi)

    assert dr.all((dr.reinterpret_array(mi.UInt32, ss.shape) ==
            dr.reinterpret_array(mi.UInt32, mesh_ptr)))

    # Lune
    flags = mi.DiscontinuityFlags.PerimeterType | mi.DiscontinuityFlags.DirectionLune
    ss = mesh.sample_silhouette(samples, flags)

    valid = ss.is_valid()
    ss = dr.gather(mi.SilhouetteSample3f, ss, dr.compress(valid))

    assert dr.all(ss.discontinuity_type == mi.DiscontinuityFlags.PerimeterType.value)
    assert dr.all(ss.p.x == 0)
    assert dr.all(
        (ss.p.y <= 1) & (ss.p.y >= 0) &
        (ss.p.z <= 1) & (ss.p.z >= 0)
    )
    assert dr.all(ss.flags == flags)
    assert dr.allclose(dr.dot(ss.n, ss.d), 0, atol=1e-6)
    perimeter = 2 + dr.sqrt(2)
    assert dr.allclose(ss.pdf, dr.rcp(perimeter) * dr.inv_four_pi)

    assert dr.all((dr.reinterpret_array(mi.UInt32, ss.shape) ==
            dr.reinterpret_array(mi.UInt32, mesh_ptr)))


@fresolver_append_path
def test28_sample_silhouette_bijective(variants_vec_rgb):
    if not dr.is_diff_v(mi.Float):
        pytest.skip("Only relevant in AD-enabled variants!")

    mesh = mi.load_dict({
        "type" : "ply",
        "filename" : "resources/data/common/meshes/bunny_lowres.ply",
    })

    params = mi.traverse(mesh)
    dr.enable_grad(params['vertex_positions'])
    params.update()

    x = dr.linspace(mi.Float, 1e-3, 1-1e-3, 10)
    y = dr.linspace(mi.Float, 1e-3, 1-1e-3, 10)
    z = dr.linspace(mi.Float, 1e-3, 1-1e-3, 10)
    samples = mi.Point3f(dr.meshgrid(x, y, z))

    # Sphere
    flags = mi.DiscontinuityFlags.PerimeterType | mi.DiscontinuityFlags.DirectionSphere
    ss = mesh.sample_silhouette(samples, flags)
    out = mesh.invert_silhouette_sample(ss)
    valid = ss.is_valid()
    samples_valid = dr.gather(mi.Point3f, samples, dr.compress(valid))
    out_valid = dr.gather(mi.Point3f, out, dr.compress(valid))

    assert dr.allclose(samples_valid, out_valid, atol=1e-7)

    # Lune
    flags = mi.DiscontinuityFlags.PerimeterType | mi.DiscontinuityFlags.DirectionLune
    ss = mesh.sample_silhouette(samples, flags)
    out = mesh.invert_silhouette_sample(ss)
    valid = ss.is_valid()
    samples_valid = dr.gather(mi.Point3f, samples, dr.compress(valid))
    out_valid = dr.gather(mi.Point3f, out, dr.compress(valid))

    assert dr.allclose(samples_valid.x, out_valid.x, atol=1e-7)
    assert dr.allclose(samples_valid.y, out_valid.y, atol=1e-4) # Lune sampling is not numerically robust
    assert dr.allclose(samples_valid.z, out_valid.z, atol=1e-7)


@fresolver_append_path
def test29_discontinuity_types(variants_vec_rgb):
    mesh = mi.load_dict({
        "type" : "ply",
        "filename" : "resources/data/tests/ply/triangle.ply",
        "face_normals" : True
    })

    types = mesh.silhouette_discontinuity_types()
    assert not mi.has_flag(types, mi.DiscontinuityFlags.InteriorType)
    assert mi.has_flag(types, mi.DiscontinuityFlags.PerimeterType)


@fresolver_append_path
def test30_differential_motion(variants_vec_rgb):
    if not dr.is_diff_v(mi.Float):
        pytest.skip("Only relevant in AD-enabled variants!")

    mesh = mi.load_dict({
        "type" : "ply",
        "filename" : "resources/data/common/meshes/bunny_lowres.ply",
    })
    params = mi.traverse(mesh)

    theta = mi.Point3f(0.0)
    dr.enable_grad(theta)
    key = 'vertex_positions'
    positions = dr.unravel(mi.Point3f, params[key])
    translation = mi.Transform4f().translate([theta.x, 2 * theta.y, 3 * theta.z])
    positions = translation @ positions
    params[key] = dr.ravel(positions)
    params.update()

    si = dr.zeros(mi.SurfaceInteraction3f)
    si.prim_index = 0
    si.p = mi.Point3f(1, 0, 0) # doesn't matter
    si.uv = mi.Point2f(0.5, 0.5)

    p_diff = mesh.differential_motion(si)
    dr.forward(theta)
    v = dr.grad(p_diff)

    assert dr.allclose(p_diff, si.p)
    assert dr.allclose(v, [1.0, 2.0, 3.0])


@fresolver_append_path
def test31_primitive_silhouette_projection(variants_vec_rgb):
    mesh = mi.load_dict({
        "type" : "ply",
        "filename" : "resources/data/tests/ply/rectangle_uv.ply",
    })
    mesh.build_directed_edges()

    u = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    v = dr.linspace(mi.Float, 1e-6, 1-1e-6, 10)
    uv = mi.Point2f(dr.meshgrid(u, v))
    si = mesh.eval_parameterization(uv)

    viewpoint = mi.Point3f(0, 0, 5)

    sample = dr.linspace(mi.Float, 1e-6, 1-1e-6, dr.width(uv))
    ss = mesh.primitive_silhouette_projection(
        viewpoint, si, mi.DiscontinuityFlags.PerimeterType, sample, si.is_valid())

    valid = ss.is_valid()
    ss = dr.gather(mi.SilhouetteSample3f, ss, dr.compress(valid))

    assert dr.all(ss.discontinuity_type == mi.DiscontinuityFlags.PerimeterType.value)
    assert dr.allclose(dr.dot(ss.n, ss.d), 0, atol=1e-6)

    mesh_ptr = mi.ShapePtr(mesh)
    assert dr.all((dr.reinterpret_array(mi.UInt32, ss.shape) ==
                    dr.reinterpret_array(mi.UInt32, mesh_ptr)))


@fresolver_append_path
def test32_shape_type(variants_all_rgb):
    mesh = mi.load_dict({
        "type" : "ply",
        "filename" : "resources/data/tests/ply/rectangle_uv.ply",
    })
    assert mesh.shape_type() == mi.ShapeType.Mesh

    if dr.is_jit_v(mi.Float):
        scene = mixed_shapes_scene()
        types = scene.shapes_dr().shape_type()
        assert dr.count(types == mi.ShapeType.Mesh.value) == 2


@fresolver_append_path
def test33_rebuild_area_pmf(variants_vec_rgb):
    mesh = mi.load_dict({
        "type" : "ply",
        "filename" : "resources/data/tests/ply/triangle.ply",
        "face_normals" : True
    })

    surface_area_before = mesh.surface_area()

    params = mi.traverse(mesh)
    key = 'vertex_positions'

    vertices = dr.unravel(mi.Point3f, params[key])
    new_vertices = mi.Transform4f().scale(2) @ vertices
    params[key] = dr.ravel(new_vertices)
    params.update()

    surface_area_after = mesh.surface_area()

    assert surface_area_after == 4 * surface_area_before



@fresolver_append_path
def test34_mesh_ptr(variants_vec_rgb):
    # dr.set_flag(dr.JitFlag.Debug, True)
    scene = mixed_shapes_scene()
    shapes_dr = scene.shapes_dr()

    for i, sh in enumerate(scene.shapes()):
        as_mesh = mi.MeshPtr(sh)
        if sh.is_mesh():
            assert dr.all(dr.gather(mi.MeshPtr, shapes_dr, i) == as_mesh)
            assert as_mesh[0] == shapes_dr[i]
            assert as_mesh[0] == sh
        else:
            assert dr.all(dr.reinterpret_array(mi.UInt32, as_mesh) == 0)
            assert as_mesh[0] is None

    # The `MeshPtr` constructor should automatically zero-out non-Mesh entries.
    meshes = mi.MeshPtr(shapes_dr)
    is_nnz = dr.reinterpret_array(mi.UInt32, meshes) != 0
    assert dr.all(is_nnz == (True, False, True))


@fresolver_append_path
def test35_mesh_vcalls(variants_vec_rgb):
    scene = mixed_shapes_scene()
    expected_ordering = [("shape1", True), ("shape2", False), ("shape3", True)]
    for i, sh in enumerate(scene.shapes()):
        assert (sh.id(), sh.is_mesh()) == expected_ordering[i]
        if sh.is_mesh():
            sh.build_directed_edges()

    shapes = scene.shapes_dr()
    meshes = mi.MeshPtr(shapes)

    # Not strictly needed, since the non-Mesh pointers
    # should already have been zeroed-out.
    active = shapes.is_mesh()
    assert dr.all(active == mi.Mask([True, False, True]))

    # Shapes in the scene are: Mesh, Rectangle, Mesh
    assert dr.all(meshes.vertex_count() == [4, 0, 4])
    assert dr.all(meshes.face_count() == [2, 0, 2])
    assert dr.all(meshes.has_vertex_normals() == active)
    assert dr.all(meshes.has_vertex_texcoords() == active)
    assert not dr.any(meshes.has_mesh_attributes())
    assert not dr.any(meshes.has_face_normals())

    idx = mi.UInt32([0, 99, 1])
    face_idx = meshes.face_indices(idx, active=active)
    edge_idx = meshes.edge_indices(idx, idx, active=active)
    positions = meshes.vertex_position(idx, active=active)
    normals = meshes.vertex_normal(idx, active=active)
    texcoords = meshes.vertex_texcoord(idx, active=active)
    face_normals = meshes.face_normal(idx, active=active)
    opposite = meshes.opposite_dedge(idx, active=active)

    dr.schedule(face_idx, edge_idx, positions, normals, texcoords,
                face_normals, opposite)

    # Check the vcall results against direct calls on the
    # individual Mesh instances.
    for i, sh in enumerate(scene.shapes()):
        if not sh.is_mesh():
            continue
        idx_i = dr.gather(mi.UInt32, idx, i)
        assert dr.all(dr.gather(type(face_idx), face_idx, i)
                      == sh.face_indices(idx_i))
        assert dr.all(dr.gather(type(edge_idx), edge_idx, i)
                      == sh.edge_indices(idx_i, idx_i))
        assert dr.all(dr.gather(type(positions), positions, i)
                      == sh.vertex_position(idx_i))
        assert dr.all(dr.gather(type(normals), normals, i)
                      == sh.vertex_normal(idx_i))
        assert dr.all(dr.gather(type(texcoords), texcoords, i)
                      == sh.vertex_texcoord(idx_i))
        assert dr.all(dr.gather(type(face_normals), face_normals, i)
                      == sh.face_normal(idx_i))
        assert dr.all(dr.gather(type(opposite), opposite, i)
                      == sh.opposite_dedge(idx_i))
