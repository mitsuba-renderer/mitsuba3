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
    si.footprint = mi.Matrix2f([[24, 25], [26, 27]])
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
  footprint=[[24, 25],
             [26, 27]],
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


def test06_footprint(variants_all_backends_once):
    """
    Checks that ``RayFlags.Footprint`` projects the ray cone onto the hit
    surface with the expected UV-space extent, and that the computation is
    skipped when the flag, the cone, or a filtered texture is absent.
    """
    import math
    import numpy as np
    from mitsuba.scalar_rgb.test.util import find_resource

    # A rectangle spans [-1, 1]^2 with UVs in [0, 1]^2, so a world space
    # length maps to half its value in UV space. The filtered texture keeps
    # the scene from skipping the footprint computation.
    def make_scene(filter_type):
        return mi.load_dict({
            'type': 'scene',
            'rect': {
                'type': 'rectangle',
                'bsdf': {
                    'type': 'diffuse',
                    'reflectance': {
                        'type': 'bitmap',
                        'filename': find_resource('resources/data/common/textures/carrot.png'),
                        'filter_type': filter_type
                    }
                }
            }
        })

    scene = make_scene('trilinear')
    assert scene.has_filtered_textures()

    theta = math.radians(60)
    d = mi.Vector3f(math.sin(theta), 0, -math.cos(theta))
    ray = mi.Ray3f(mi.Point3f(0, 0, 0) - d * 5, d)

    # Cone of width 1/256 at the hit point, which lies 5 units away
    ray.cone = mi.RayCone(0, 1 / (256 * 5))
    si = scene.ray_intersect(ray, mi.RayFlags.Default, coherent=True)

    # The footprint stretches by 1/cos(theta) in the plane of incidence
    # (the u axis) and is unchanged across it. The columns of the footprint
    # matrix span the ellipse, whose axes are its singular values.
    def axes(si):
        m = np.array(si.footprint).reshape(2, 2)
        return np.linalg.svd(m, compute_uv=False)

    assert np.allclose(axes(si), [0.5 / 256 / math.cos(theta), 0.5 / 256], atol=1e-7)

    # The footprint is zero without the flag, and for a ray without a cone
    si = scene.ray_intersect(ray, mi.RayFlags.Shading, coherent=True)
    assert dr.all(si.footprint == 0, axis=None)
    ray.cone = mi.RayCone(0, 0)
    si = scene.ray_intersect(ray, mi.RayFlags.Default, coherent=True)
    assert dr.all(si.footprint == 0, axis=None)

    # Normal incidence yields a circular footprint
    ray = mi.Ray3f(mi.Point3f(0.2, 0.1, 3), mi.Vector3f(0, 0, -1))
    ray.cone = mi.RayCone(0.03, 0)
    si = scene.ray_intersect(ray, mi.RayFlags.Default, coherent=True)
    assert np.allclose(axes(si), [0.015, 0.015], atol=1e-7)

    # A scene without filtered textures skips the footprint computation
    scene = make_scene('bilinear')
    assert not scene.has_filtered_textures()
    si = scene.ray_intersect(ray, mi.RayFlags.Default, coherent=True)
    assert dr.all(si.footprint == 0, axis=None)
