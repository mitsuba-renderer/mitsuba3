import mitsuba
import pytest
import enoki as ek


def test01_intersection_construction(variant_scalar_rgb):
    from mitsuba.core import Frame3f
    from mitsuba.render import SurfaceInteraction3f

    si = SurfaceInteraction3f()
    si.shape = None
    si.t = 1
    si.time = 2
    si.wavelengths = []
    si.p = [1, 2, 3]
    si.n = [4, 5, 6]
    si.uv = [7, 8]
    si.sh_frame = Frame3f(
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
    assert si.sh_frame == Frame3f([9, 10, 11], [12, 13, 14], [15, 16, 17])
    assert repr(si) == """SurfaceInteraction[
  t = 1,
  time = 2,
  wavelengths = [],
  p = [1, 2, 3],
  shape = nullptr,
  uv = [7, 8],
  n = [4, 5, 6],
  sh_frame = Frame[
    s = [9, 10, 11],
    t = [12, 13, 14],
    n = [15, 16, 17]
  ],
  dp_du = [18, 19, 20],
  dp_dv = [21, 22, 23],
  dn_du = [18, 19, 20],
  dn_dv = [21, 22, 23],
  duv_dx = [24, 25],
  duv_dy = [26, 27],
  wi = [31, 32, 33],
  prim_index = 34,
  instance = nullptr
]"""


def test02_intersection_partials(variant_scalar_rgb):
    from mitsuba.core import Frame3f, Ray3f, RayDifferential3f
    from mitsuba.render import SurfaceInteraction3f

    # Test the texture partial computation with some random data

    o = [0.44650541, 0.16336525, 0.74225088]
    d = [0.2956123, 0.67325977, 0.67774232]
    time = 0.5
    w = []
    r = RayDifferential3f(o, d, time, w)
    r.o_x = r.o + [0.1, 0, 0]
    r.o_y = r.o + [0, 0.1, 0]
    r.d_x = r.d
    r.d_y = r.d
    r.has_differentials = True

    si = SurfaceInteraction3f()
    si.p = r(10)
    si.dp_du = [0.5514372, 0.84608955, 0.41559092]
    si.dp_dv = [0.14551054, 0.54917541, 0.39286475]
    si.n = ek.cross(si.dp_du, si.dp_dv)
    si.n /= ek.norm(si.n)
    si.t = 0

    si.compute_uv_partials(r)

    # Positions reached via computed partials
    px1 = si.dp_du * si.duv_dx[0] + si.dp_dv * si.duv_dx[1]
    py1 = si.dp_du * si.duv_dy[0] + si.dp_dv * si.duv_dy[1]

    # Manually
    px2 = r.o_x + r.d_x * \
        ((ek.dot(si.n, si.p) - ek.dot(si.n, r.o_x)) / ek.dot(si.n, r.d_x))
    py2 = r.o_y + r.d_y * \
        ((ek.dot(si.n, si.p) - ek.dot(si.n, r.o_y)) / ek.dot(si.n, r.d_y))
    px2 -= si.p
    py2 -= si.p

    assert(ek.allclose(px1, px2))
    assert(ek.allclose(py1, py2))

    si.dp_du = [0, 0, 0]
    si.compute_uv_partials(r)

    assert(ek.allclose(px1, px2))
    assert(ek.allclose(py1, py2))

    si.compute_uv_partials(r)

    assert(ek.allclose(si.duv_dx, [0, 0]))
    assert(ek.allclose(si.duv_dy, [0, 0]))

def test03_mueller_to_world_to_local(variant_scalar_mono_polarized):
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
    from mitsuba.core import Frame3f, UnpolarizedSpectrum
    from mitsuba.render import SurfaceInteraction3f
    from mitsuba.render.mueller import linear_polarizer
    import numpy as np

    si = SurfaceInteraction3f()
    si.sh_frame = Frame3f(ek.normalize([1.0, 1.0, 1.0]))

    M = linear_polarizer(UnpolarizedSpectrum(1.0))

    # Random incident and outgoing directions
    wi_world = ek.normalize([0.2, 0.0, 1.0])
    wo_world = ek.normalize([0.0, -0.8, 1.0])

    wi_local = si.to_local(wi_world)
    wo_local = si.to_local(wo_world)

    M_local = si.to_local_mueller(M, wi_world, wo_world)
    M_world = si.to_world_mueller(M_local, wi_local, wo_local)

    assert ek.allclose(M, M_world, atol=1e-5)
