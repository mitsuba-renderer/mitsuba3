"""
Rays spawned from surface interactions must not hit the surface they start
on, shadow rays between two surfaces must not be falsely occluded, and the
error bounds that shapes report must stay close to the actual roundoff.
"""

import numpy as np
import pytest
import drjit as dr
import mitsuba as mi

# Shapes that report ray(t) as the hit point. Their bound grows with the
# distance of the ray origin.
RAY_BASED = ('sdfgrid', 'linearcurve', 'bsplinecurve', 'ellipsoids')

SHAPES = ['sphere', 'cube', 'cylinder', 'disk', 'rectangle', 'instance',
          'ellipsoidsmesh', *RAY_BASED]

# Shapes that implement sample_position()
SAMPLED = ['sphere', 'cube', 'cylinder', 'disk', 'rectangle']

# Single-sided shapes, whose rays spawned on the inside legitimately hit the
# far wall
OPEN = ('cylinder', 'linearcurve', 'bsplinecurve')

# Two-sided planar shapes, whose samples are visible from both sides
PLANAR = ('disk', 'rectangle')

CONFIGS = [(0.0, 1.0), (1e3, 1.0), (0.0, 1e-3), (1e4, 1e3)]


def shape_dict(shape, center, scale, tmp_path=None, flip=False):
    """Dictionary of a unit-sized ``shape`` scaled by ``scale`` and centered
    at ``center``. ``flip`` turns it upside down. Returns the dictionary and
    the size of the shape. The curve shapes write a file to ``tmp_path``."""
    T = mi.ScalarTransform4f().translate(center)
    if flip:
        T = T @ mi.ScalarTransform4f().rotate([1, 0, 0], 180)
    T = T @ mi.ScalarTransform4f().scale(scale)
    size = scale
    if shape == 'sdfgrid':
        # Level set of a plane through the center of the grid
        z = ((np.arange(16) + 0.5) / 16 - 0.5).astype(np.float32)
        grid = np.broadcast_to(z[:, None, None, None], (16, 16, 16, 1))
        d = {'type': 'sdfgrid', 'grid': mi.TensorXf(grid)}
        T = T @ mi.ScalarTransform4f().translate([-0.5] * 3)
        size = 0.5 * scale
    elif shape == 'instance':
        # A rotated instance of a cube, which exercises the transport of the
        # nested bound through a non-axis-aligned transform
        d = {'type': 'instance', 'shapegroup': {'type': 'ref', 'id': 'grp'}}
        T = T @ mi.ScalarTransform4f().rotate(dr.normalize(mi.ScalarVector3f(1, 1, 0)), 30)
    elif shape == 'cylinder':
        # Lying on its side so that the wall faces up and down
        d = {'type': 'cylinder'}
        T = T @ mi.ScalarTransform4f().rotate([0, 1, 0], 90).translate([0, 0, -0.5])
    elif shape in ('linearcurve', 'bsplinecurve'):
        # A tube along the x axis. Curve radii ignore the 'to_world' scale,
        # so the file is written at the final size
        fname = tmp_path / f'{shape}_{scale}.txt'
        fname.write_text(''.join(f'{x * scale} 0 0 {scale}\n'
                                 for x in (-1.5, -1, -0.3, 0.3, 1, 1.5)))
        d = {'type': shape, 'filename': str(fname)}
        T = mi.ScalarTransform4f().translate(center)
    elif shape in ('ellipsoids', 'ellipsoidsmesh'):
        # A sphere given as an ellipsoid, which does not accept 'to_world'
        # together with a data tensor. The mesh variant tessellates it.
        data = np.float32([[*center, scale, scale, scale, 0, 0, 0, 1]])
        d = {'type': shape, 'data': mi.TensorXf(data), 'extent': 1.0}
        if shape == 'ellipsoidsmesh':
            d['shell'] = 'ico_sphere'
        return d, size
    else:
        d = {'type': shape}
    d['to_world'] = T
    return d, size


def make_scene(shape, offset, scale, tmp_path=None, receiver='rectangle'):
    """A ``shape`` scaled by ``scale`` and translated by ``offset`` along
    every axis, plus a downward-facing 'receiver' shape above it. Returns
    the scene, the center and the size of the shape."""
    center = mi.ScalarPoint3f([offset] * 3)
    d, size = shape_dict(shape, center, scale, tmp_path)
    rsize = 2 * size if receiver == 'rectangle' else size
    r, _ = shape_dict(receiver, center + [0, 0, 3 * size], rsize, tmp_path, flip=True)
    d = {'type': 'scene', 'shape': d, 'receiver': r}
    if shape == 'instance':
        d['grp'] = {'type': 'shapegroup', 'inner': {'type': 'cube'}}
    return mi.load_dict(d), center, size


def shape_by_id(scene, id):
    return next((s for s in scene.shapes() if s.id() == id), None)


