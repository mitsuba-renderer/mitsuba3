import mitsuba
import pytest

from mitsuba.python.test.util import fresolver_append_path


@fresolver_append_path
@pytest.mark.parametrize('filter_type', ['nearest', 'bilinear'])
@pytest.mark.parametrize('wrap_mode', ['repeat', 'clamp', 'mirror'])
def test01_sample_position(variants_vec_backends_once, filter_type, wrap_mode):
    from mitsuba.core import load_string, ScalarBoundingBox2f
    from mitsuba.python.chi2 import ChiSquareTest, PlanarDomain

    bitmap = load_string("""
    <texture type="bitmap" version="2.0.0">
        <string name="filename" value="resources/data/common/textures/carrot.png"/>
        <string name="filter_type" value="%s"/>
        <string name="wrap_mode" value="%s"/>
    </texture>""" % (filter_type, wrap_mode)).expand()[0]

    chi2 = ChiSquareTest(
        domain=PlanarDomain(ScalarBoundingBox2f([0, 0], [1, 1])),
        sample_func=lambda s: bitmap.sample_position(s)[0],
        pdf_func=lambda p: bitmap.pdf_position(p),
        sample_dim=2,
        res=201,
        ires=8
    )

    assert chi2.run()


@fresolver_append_path
def test02_eval_grad(variant_scalar_rgb, np_rng):
    # Tests evaluating the texture gradient under different rotation
    from mitsuba.core import load_string, Vector2f
    from mitsuba.render import SurfaceInteraction3f
    import numpy as np
    import enoki as ek
    delta = 1e-4
    si = SurfaceInteraction3f()
    for u01 in np_rng.random((10, 1)):
        angle = 360.0*u01[0]
        bitmap = load_string("""
        <texture type="bitmap" version="2.0.0">
            <string name="filename" value="resources/data/common/textures/noise_8x8.png"/>
            <transform name="to_uv">
                <rotate angle="%f"/>
            </transform>
        </texture>""" % angle).expand()[0]
        for uv in np_rng.random((10, 2)):
            si.uv = Vector2f(uv)
            f = bitmap.eval_1(si)
            si.uv = uv + Vector2f(delta, 0)
            fu = bitmap.eval_1(si)
            si.uv = uv + Vector2f(0, delta)
            fv = bitmap.eval_1(si)
            gradient_finite_difference = Vector2f((fu - f)/delta, (fv - f)/delta)
            gradient_analytic = bitmap.eval_1_grad(si)
            assert ek.allclose(0, ek.abs(gradient_finite_difference/gradient_analytic - 1.0), atol = 1e04)


@fresolver_append_path
@pytest.mark.parametrize('wrap_mode', ['repeat', 'clamp', 'mirror'])
def test03_wrap(variants_vec_backends_once_rgb, wrap_mode):
    from mitsuba.core import Float, load_string
    from mitsuba.render import SurfaceInteraction3f
    import numpy as np
    import enoki as ek

    bitmap = load_string("""
    <texture type="bitmap" version="2.0.0">
        <string name="filename" value="resources/data/common/textures/noise_8x8.png"/>
        <string name="wrap_mode" value="%s"/>
    </texture>""" % (wrap_mode)).expand()[0]

    def eval_ranges(range_x, range_y):
        xv, yv = ek.meshgrid(range_x, range_y)

        siv = SurfaceInteraction3f()
        siv.uv = [xv, yv]

        return bitmap.eval_3(siv)

    axis_res = 20

    x = ek.linspace(Float, 0, 1, axis_res)
    y = ek.linspace(Float, 0, 1, axis_res)
    ref = eval_ranges(x, y)

    if wrap_mode == 'repeat':
        # Top left
        x = ek.linspace(Float, -1, 0, axis_res)
        y = ek.linspace(Float, -1, 0, axis_res)
        assert ek.allclose(0, ref - eval_ranges(x, y), atol=1e-04)

        # Bottom right
        x = ek.linspace(Float, 1, 2, axis_res)
        y = ek.linspace(Float, 1, 2, axis_res)
        assert ek.allclose(0, ref - eval_ranges(x, y), atol=1e-04)

    elif wrap_mode == 'clamp':
        # Top
        x = ek.linspace(Float, 0, 1, axis_res)
        y = ek.linspace(Float, -1, 0, axis_res)
        top = np.array(ref)[:axis_res]
        assert ek.allclose(0, top - np.reshape(
            eval_ranges(x, y), (axis_res, axis_res, 3)), atol=1e-04)

        # Bottom
        x = ek.linspace(Float, 0, 1, axis_res)
        y = ek.linspace(Float, 1, 2, axis_res)
        bottom = np.array(ref)[-axis_res:]
        assert ek.allclose(0, bottom - np.reshape(
            eval_ranges(x, y), (axis_res, axis_res, 3)), atol=1e-04)

        # Left
        x = ek.linspace(Float, -1, 0, axis_res)
        y = ek.linspace(Float, 0, 1, axis_res)
        left = np.array(ref)[::axis_res]
        assert ek.allclose(0, np.repeat(left, axis_res, axis=0) -
            eval_ranges(x, y), atol=1e-04)

        # Right
        x = ek.linspace(Float, 1, 2, axis_res)
        y = ek.linspace(Float, 0, 1, axis_res)
        right = np.array(ref)[axis_res - 1::axis_res]
        assert ek.allclose(0, np.repeat(right, axis_res, axis=0) -
            eval_ranges(x, y), atol=1e-04)

    elif wrap_mode == 'mirror':
        # Top left
        x = ek.linspace(Float, -1, 0, axis_res)[::-1]
        y = ek.linspace(Float, -1, 0, axis_res)[::-1]
        assert ek.allclose(0, ref - eval_ranges(x, y), atol=1e-04)

        # Bottom right
        x = ek.linspace(Float, 1, 2, axis_res)[::-1]
        y = ek.linspace(Float, 1, 2, axis_res)[::-1]
        assert ek.allclose(0, ref - eval_ranges(x, y), atol=1e-04)
