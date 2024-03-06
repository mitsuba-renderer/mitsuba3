import pytest
import drjit as dr
import mitsuba as mi

def test01_interaction_invalid_init(variants_all_backends_once):
    si = dr.zeros(mi.SurfaceInteraction3f)
    assert dr.none(si.is_valid())

    si = dr.zeros(mi.SurfaceInteraction3f, 4)
    assert dr.none(si.is_valid())


def test02_intersection_construction(variant_scalar_rgb):
    si = dr.zeros(mi.SurfaceInteraction3f)
    assert not si.is_valid()

    si.shape = None
    si.t = 1
    si.time = 2
    si.wavelengths = []
    si.p = [1, 2, 3]
    si.n = [4, 5, 6]
    si.uv = [7, 8]
    si.sh_frame = mi.Frame3f(
        [9, 10, 11],
        [12, 13, 14],
        [15, 16, 17]
    )
    si.dp_du = [18, 19, 20]
    si.dp_dv = [21, 22, 23]
    si.dn_du = [18, 19, 20]
    si.dn_dv = [21, 22, 23]
    si.duv_dx = [24, 25]
    si.duv_dy = [26, 27]
    si.wi = [31, 32, 33]
    si.prim_index = 34
    si.instance = None
    assert si.sh_frame == mi.Frame3f([9, 10, 11], [12, 13, 14], [15, 16, 17])

    assert repr(si).strip() == """SurfaceInteraction[
  t=1,
  time=2,
  wavelengths=[],
  p=[1, 2, 3],
  n=[4, 5, 6],
  shape=0x0,
  uv=[7, 8],
  sh_frame=Frame[
             s=[9, 10, 11],
             t=[12, 13, 14],
             n=[15, 16, 17]
],
  dp_du=[18, 19, 20],
  dp_dv=[21, 22, 23],
  dn_du=[18, 19, 20],
  dn_dv=[21, 22, 23],
  duv_dx=[24, 25],
  duv_dy=[26, 27],
  wi=[31, 32, 33],
  prim_index=34,
  instance=0x0
]"""


def test03_intersection_partials(variant_scalar_rgb):
    # Test the texture partial computation with some random data

    o = [0.44650541, 0.16336525, 0.74225088]
    d = [0.2956123, 0.67325977, 0.67774232]
    time = 0.5
    w = []
    r = mi.RayDifferential3f(o, d, time, w)
    r.o_x = r.o + [0.1, 0, 0]
    r.o_y = r.o + [0, 0.1, 0]
    r.d_x = r.d
    r.d_y = r.d
    r.has_differentials = True

    si = mi.SurfaceInteraction3f()
    si.p = r(10)
    si.dp_du = [0.5514372, 0.84608955, 0.41559092]
    si.dp_dv = [0.14551054, 0.54917541, 0.39286475]
    si.n = dr.cross(si.dp_du, si.dp_dv)
    si.n /= dr.norm(si.n)
    si.t = 0

    si.compute_uv_partials(r)

    # Positions reached via computed partials
    px1 = si.dp_du * si.duv_dx[0] + si.dp_dv * si.duv_dx[1]
    py1 = si.dp_du * si.duv_dy[0] + si.dp_dv * si.duv_dy[1]

    # Manually
    px2 = r.o_x + r.d_x * \
        ((dr.dot(si.n, si.p) - dr.dot(si.n, r.o_x)) / dr.dot(si.n, r.d_x))
    py2 = r.o_y + r.d_y * \
        ((dr.dot(si.n, si.p) - dr.dot(si.n, r.o_y)) / dr.dot(si.n, r.d_y))
    px2 -= si.p
    py2 -= si.p

    assert dr.allclose(px1, px2, atol=1e-6)
    assert dr.allclose(py1, py2, atol=1e-6)

    si.dp_du = [0, 0, 0]
    si.compute_uv_partials(r)

    assert dr.allclose(px1, px2, atol=1e-6)
    assert dr.allclose(py1, py2, atol=1e-6)

    si.compute_uv_partials(r)

    assert dr.allclose(si.duv_dx, [0, 0])
    assert dr.allclose(si.duv_dy, [0, 0])

def test04_mueller_to_world_to_local(variant_scalar_mono_polarized):
    """
    At a few places, coordinate changes between local BSDF reference frame and
    world coordinates need to take place. This change also needs to be applied
    to Mueller matrices used in computations involving polarization state.

    In practice, this is always a simple rotation of reference Stokes vectors
    (for incident & outgoing directions) of the Mueller matrix.

    To test this behavior we take any Mueller matrix (e.g. linear polarizer)
    for some arbitrary incident/outgoing directions in world coordinates and
    compute the round trip going to local frame and back again.
    """
    si = mi.SurfaceInteraction3f()
    si.sh_frame = mi.Frame3f(dr.normalize(mi.Vector3f(1.0, 1.0, 1.0)))

    M = mi.mueller.linear_polarizer(mi.UnpolarizedSpectrum(1.0))

    # Random incident and outgoing directions
    wi_world = dr.normalize(mi.Vector3f(0.2, 0.0, 1.0))
    wo_world = dr.normalize(mi.Vector3f(0.0, -0.8, 1.0))

    wi_local = si.to_local(wi_world)
    wo_local = si.to_local(wo_world)

    M_local = si.to_local_mueller(M, wi_world, wo_world)
    M_world = si.to_world_mueller(M_local, wi_local, wo_local)

    assert dr.allclose(M, M_world, atol=1e-5)

def test05_gather_interaction(variants_any_llvm):
    from mitsuba import ScalarTransform4f as T
    scene = mi.load_dict({
        "type" : "scene",
        'sphere': { 'type' : 'sphere' },
        'integrator': { 'type': 'path' },
        'mysensor': {
            'type': 'perspective',
            'to_world': T().look_at(origin=[0, 0, 3], target=[0, 0, 0], up=[0, 1, 0]),
            'myfilm': {'type': 'hdrfilm'},
        },
    })
    sensor = scene.sensors()[0]
    ray, w = sensor.sample_ray(0.0, 0.0, mi.Point2f([0.1, 0.2]), mi.Point2f([0.1, 0.2]))
    si = scene.ray_intersect(ray)
    
    si_ = dr.gather(mi.SurfaceInteraction3f, si, mi.UInt32([0, 2]))

    assert(dr.width(si_.shape) == 2)
    assert(dr.allclose(si_.t[0], si.t[0]))
    assert(dr.allclose(si_.t[1], si.t[2]))
