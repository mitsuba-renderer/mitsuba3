"""
Behavior shared by the analytic shapes: the orientation of their normals
under ``flip_normals`` and mirrored transformations, and the inversion of
their UV parameterization.
"""

import pytest
import drjit as dr
import mitsuba as mi

# Object-space normal at an object-space point of the unit-sized shape
LOCAL_NORMAL = {
    'sphere':    lambda p: mi.Normal3f(p),
    'cylinder':  lambda p: mi.Normal3f(p.x, p.y, 0),
    'disk':      lambda p: mi.Normal3f(0, 0, 1),
    'rectangle': lambda p: mi.Normal3f(0, 0, 1),
}

# Surface area of the unit-sized shapes
AREA = {'sphere': 4 * dr.pi, 'cylinder': 2 * dr.pi, 'disk': dr.pi, 'rectangle': 4}


def check_normals(s, shape, flipped):
    """The normals of sample_position() and ray_intersect() agree, and they
    oppose the object-space normal mapped through 'to_world' iff 'flipped'"""
    ps = s.sample_position(0, [0.3, 0.7])
    si = s.ray_intersect(mi.Ray3f(ps.p + 0.5 * ps.n, -ps.n))
    assert dr.all(si.is_valid())
    assert dr.allclose(si.p, ps.p, atol=1e-6)
    assert dr.allclose(si.n, ps.n, atol=1e-6)

    to_world = mi.Transform4f(mi.traverse(s)['to_world'])
    n = to_world @ LOCAL_NORMAL[shape](to_world.inverse() @ ps.p)
    assert dr.all((dr.dot(ps.n, n) < 0) == flipped)


@pytest.mark.parametrize('shape', ['sphere', 'cylinder', 'disk', 'rectangle'])
@pytest.mark.parametrize('flip', [False, True])
@pytest.mark.parametrize('mirrored', [False, True])
def test01_normal_orientation(variants_all_rgb, shape, flip, mirrored):
    """Only 'flip_normals' turns the normals inward. A reflection in 'to_world'
    does not, and 'to_world' parameter updates do not toggle the orientation"""
    T = mi.ScalarTransform4f
    s = mi.load_dict({'type': shape, 'flip_normals': flip,
                      'to_world': T().scale([-1, 1, 1] if mirrored else 1)})
    check_normals(s, shape, flip)

    params = mi.traverse(s)
    for scale in [-2, 2]:
        params['to_world'] = mi.Transform4f().scale(scale)
        params.update()
        check_normals(s, shape, flip)
        assert dr.allclose(s.surface_area(), AREA[shape] * scale**2)


@pytest.mark.parametrize('shape', ['disk', 'rectangle'])
@pytest.mark.parametrize('scale', [0.1, 3.0])
@pytest.mark.parametrize('angle', [30.0, 120.0, 180.0])
def test03_eval_param_consistency(variants_vec_rgb, shape, scale, angle):
    """eval_parameterization() inverts the UV coordinates of position samples
    and ray intersections, also for rotations beyond 90 degrees"""
    T = mi.ScalarTransform4f
    s = mi.load_dict({
        'type': shape,
        'to_world': T().translate([0, 1, 0]) @ T().rotate([0, 1, 0], angle) @
                    T().scale(scale)
    })

    x = dr.linspace(mi.Float, 1e-3, 1 - 1e-3, 10)
    ps = s.sample_position(0, mi.Point2f(dr.meshgrid(x, x)))

    si_param = s.eval_parameterization(ps.uv)
    assert dr.all(si_param.is_valid())
    assert dr.allclose(si_param.p, ps.p, atol=1e-5)
    assert dr.allclose(si_param.uv, ps.uv, atol=1e-5)

    si_ray = s.ray_intersect(mi.Ray3f(ps.p + ps.n, -ps.n))
    assert dr.all(si_ray.is_valid())
    assert dr.allclose(si_ray.p, ps.p, atol=1e-5)
    assert dr.allclose(si_ray.uv, ps.uv, atol=1e-5)


@pytest.mark.parametrize('shape', ['disk', 'rectangle'])
@pytest.mark.parametrize('detach', [False, True])
def test04_eval_param_motion(variants_all_ad_rgb, shape, detach):
    """The parameterized point follows the shape unless it is detached"""
    T = mi.ScalarTransform4f
    s = mi.load_dict({'type': shape,
                      'to_world': T().translate([1, 2, 3]) @ T().scale(2)})

    params = mi.traverse(s)
    theta = mi.Float(0.0)
    dr.enable_grad(theta)
    params['to_world'] = mi.Transform4f().translate([theta, 0, 0]) @ \
                         mi.Transform4f(params['to_world'])
    params.update()

    flags = mi.RayFlags.Default
    if detach:
        flags |= mi.RayFlags.DetachShape
    si = s.eval_parameterization(mi.Point2f(0.3, 0.7), flags)
    dr.forward(theta)
    assert dr.allclose(dr.grad(si.p), [0 if detach else 1, 0, 0])
    assert dr.allclose(dr.grad(si.uv), 0)
