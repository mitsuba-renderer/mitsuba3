import pytest
import drjit as dr
import mitsuba as mi

from mitsuba.scalar_rgb.test.util import fresolver_append_path


@fresolver_append_path
def example_scene(shape, scale=1.0, translate=[0, 0, 0], angle=0.0):
    from mitsuba import ScalarTransform4f as T

    to_world = T().translate(translate) @ T().rotate([0, 1, 0], angle) @ T().scale(scale)

    shape2 = shape.copy()
    shape2['to_world'] = to_world

    s = mi.load_dict({
        'type' : 'scene',
        'shape' : shape2
    })

    s_inst = mi.load_dict({
        'type' : 'scene',
        'group_0' : {
            'type' : 'shapegroup',
            'shape' : shape
        },
        'instance' : {
            'type' : 'instance',
            "group" : {
                "type" : "ref",
                "id" : "group_0"
            },
            'to_world' : to_world
        }
    })

    return s, s_inst


shapes = [
    { 'type' : 'obj', 'filename' : 'resources/data/common/meshes/rectangle.obj' },
    { 'type' : 'rectangle'},
    { 'type' : 'sphere'},
]


@pytest.mark.parametrize("shape", shapes)
def test01_ray_intersect(variant_scalar_rgb, shape):
    s, s_inst = example_scene(shape)

    # grid size
    n = 11
    inv_n = 1.0 / n

    for x in range(n):
        for y in range(n):
            x_coord = (2 * (x * inv_n) - 1) + 0.014
            y_coord = (2 * (y * inv_n) - 1) + 0.057
            ray = mi.Ray3f(o=[x_coord, y_coord + 1, -8], d=[0.0, 0.0, 1.0],
                           time=0.0, wavelengths=[])

            si_found = s.ray_test(ray)
            si_found_inst = s_inst.ray_test(ray)

            assert si_found == si_found_inst

            if si_found:
                flags = mi.RayFlags.Default | mi.RayFlags.NormalPartials
                si = s.ray_intersect(ray, flags, coherent=True, active=True)
                si_inst = s_inst.ray_intersect(ray, flags, coherent=True, active=True)

                assert si.prim_index == si_inst.prim_index
                assert si.instance_index == 0
                assert si_inst.instance_index != 0
                assert dr.allclose(si.t, si_inst.t, atol=2e-2)
                assert dr.allclose(si.time, si_inst.time, atol=2e-2)
                assert dr.allclose(si.p, si_inst.p, atol=2e-2)
                assert dr.allclose(si.sh_frame.n, si_inst.sh_frame.n, atol=2e-2)
                assert dr.allclose(si.dp_du, si_inst.dp_du, atol=2e-2)
                assert dr.allclose(si.dp_dv, si_inst.dp_dv, atol=2e-2)
                assert dr.allclose(si.uv, si_inst.uv, atol=2e-2)
                assert dr.allclose(si.wi, si_inst.wi, atol=2e-2)

                if dr.norm(si.dn_du) > 0.0 and dr.norm(si.dn_dv) > 0.0:
                    assert dr.allclose(si.dn_du, si_inst.dn_du, atol=2e-2)
                    assert dr.allclose(si.dn_dv, si_inst.dn_dv, atol=2e-2)


