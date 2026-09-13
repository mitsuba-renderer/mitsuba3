"""
Behavior shared by the analytic shapes: the orientation of their normals
under ``flip_normals`` and mirrored transformations.
"""

import pytest
import drjit as dr
import mitsuba as mi

# Object-space normal at an object-space point of the unit-sized shape
LOCAL_NORMAL = {
    'sphere':    lambda p: mi.Normal3f(p),
}

# Surface area of the unit-sized shapes
AREA = {'sphere': 4 * dr.pi}


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


@pytest.mark.parametrize('shape', ['sphere'])
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
