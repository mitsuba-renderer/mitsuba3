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
                si = s.ray_intersect(ray, mi.RayFlags.All | mi.RayFlags.dNSdUV, coherent=True, active=True)
                si_inst = s_inst.ray_intersect(ray, mi.RayFlags.All | mi.RayFlags.dNSdUV, coherent=True, active=True)

                assert si.prim_index == si_inst.prim_index
                assert si.instance is None
                assert si_inst.instance is not None
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

                for dn_flags in [mi.RayFlags.dNGdUV, mi.RayFlags.dNSdUV]:
                    if si_found:
                        si = s.ray_intersect(ray, mi.RayFlags.All | dn_flags, coherent=True, active=True)
                        si_inst = s_inst.ray_intersect(ray, mi.RayFlags.All | dn_flags, coherent=True, active=True)

                        assert si.prim_index == si_inst.prim_index
                        assert si.instance is None
                        assert si_inst.instance is not None
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

    ray = mi.Ray3f([-0.5, -0.5, -12], [0.0, 0.0, 1.0], time, [])
    pi = scene.ray_intersect(ray)
    assert dr.all(pi.is_valid())
    instance_str = str(pi.instance) if scalar_mode else str(pi.instance[0])
    assert '[0.5, 0, 0, -0.5]' in instance_str
    assert '[0, 0.5, 0, -0.5]' in instance_str

    ray = mi.Ray3f([-0.5, 0.5, -12], [0.0, 0.0, 1.0], time, [])
    pi = scene.ray_intersect(ray)
    assert dr.all(pi.is_valid())
    instance_str = str(pi.instance) if scalar_mode else str(pi.instance[0])
    assert '[0.5, 0, 0, -0.5]' in instance_str
    assert '[0, 0.5, 0, 0.5]' in instance_str

    ray = mi.Ray3f([0.5, -0.5, -12], [0.0, 0.0, 1.0], time, [])
    pi = scene.ray_intersect(ray)
    assert dr.all(pi.is_valid())
    instance_str = str(pi.instance) if scalar_mode else str(pi.instance[0])
    assert '[0.5, 0, 0, 0.5]' in instance_str
    assert '[0, 0.5, 0, -0.5]' in instance_str

    ray = mi.Ray3f([0.5, 0.5, -12], [0.0, 0.0, 1.0], time, [])
    pi = scene.ray_intersect(ray)

    assert dr.all(pi.is_valid())

    if scalar_mode:
        assert 'instance=0x0' in str(pi)
    else:
        assert ('instance=[' + '0x0, ' * (width - 1) + '0x0]') in str(pi)


@pytest.mark.parametrize("shape", shapes)
def test04_single_child_group_recovery(variants_vec_backends_once_rgb, shape):
    """A ShapeGroup with exactly one child, instanced with a non-identity
    transform, must recover ``si.shape`` (the child) and ``si.instance`` (the
    Instance) correctly.

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

            # The instanced hit names the Instance as si.instance; the direct
            # hit has none. The recovered child (si.shape) is never the Instance.
            assert dr.all(~valid | (si_inst.instance != dr.zeros(mi.ShapePtr)))
            assert dr.all(si.instance == dr.zeros(mi.ShapePtr))
            assert dr.all(~valid | (si_inst.shape != si_inst.instance))

            # The recovered child geometry must match the directly-placed shape.
            assert dr.allclose(si.t, si_inst.t, atol=2e-2)
            assert dr.allclose(si.p, si_inst.p, atol=2e-2)
            assert dr.allclose(si.n, si_inst.n, atol=2e-2)