@pytest.mark.parametrize("shape", shapes)
def test02_ray_intersect_transform(variant_scalar_rgb, shape):
    trans = mi.ScalarVector3f([0, 1, 0])
    angle = 15

    for scale in [0.57, 2.7]:
        s, s_inst = example_scene(shape, scale, trans, angle)

        # grid size
        n = 11
        inv_n = 1.0 / n

        for x in range(n):
            for y in range(n):
                x_coord = scale * (2 * (x * inv_n) - 1)
                y_coord = scale * (2 * (y * inv_n) - 1)

                ray = mi.Ray3f(o=mi.ScalarVector3f([x_coord, y_coord, -12]) + trans,
                               d = [0.0, 0.0, 1.0],
                               time = 0.0, wavelengths = [])

                si_found = s.ray_test(ray)
                si_found_inst = s_inst.ray_test(ray)

                assert si_found == si_found_inst

                if si_found:
                    flags = mi.RayFlags.Default | mi.RayFlags.NormalPartials
                    si = s.ray_intersect(ray, flags, coherent=True, active=True)
                    si_inst = s_inst.ray_intersect(ray, flags, coherent=True, active=True)

                    assert si.prim_index == si_inst.prim_index
                    assert si.instance_index == 0
                    assert si_inst.instance_index != 0
                    assert dr.allclose(si.t, si_inst.t, atol=2e-2)
                    assert dr.allclose(si.time, si_inst.time, atol=2e-2)
                    assert dr.allclose(si.p, si_inst.p, atol=2e-2)
                    assert dr.allclose(si.dp_du, si_inst.dp_du, atol=2e-2)
                    assert dr.allclose(si.dp_dv, si_inst.dp_dv, atol=2e-2)
                    assert dr.allclose(si.uv, si_inst.uv, atol=2e-2)
                    assert dr.allclose(si.wi, si_inst.wi, atol=2e-2)

                    if dr.norm(si.dn_du) > 0.0 and dr.norm(si.dn_dv) > 0.0:
                        assert dr.allclose(si.dn_du, si_inst.dn_du, atol=2e-2)
                        assert dr.allclose(si.dn_dv, si_inst.dn_dv, atol=2e-2)


@pytest.mark.parametrize('width', [1, 10])
def test03_ray_intersect_instance(variants_all_rgb, width):
    """Check that we get the correct instance pointer when tracing a ray"""

    from mitsuba import ScalarTransform4f as T

    scalar_mode = mi.variant().startswith('scalar')

    scene = mi.load_dict({
        'type' : 'scene',

        'group_0' : {
            'type' : 'shapegroup',
            'shape' : {
                'type' : 'rectangle'
            }
        },

        'instance_00' : {
            'type' : 'instance',
            "group" : {
                "type" : "ref",
                "id" : "group_0"
            },
            'to_world' : T().translate([-0.5, -0.5, 0.0]) @ T().scale(0.5)
        },

        'instance_01' : {
            'type' : 'instance',
            "group" : {
                "type" : "ref",
                "id" : "group_0"
            },
            'to_world' : T().translate([-0.5, 0.5, 0.0]) @ T().scale(0.5)
        },

        'instance_10' : {
            'type' : 'instance',
            "group" : {
                "type" : "ref",
                "id" : "group_0"
            },
            'to_world' : T().translate([0.5, -0.5, 0.0]) @ T().scale(0.5)
        },

        'shape' : {
            'type' : 'rectangle',
            'to_world' : T().translate([0.5, 0.5, 0.0]) @ T().scale(0.5)
        }
    })

    time = 0.0 if scalar_mode else [0.0] * width

    def hit_instance_str(si):
        slot = int(si.instance_index if scalar_mode else si.instance_index[0])
        assert slot != 0
        return str(scene.instance(slot - 1))

    ray = mi.Ray3f([-0.5, -0.5, -12], [0.0, 0.0, 1.0], time, [])
    si = scene.ray_intersect(ray)
    assert dr.all(si.is_valid())
    instance_str = hit_instance_str(si)
    assert '[0.5, 0, 0, -0.5]' in instance_str
    assert '[0, 0.5, 0, -0.5]' in instance_str

    ray = mi.Ray3f([-0.5, 0.5, -12], [0.0, 0.0, 1.0], time, [])
    si = scene.ray_intersect(ray)
    assert dr.all(si.is_valid())
    instance_str = hit_instance_str(si)
    assert '[0.5, 0, 0, -0.5]' in instance_str
    assert '[0, 0.5, 0, 0.5]' in instance_str

    ray = mi.Ray3f([0.5, -0.5, -12], [0.0, 0.0, 1.0], time, [])
    si = scene.ray_intersect(ray)
    assert dr.all(si.is_valid())
    instance_str = hit_instance_str(si)
    assert '[0.5, 0, 0, 0.5]' in instance_str
    assert '[0, 0.5, 0, -0.5]' in instance_str

    ray = mi.Ray3f([0.5, 0.5, -12], [0.0, 0.0, 1.0], time, [])
    si = scene.ray_intersect(ray)

    assert dr.all(si.is_valid())
    assert dr.all(si.instance_index == 0)


