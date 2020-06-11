import mitsuba
import pytest
import enoki as ek

from mitsuba.python.test.util import fresolver_append_path

@fresolver_append_path
def example_scene(shape, scale=1.0, translate=[0, 0, 0], angle=0.0):
    from mitsuba.core import xml, ScalarTransform4f as T

    to_world = T.translate(translate) * T.rotate([0, 1, 0], angle) * T.scale(scale)

    shape2 = shape.copy()
    shape2['to_world'] = to_world

    s = xml.load_dict({
        'type' : 'scene',
        'shape' : shape2
    })

    s_inst = xml.load_dict({
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
    from mitsuba.core import Ray3f
    from mitsuba.render import HitComputeFlags

    s, s_inst = example_scene(shape)

    # grid size
    n = 21
    inv_n = 1.0 / n

    for x in range(n):
        for y in range(n):
            x_coord = (2 * (x * inv_n) - 1)
            y_coord = (2 * (y * inv_n) - 1)
            ray = Ray3f(o=[x_coord, y_coord + 1, -8], d=[0.0, 0.0, 1.0],
                        time=0.0, wavelengths=[])

            si_found = s.ray_test(ray)
            si_found_inst = s_inst.ray_test(ray)

            assert si_found == si_found_inst

            if si_found:
                si = s.ray_intersect(ray, HitComputeFlags.All | HitComputeFlags.dNSdUV)
                si_inst = s_inst.ray_intersect(ray, HitComputeFlags.All | HitComputeFlags.dNSdUV)

                assert si.prim_index == si_inst.prim_index
                assert si.instance is None
                assert si_inst.instance is not None
                assert ek.allclose(si.t, si_inst.t, atol=2e-2)
                assert ek.allclose(si.time, si_inst.time, atol=2e-2)
                assert ek.allclose(si.p, si_inst.p, atol=2e-2)
                assert ek.allclose(si.sh_frame.n, si_inst.sh_frame.n, atol=2e-2)
                assert ek.allclose(si.dp_du, si_inst.dp_du, atol=2e-2)
                assert ek.allclose(si.dp_dv, si_inst.dp_dv, atol=2e-2)
                assert ek.allclose(si.uv, si_inst.uv, atol=2e-2)
                assert ek.allclose(si.wi, si_inst.wi, atol=2e-2)

                if ek.norm(si.dn_du) > 0.0 and ek.norm(si.dn_dv) > 0.0:
                    assert ek.allclose(si.dn_du, si_inst.dn_du, atol=2e-2)
                    assert ek.allclose(si.dn_dv, si_inst.dn_dv, atol=2e-2)


@pytest.mark.parametrize("shape", shapes)
def test02_ray_intersect_transform(variant_scalar_rgb, shape):
    from mitsuba.core import Ray3f, ScalarVector3f
    from mitsuba.render import HitComputeFlags

    trans = ScalarVector3f([0, 1, 0])
    angle = 15

    for scale in [0.5, 2.7]:
        s, s_inst = example_scene(shape, scale, trans, angle)

        # grid size
        n = 21
        inv_n = 1.0 / n

        for x in range(n):
            for y in range(n):
                x_coord = scale * (2 * (x * inv_n) - 1)
                y_coord = scale * (2 * (y * inv_n) - 1)

                ray = Ray3f(o=ScalarVector3f([x_coord, y_coord, -12]) + trans,
                            d = [0.0, 0.0, 1.0],
                            time = 0.0, wavelengths = [])

                si_found = s.ray_test(ray)
                si_found_inst = s_inst.ray_test(ray)

                assert si_found == si_found_inst

                for dn_flags in [HitComputeFlags.dNGdUV, HitComputeFlags.dNSdUV]:
                    if si_found:
                        si = s.ray_intersect(ray, HitComputeFlags.All | dn_flags)
                        si_inst = s_inst.ray_intersect(ray, HitComputeFlags.All | dn_flags)

                        assert si.prim_index == si_inst.prim_index
                        assert si.instance is None
                        assert si_inst.instance is not None
                        assert ek.allclose(si.t, si_inst.t, atol=2e-2)
                        assert ek.allclose(si.time, si_inst.time, atol=2e-2)
                        assert ek.allclose(si.p, si_inst.p, atol=2e-2)
                        assert ek.allclose(si.dp_du, si_inst.dp_du, atol=2e-2)
                        assert ek.allclose(si.dp_dv, si_inst.dp_dv, atol=2e-2)
                        assert ek.allclose(si.uv, si_inst.uv, atol=2e-2)
                        assert ek.allclose(si.wi, si_inst.wi, atol=2e-2)

                        if ek.norm(si.dn_du) > 0.0 and ek.norm(si.dn_dv) > 0.0:
                            assert ek.allclose(si.dn_du, si_inst.dn_du, atol=2e-2)
                            assert ek.allclose(si.dn_dv, si_inst.dn_dv, atol=2e-2)



def test03_ray_intersect_instance(variants_all_rgb):
    from mitsuba.core import xml, Ray3f, ScalarVector3f, ScalarTransform4f as T

    """Check that we can the correct instance pointer when tracing a ray"""

    scene = xml.load_dict({
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
            'to_world' : T.translate([-0.5, -0.5, 0.0]) * T.scale(0.5)
        },

        'instance_01' : {
            'type' : 'instance',
            "group" : {
                "type" : "ref",
                "id" : "group_0"
            },
            'to_world' : T.translate([-0.5, 0.5, 0.0]) * T.scale(0.5)
        },

        'instance_10' : {
            'type' : 'instance',
            "group" : {
                "type" : "ref",
                "id" : "group_0"
            },
            'to_world' : T.translate([0.5, -0.5, 0.0]) * T.scale(0.5)
        },

        'shape' : {
            'type' : 'rectangle',
            'to_world' : T.translate([0.5, 0.5, 0.0]) * T.scale(0.5)
        }
    })

    ray = Ray3f([-0.5, -0.5, -12], [0.0, 0.0, 1.0], 0.0, [])
    pi = scene.ray_intersect_preliminary(ray)
    assert '[0.5, 0, 0, -0.5]' in str(pi)
    assert '[0, 0.5, 0, -0.5]' in str(pi)

    ray = Ray3f([-0.5, 0.5, -12], [0.0, 0.0, 1.0], 0.0, [])
    pi = scene.ray_intersect_preliminary(ray)
    assert '[0.5, 0, 0, -0.5]' in str(pi)
    assert '[0, 0.5, 0, 0.5]' in str(pi)

    ray = Ray3f([0.5, -0.5, -12], [0.0, 0.0, 1.0], 0.0, [])
    pi = scene.ray_intersect_preliminary(ray)
    assert '[0.5, 0, 0, 0.5]' in str(pi)
    assert '[0, 0.5, 0, -0.5]' in str(pi)

    ray = Ray3f([0.5, 0.5, -12], [0.0, 0.0, 1.0], 0.0, [])
    pi = scene.ray_intersect_preliminary(ray)
    assert 'instance = nullptr' in str(pi) or 'instance = [nullptr]' in str(pi)