def hits(scene, center, size, n, dist=4.0, front_only=False):
    """Hit the shape under test with ``n`` rays from ``dist`` shape sizes
    away, aimed at random points inside the shape. The normals are flipped
    to face the ray origins. Returns the interactions, the mask of hits on
    the shape and the sampler."""
    sampler = mi.load_dict({'type': 'independent'})
    sampler.seed(0, n)
    center = mi.Point3f(center)
    o = center + dist * size * mi.warp.square_to_uniform_sphere(sampler.next_2d())
    r = dr.cbrt(sampler.next_1d()) * 0.8 * size
    target = center + r * mi.warp.square_to_uniform_sphere(sampler.next_2d())
    ray = mi.Ray3f(o, dr.normalize(target - o))
    si = scene.ray_intersect(ray)
    front = dr.dot(si.n, ray.d) < 0
    si.n = dr.select(front, si.n, -si.n)
    # Instanced hits report the nested shape, so exclude the receiver instead
    active = si.is_valid()
    receiver = shape_by_id(scene, 'receiver')
    if receiver is not None:
        active &= si.shape != mi.ShapePtr(receiver)
    if front_only:
        active &= front
    return si, active, sampler


def check_linearcurve(shape, scale):
    """At small scales, Embree returns the curve parameter of a joint sphere
    for hits on the constant-radius segments of a linear curve. The normals
    then tilt along the axis by up to 0.17, and rays spawned around them
    enter the tube. Mitsuba's own intersector on Metal is unaffected."""
    embree = mi.MI_ENABLE_EMBREE and mi.variant().startswith(('scalar', 'llvm'))
    if shape == 'linearcurve' and scale < 1e-2 and embree:
        pytest.xfail('Embree misreports the curve parameter at small scales')


def error_limit(shape, center, size, dist=0.0):
    """Upper limit for the reported bound: a small multiple of the roundoff
    of the coordinate magnitudes that enter the hit point. The SDF grid
    additionally stops its root finder at a fixed distance."""
    reach = dr.sum(dr.abs(mi.ScalarPoint3f(center))) + size
    if shape in RAY_BASED:
        reach += dist * size
    limit = 12 * mi.math.PositionEpsilon * reach
    if shape == 'sdfgrid':
        limit += 1e-5
    return limit


@pytest.mark.parametrize('shape', SHAPES)
@pytest.mark.parametrize('offset, scale, dist', [(*c, 4.0) for c in CONFIGS] + [(0.0, 1.0, 1e4)])
def test01_spawn_ray_no_self_hit(variants_vec_backends_once, shape, offset, scale, dist, tmp_path):
    """Rays spawned into the hemisphere around the normal do not hit the
    surface they start on. The reported error bound is positive and stays
    within a small multiple of the roundoff of the quantities involved."""
    # The last configuration places the ray origins far away from a shape
    # near the world origin, which catches bounds that ignore the ray extent
    check_linearcurve(shape, scale)
    scene, center, size = make_scene(shape, offset, scale, tmp_path)
    si, active, sampler = hits(scene, center, size, 1 << 14, dist,
                               front_only=shape in OPEN)
    assert dr.count(active)[0] > 1000

    limit = error_limit(shape, center, size, dist)
    assert dr.all(~active | ((si.p_err > 0) & (si.p_err < limit)))

    d = mi.Frame3f(si.n).to_world(mi.warp.square_to_uniform_hemisphere(sampler.next_2d()))
    si2 = scene.ray_intersect(si.spawn_ray(d), active=active)
    assert dr.none(active & si2.is_valid() & (si2.t < 1e-3 * size))


@pytest.mark.parametrize('shape', SHAPES)
@pytest.mark.parametrize('receiver', ['rectangle', 'same'])
@pytest.mark.parametrize('offset, scale', CONFIGS)
def test02_spawn_ray_to_no_false_occlusion(variants_vec_backends_once, shape,
                                           receiver, offset, scale, tmp_path):
    """Shadow rays from hits on the shape to sampled positions on a receiver
    above it are unoccluded. The receiver is a rectangle or a copy of the
    shape, which covers the bound reported by every ``sample_position()``."""
    if receiver == 'same':
        if shape not in SAMPLED:
            pytest.skip('Shape does not implement sample_position()')
        receiver = shape
    check_linearcurve(shape, scale)
    scene, center, size = make_scene(shape, offset, scale, tmp_path, receiver)
    si, active, sampler = hits(scene, center, size, 1 << 14, front_only=shape in OPEN)

    target = shape_by_id(scene, 'receiver')
    ps = target.sample_position(mi.Float(0.0), sampler.next_2d())
    rsize = 2 * size if receiver == 'rectangle' else size
    limit = error_limit(receiver, center + [0, 0, 3 * size], rsize)
    assert dr.all((ps.p_err > 0) & (ps.p_err < limit))

    # Connect hits to positions that are visible from them: the hit faces
    # the target, and the target faces the hit unless it is two-sided
    d = dr.normalize(ps.p - si.p)
    active &= dr.dot(si.n, d) > 0.1
    if receiver not in PLANAR:
        active &= dr.dot(ps.n, d) < -0.1
    assert dr.count(active)[0] > 300
    assert dr.none(active & scene.ray_test(si.spawn_ray_to(ps), active=active))

    # The bare-point overload only offsets the origin and shortens the segment
    assert dr.none(active & scene.ray_test(si.spawn_ray_to(ps.p), active=active))

