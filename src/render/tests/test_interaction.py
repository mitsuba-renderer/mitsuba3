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
    si.wi = [31, 32, 33]
    si.prim_index = 34
    si.instance_index = 0
    assert si.sh_frame == mi.Frame3f([9, 10, 11], [12, 13, 14], [15, 16, 17])

    assert repr(si).strip() == """SurfaceInteraction[
  t=1,
  time=2,
  wavelengths=[],
  p=[1, 2, 3],
  n=[4, 5, 6],
  p_err=0,
  shape=0x0,
  uv=[7, 8],
  sh_frame=Frame[
             s=[9, 10, 11],
             t=[12, 13, 14],
             n=[15, 16, 17]
],
  frame_flipped=0,
  dp_du=[18, 19, 20],
  dp_dv=[21, 22, 23],
  dn_du=[18, 19, 20],
  dn_dv=[21, 22, 23],
  wi=[31, 32, 33],
  prim_index=34,
  instance_index=0
]"""


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
