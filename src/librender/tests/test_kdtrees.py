from mitsuba.scalar_rgb.core import Properties, Ray3f, MTS_WAVELENGTH_SAMPLES
from mitsuba.scalar_rgb.render import ShapeKDTree, Mesh, SurfaceInteraction3f, Scene
from mitsuba.scalar_rgb.core.xml import load_string
from mitsuba.test.util import fresolver_append_path

import math
import numpy as np
import pytest

from .mesh_generation import create_stairs


def make_synthetic_scene(n_steps):
    props = Properties("scene")
    props["_unnamed_0"] = create_stairs(n_steps)
    return Scene(props)


def compare_results(res_a, res_b, atol=0):
    assert np.allclose(res_a.t, res_b.t, atol=atol), "\n%s\n\n%s" % (res_a.t, res_b.t)
    assert np.all(res_a.is_valid() == res_b.is_valid())

# ------------------------------------------------------------------------------

def test01_depth_scalar_stairs():
    n_steps = 20
    scene = make_synthetic_scene(n_steps)

    n = 128
    inv_n = 1.0 / (n-1)
    wavelengths = np.zeros(MTS_WAVELENGTH_SAMPLES)

    for x in range(n - 1):
        for y in range(n - 1):
            o = np.array([x * inv_n,  y * inv_n,  2])
            d = np.array([0,  0,  -1])
            r = Ray3f(o, d, 0.5, wavelengths)
            r.mint = 0
            r.maxt = 100

            res_naive   = scene.ray_intersect_naive(r)
            res         = scene.ray_intersect(r)
            res_shadow  = scene.ray_test(r)

            step_idx = math.floor((y * inv_n) * n_steps)

            assert np.all(res_shadow)
            assert np.all(res_shadow == res_naive.is_valid())
            expected = SurfaceInteraction3f()
            expected.t = 2.0 - (step_idx / n_steps)
            compare_results(res_naive, expected, atol=1e-9)
            compare_results(res_naive, res)


@fresolver_append_path
def test02_depth_scalar_bunny():
    scene = load_string("""
        <scene version="0.5.0">
            <shape type="ply">
                <string name="filename" value="resources/data/ply/bunny_lowres.ply"/>
            </shape>
        </scene>
    """)
    b = scene.kdtree().bbox()

    n = 100
    inv_n = 1.0 / (n - 1)
    wavelengths = np.zeros(MTS_WAVELENGTH_SAMPLES)

    for x in range(n):
        for y in range(n):
            o = np.array([b.min[0] * (1 - x * inv_n) + b.max[0] * x * inv_n,
                          b.min[1] * (1 - y * inv_n) + b.max[1] * y * inv_n,
                          b.min[2]])
            d = np.array([0, 0, 1])
            r = Ray3f(o, d, 0.5, wavelengths)
            r.mint = 0
            r.maxt = 100

            res_naive  = scene.ray_intersect_naive(r)
            res        = scene.ray_intersect(r)
            res_shadow = scene.ray_test(r)
            assert np.all(res_shadow == res_naive.is_valid())
            compare_results(res_naive, res)

def test03_depth_packet_stairs():
    try:
        from mitsuba.packet_rgb.core import Ray3f as Ray3fX
        from mitsuba.packet_rgb.render import Scene as SceneX
    except ImportError:
        pytest.skip("packet_rgb mode not enabled")

    props = Properties("scene")
    props["_unnamed_0"] = create_stairs_packet(11)
    scene = SceneX(props)

    n = 4
    inv_n = 1.0 / (n - 1)
    rays = Ray3fX(n * n)
    d = np.array([0, 0, -1])
    wavelengths = np.zeros(MTS_WAVELENGTH_SAMPLES)

    for x in range(n):
        for y in range(n):
            o = np.array([x * inv_n, y * inv_n, 2])
            o[0:2] = o[0:2] * 0.999 + 0.0005
            rays[x * n + y] = Ray3f(o, d, 0, 100, 0.5, wavelengths)

    res_naive  = scene.ray_intersect_naive(rays)
    res        = scene.ray_intersect(rays)
    res_shadow = scene.ray_test(rays)

    # TODO: spot-check (here, we only check consistency)
    assert np.all(res_shadow == res.is_valid())
    compare_results(res_naive, res, atol=1e-6)
