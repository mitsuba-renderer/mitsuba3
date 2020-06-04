import mitsuba
import pytest
import enoki as ek

from .mesh_generation import create_stairs

from mitsuba.python.test.util import fresolver_append_path


def make_synthetic_scene(n_steps):
    from mitsuba.core import Properties
    from mitsuba.render import Scene

    props = Properties("scene")
    props["_unnamed_0"] = create_stairs(n_steps)
    return Scene(props)


def compare_results(res_a, res_b, atol=0.0):
    assert ek.all(res_a.is_valid() == res_b.is_valid())
    if ek.any(res_a.is_valid()):
        assert ek.allclose(res_a.t, res_b.t, atol=atol), "\n%s\n\n%s" % (res_a.t, res_b.t)

# ------------------------------------------------------------------------------

def test01_depth_scalar_stairs(variant_scalar_rgb):
    from mitsuba.core import Ray3f
    from mitsuba.render import SurfaceInteraction3f

    if mitsuba.core.MTS_ENABLE_EMBREE:
        pytest.skip("EMBREE enabled")

    n_steps = 20
    scene = make_synthetic_scene(n_steps)

    n = 128
    inv_n = 1.0 / (n-1)
    wavelengths = []

    for x in range(n - 1):
        for y in range(n - 1):
            o = [x * inv_n,  y * inv_n,  2]
            d = [0,  0,  -1]
            r = Ray3f(o, d, 0.5, wavelengths)
            r.mint = 0
            r.maxt = 100

            res_naive   = scene.ray_intersect_naive(r)
            res         = scene.ray_intersect(r)
            res_shadow  = scene.ray_test(r)

            step_idx = ek.floor((y * inv_n) * n_steps)

            assert ek.all(res_shadow)
            assert ek.all(res_shadow == res_naive.is_valid())
            expected = SurfaceInteraction3f()
            expected.t = 2.0 - (step_idx / n_steps)
            compare_results(res_naive, expected, atol=1e-9)
            compare_results(res_naive, res)


@fresolver_append_path
def test02_depth_scalar_bunny(variant_scalar_rgb):
    from mitsuba.core import Ray3f, BoundingBox3f
    from mitsuba.core.xml import load_string
    from mitsuba.render import SurfaceInteraction3f

    if mitsuba.core.MTS_ENABLE_EMBREE:
        pytest.skip("EMBREE enabled")

    scene = load_string("""
        <scene version="0.5.0">
            <shape type="ply">
                <string name="filename" value="resources/data/common/meshes/bunny_lowres.ply"/>
            </shape>
        </scene>
    """)
    b = scene.bbox()

    n = 100
    inv_n = 1.0 / (n - 1)
    wavelengths = []

    for x in range(n):
        for y in range(n):
            o = [b.min[0] * (1 - x * inv_n) + b.max[0] * x * inv_n,
                 b.min[1] * (1 - y * inv_n) + b.max[1] * y * inv_n,
                 b.min[2]]
            d = [0, 0, 1]
            r = Ray3f(o, d, 0.5, wavelengths)
            r.mint = 0
            r.maxt = 100

            res_naive  = scene.ray_intersect_naive(r)
            res        = scene.ray_intersect(r)
            res_shadow = scene.ray_test(r)
            assert ek.all(res_shadow == res_naive.is_valid())
            compare_results(res_naive, res)


def test03_depth_packet_stairs(variant_packet_rgb):
    from mitsuba.core import Ray3f as Ray3fX, Properties
    from mitsuba.render import Scene

    if mitsuba.core.MTS_ENABLE_EMBREE:
        pytest.skip("EMBREE enabled")

    props = Properties("scene")
    props["_unnamed_0"] = create_stairs(11)
    scene = Scene(props)

    mitsuba.set_variant("scalar_rgb")
    from mitsuba.core import Ray3f, Vector3f

    n = 4
    inv_n = 1.0 / (n - 1)
    rays = Ray3fX.zero(n * n)
    d = [0, 0, -1]
    wavelengths = []

    for x in range(n):
        for y in range(n):
            o = Vector3f(x * inv_n, y * inv_n, 2)
            o = o * 0.999 + 0.0005
            rays[x * n + y] = Ray3f(o, d, 0, 100, 0.5, wavelengths)

    res_naive  = scene.ray_intersect_naive(rays)
    res        = scene.ray_intersect(rays)
    res_shadow = scene.ray_test(rays)

    # TODO: spot-check (here, we only check consistency)
    assert ek.all(res_shadow == res.is_valid())
    compare_results(res_naive, res, atol=1e-6)
