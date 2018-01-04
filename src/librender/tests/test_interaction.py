import mitsuba
import numpy as np

from mitsuba.render import SurfaceInteraction3f
from mitsuba.core   import Frame3f, Ray3f, RayDifferential3f

def test01_intersection_construction():
    si = SurfaceInteraction3f()
    si.shape = None
    si.t = 1
    si.time = 2
    si.wavelengths = [200, 300, 400, 500]
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
    si.duv_dx = [24, 25]
    si.duv_dy = [26, 27]
    si.wi = [31, 32, 33]
    si.prim_index = 34
    si.instance = None
    si.has_uv_partials = False
    assert repr(si) == """SurfaceInteraction[
  t = 1,
  time = 2,
  wavelengths = [200, 300, 400, 500],
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
  wi = [31, 32, 33],
  prim_index = 34,
  instance = nullptr,
  has_uv_partials = 0,
]"""


def test02_intersection_partials():
    # Test the texture partial computation with some random data

    o = np.array([0.44650541, 0.16336525, 0.74225088])
    d = np.array([0.2956123, 0.67325977, 0.67774232])
    time = 0.5
    w = np.array([500, 600, 750, 800])
    r = RayDifferential3f(o, d, time, w)
    r.o_x = r.o + np.array([0.1, 0, 0])
    r.o_y = r.o + np.array([0, 0.1, 0])
    r.d_x = r.d
    r.d_y = r.d
    r.has_differentials = True

    si = SurfaceInteraction3f()
    si.p = r(10)
    si.dp_du = np.array([0.5514372, 0.84608955, 0.41559092])
    si.dp_dv = np.array([0.14551054, 0.54917541, 0.39286475])
    si.n = np.cross(si.dp_du, si.dp_dv)
    si.n /= np.linalg.norm(si.n)
    si.t = 0

    si.compute_partials(r)

    # Positions reached via computed partials
    px1 = si.dp_du * si.duv_dx[0] + si.dp_dv * si.duv_dx[1]
    py1 = si.dp_du * si.duv_dy[0] + si.dp_dv * si.duv_dy[1]

    # Manually
    px2 = r.o_x + r.d_x * \
        ((np.dot(si.n, si.p) - np.dot(si.n, r.o_x)) / np.dot(si.n, r.d_x))
    py2 = r.o_y + r.d_y * \
        ((np.dot(si.n, si.p) - np.dot(si.n, r.o_y)) / np.dot(si.n, r.d_y))
    px2 -= si.p
    py2 -= si.p

    assert(np.allclose(px1, px2))
    assert(np.allclose(py1, py2))

    si.dp_du = np.array([0, 0, 0])
    si.compute_partials(r)

    assert(np.allclose(px1, px2))
    assert(np.allclose(py1, py2))

    si.has_uv_partials = False
    si.compute_partials(r)

    assert(np.allclose(si.duv_dx, [0, 0]))
    assert(np.allclose(si.duv_dy, [0, 0]))