@pytest.mark.parametrize("shape", shapes)
def test04_single_child_group_recovery(variants_vec_backends_once_rgb, shape):
    """A ShapeGroup with exactly one child, instanced with a non-identity
    transform, must recover ``si.shape`` (the child) and ``si.instance_index``
    (the record slot of the Instance) correctly.

    This exercises the GPU backends' per-geometry hit recovery: a one-child group
    resolves its hit shape from ``pi.shape`` exactly like a multi-child group. The
    directly-placed shape (``s``) is the ground truth the instanced hit
    (``s_inst``) must match field-for-field."""
    s, s_inst = example_scene(shape, scale=0.5, translate=[0.1, 0.2, 0.0],
                              angle=15.0)

    n = 7
    inv_n = 1.0 / n
    for x in range(n):
        for y in range(n):
            x_coord = (2 * (x * inv_n) - 1) + 0.014
            y_coord = (2 * (y * inv_n) - 1) + 0.057
            ray = mi.Ray3f(o=[x_coord, y_coord + 1, -8], d=[0.0, 0.0, 1.0],
                           time=0.0, wavelengths=[])

            # Trace the two scenes in separate kernels. On the OptiX backend a
            # single kernel cannot use more than one pipeline/SBT, and ``s`` and
            # ``s_inst`` each carry their own, so fusing both traces would abort.
            si = s.ray_intersect(ray)
            dr.eval(si)
            si_inst = s_inst.ray_intersect(ray)

            valid = si.is_valid()
            assert dr.all(valid == si_inst.is_valid())
            if dr.none(valid):
                continue

            # The instanced hit reports the Instance's record slot; the
            # direct hit has none.
            assert dr.all(~valid | (si_inst.instance_index != 0))
            assert dr.all(si.instance_index == 0)

            # The recovered child geometry must match the directly-placed shape.
            assert dr.allclose(si.t, si_inst.t, atol=2e-2)
            assert dr.allclose(si.p, si_inst.p, atol=2e-2)
            assert dr.allclose(si.n, si_inst.n, atol=2e-2)


def test05_normal_partials_non_similarity(variant_scalar_rgb):
    """The instance transform must not assume that it is a similarity"""
    from drjit.scalar import ArrayXf as ScalarF
    from mitsuba import ScalarTransform4f as T
    from mitsuba.scalar_rgb.test.util import curved_patch, check_normal_partials

    to_world = T().rotate([0.6, 0.8, 0.0], 37).scale([1.0, 2.0, 3.0])
    assert not to_world.is_similarity()

    mesh, ref = curved_patch()
    scene = mi.load_dict({
        'type': 'scene',
        'group': {'type': 'shapegroup', 'shape': mesh},
        'instance': {'type': 'instance',
                     'group': {'type': 'ref', 'id': 'group'},
                     'to_world': to_world}
    })
    flags = mi.RayFlags.Default | mi.RayFlags.NormalPartials

    # Probe a grid spanning the transformed patch from above
    bbox, hits = scene.bbox(), 0
    for x in dr.linspace(ScalarF, bbox.min.x, bbox.max.x, 10):
        for y in dr.linspace(ScalarF, bbox.min.y, bbox.max.y, 10):
            ray = mi.Ray3f(mi.Point3f(x, y, bbox.max.z + 1),
                           mi.Vector3f(0, 0, -1))
            si = scene.ray_intersect(ray, flags, True)
            if not si.is_valid():
                continue
            hits += 1
            check_normal_partials(si, ref.normal_field(int(si.prim_index)),
                                  to_world)

    assert hits > 5, hits


@pytest.mark.parametrize("api", ['scene', 'pi'])
def test06_ad_gradients(variants_all_ad_rgb, api):
    """Gradients propagate through instanced ray-tracing calls, both with
    respect to the instance-to-world transform and with respect to parameters
    of the instanced geometry. Both the scene-level and the preliminary
    intersection entry points are checked."""

    def make_scene():
        return mi.load_dict({
            'type': 'scene',
            'group': {'type': 'shapegroup', 'shape': {'type': 'sphere'}},
            'instance': {'type': 'instance',
                         'group': {'type': 'ref', 'id': 'group'},
                         'to_world': mi.ScalarTransform4f().translate([5, 0, 0])}
        })

    def intersect(scene, ray, ray_flags):
        if api == 'scene':
            return scene.ray_intersect(ray, ray_flags, True)
        else:
            pi = scene.ray_intersect_preliminary(ray)
            return scene.compute_surface_interaction(ray, pi, ray_flags)

    def grad(key, ray_flags, output):
        """Differentiate 'output' with respect to a z-translation appended to
        the transform parameter 'key'"""
        scene = make_scene()
        params = mi.traverse(scene)
        theta = mi.Float(0.0)
        dr.enable_grad(theta)
        params[key] = mi.Transform4f(params[key]) @ \
                      mi.Transform4f().translate([0, 0, theta])
        params.update()

        ray = mi.Ray3f(mi.Point3f(5, 0, -5), mi.Vector3f(0, 0, 1))
        si = intersect(scene, ray, ray_flags)
        dr.backward(si.p.z if output == 'p.z' else si.t,
                    flags=dr.ADFlag.Default | dr.ADFlag.AllowNoGrad)
        return dr.grad(theta)

    follow = mi.RayFlags.Default | mi.RayFlags.FollowShape
    detach = mi.RayFlags.Default | mi.RayFlags.DetachShape

    # Instance transform: the hit point follows the moving instance
    assert dr.allclose(grad('instance.to_world', follow, 'p.z'), 1.0)

    # Instance transform, default mode: the hit point stays on the ray while
    # the distance tracks the moving tangent plane
    assert dr.allclose(grad('instance.to_world', mi.RayFlags.Default, 't'), 1.0)

    # DetachShape severs the dependence entirely
    assert dr.allclose(grad('instance.to_world', detach, 't'), 0.0)

    # Group-internal shape parameters, reached through the instanced hit
    assert dr.allclose(grad('group.shape.to_world', mi.RayFlags.Default, 't'), 1.0)


@fresolver_append_path
@pytest.mark.parametrize("api", ['scene', 'pi'])
def test07_ad_gradients_vs_direct_mesh(variants_all_ad_rgb, api):
    """Gradient reference test: a mesh under a non-similarity world transform
    must produce the same position/distance derivatives whether the moving
    transform is realized directly (differentiated through the mesh's vertex
    positions) or through an instance (differentiated through the instance's
    to_world parameter)."""
    from mitsuba import ScalarTransform4f as T

    to_world = T().translate([1.5, 0.5, -0.3]) @ T().rotate([0, 1, 0], 30) @ \
               T().rotate([1, 0, 0], -20) @ T().scale([1.0, 2.0, 3.0])
    mesh = {'type': 'obj',
            'filename': 'resources/data/common/meshes/rectangle.obj'}

    # A grid of rays that hits the transformed rectangle
    x, y = dr.meshgrid(dr.linspace(mi.Float, -0.7, 0.7, 4),
                       dr.linspace(mi.Float, -0.7, 0.7, 4))
    p_world = mi.Transform4f(to_world) @ mi.Point3f(x, y, 0)
    d = dr.normalize(mi.Vector3f(0.2, -0.1, -1.0))
    ray = mi.Ray3f(p_world - 5 * d, d)

    # World-space surface motion per unit of the appended local z-translation
    v_world = mi.Transform4f(to_world) @ mi.Vector3f(0, 0, 1)

    def intersect(scene, ray_flags):
        if api == 'scene':
            return scene.ray_intersect(ray, ray_flags, True)
        else:
            pi = scene.ray_intersect_preliminary(ray)
            return scene.compute_surface_interaction(ray, pi, ray_flags)

    def grad(instanced, ray_flags, output):
        theta = mi.Float(0.0)
        dr.enable_grad(theta)
        if instanced:
            scene = mi.load_dict({
                'type': 'scene',
                'group': {'type': 'shapegroup', 'shape': mesh},
                'instance': {'type': 'instance',
                             'group': {'type': 'ref', 'id': 'group'}}})
            params = mi.traverse(scene)
            params['instance.to_world'] = mi.Transform4f(to_world) @ \
                mi.Transform4f().translate([0, 0, theta])
            params.update()
        else:
            scene = mi.load_dict({
                'type': 'scene', 'shape': dict(mesh, to_world=to_world)})
            params = mi.traverse(scene)
            key = 'shape.positions'
            pos = dr.unravel(mi.Point3f, params[key].array)
            params[key] = mi.TensorXf(dr.ravel(pos + v_world * theta),
                                      dr.shape(params[key]))
            params.update()

        si = intersect(scene, ray_flags)
        assert dr.all(si.is_valid())
        dr.backward(dr.sum(si.t if output == 't' else si.p.z))
        return dr.grad(theta)

    follow = mi.RayFlags.Default | mi.RayFlags.FollowShape
    for ray_flags in [mi.RayFlags.Default, follow]:
        for output in ['t', 'p.z']:
            g_direct = grad(False, ray_flags, output)
            g_inst = grad(True, ray_flags, output)
            assert dr.allclose(g_direct, g_inst, rtol=1e-4), \
                (int(ray_flags), output, g_direct[0], g_inst[0])


@fresolver_append_path
def test08_ad_gradients_combined(variants_all_ad_rgb):
    """Gradient reference test for simultaneous differentiation of the
    instance-to-world transform and group-internal vertex positions. The
    gradients of both parameters must match an equivalent non-instanced mesh
    whose vertices realize the two motions directly."""
    from mitsuba import ScalarTransform4f as T

    to_world = T().translate([1.5, 0.5, -0.3]) @ T().rotate([0, 1, 0], 30) @ \
               T().rotate([1, 0, 0], -20) @ T().scale([1.0, 2.0, 3.0])
    mesh = {'type': 'obj',
            'filename': 'resources/data/common/meshes/rectangle.obj'}

    # A grid of rays that hits the transformed rectangle
    x, y = dr.meshgrid(dr.linspace(mi.Float, -0.7, 0.7, 4),
                       dr.linspace(mi.Float, -0.7, 0.7, 4))
    p_world = mi.Transform4f(to_world) @ mi.Point3f(x, y, 0)
    d = dr.normalize(mi.Vector3f(0.2, -0.1, -1.0))
    ray = mi.Ray3f(p_world - 5 * d, d)

    # World-space surface motions per unit of theta (a local z-translation
    # appended to the instance transform) and per unit of phi (a local
    # x-displacement of the group-internal vertices)
    v_theta = mi.Transform4f(to_world) @ mi.Vector3f(0, 0, 1)
    v_phi   = mi.Transform4f(to_world) @ mi.Vector3f(1, 0, 0)

    def displace(params, key, direction, amount):
        pos = dr.unravel(mi.Point3f, params[key].array)
        params[key] = mi.TensorXf(dr.ravel(pos + direction * amount),
                                  dr.shape(params[key]))

    def grads(instanced, ray_flags, output):
        theta, phi = mi.Float(0.0), mi.Float(0.0)
        dr.enable_grad(theta, phi)
        if instanced:
            scene = mi.load_dict({
                'type': 'scene',
                'group': {'type': 'shapegroup', 'shape': mesh},
                'instance': {'type': 'instance',
                             'group': {'type': 'ref', 'id': 'group'}}})
            params = mi.traverse(scene)
            params['instance.to_world'] = mi.Transform4f(to_world) @ \
                mi.Transform4f().translate([0, 0, theta])
            displace(params, 'group.shape.positions', mi.Vector3f(1, 0, 0), phi)
            params.update()
        else:
            scene = mi.load_dict({
                'type': 'scene', 'shape': dict(mesh, to_world=to_world)})
            params = mi.traverse(scene)
            displace(params, 'shape.positions', v_theta, theta)
            displace(params, 'shape.positions', v_phi, phi)
            params.update()

        si = scene.ray_intersect(ray, ray_flags, True)
        assert dr.all(si.is_valid())
        dr.backward(dr.sum(si.t if output == 't' else si.p.z))
        return dr.grad(theta), dr.grad(phi)

    follow = mi.RayFlags.Default | mi.RayFlags.FollowShape
    for ray_flags in [mi.RayFlags.Default, follow]:
        for output in ['t', 'p.z']:
            gd_theta, gd_phi = grads(False, ray_flags, output)
            gi_theta, gi_phi = grads(True, ray_flags, output)
            assert dr.allclose(gd_theta, gi_theta, rtol=1e-4, atol=1e-5), \
                ('theta', int(ray_flags), output, gd_theta[0], gi_theta[0])
            assert dr.allclose(gd_phi, gi_phi, rtol=1e-4, atol=1e-5), \
                ('phi', int(ray_flags), output, gd_phi[0], gi_phi[0])
