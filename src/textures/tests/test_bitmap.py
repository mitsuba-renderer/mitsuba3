import mitsuba
import pytest

from mitsuba.scalar_rgb.test.util import fresolver_append_path


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
    </texture>""" % (filter_type, wrap_mode))

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
    import drjit as dr
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
        </texture>""" % angle)
        for uv in np_rng.random((10, 2)):
            si.uv = Vector2f(uv)
            f = bitmap.eval_1(si)
            si.uv = uv + Vector2f(delta, 0)
            fu = bitmap.eval_1(si)
            si.uv = uv + Vector2f(0, delta)
            fv = bitmap.eval_1(si)
            gradient_finite_difference = Vector2f((fu - f)/delta, (fv - f)/delta)
            gradient_analytic = bitmap.eval_1_grad(si)
            assert dr.allclose(0, dr.abs(gradient_finite_difference/gradient_analytic - 1.0), atol = 1e04)


@fresolver_append_path
@pytest.mark.parametrize('wrap_mode', ['repeat', 'clamp', 'mirror'])
def test03_wrap(variants_vec_backends_once_rgb, wrap_mode):
    from mitsuba.core import Float, load_string
    from mitsuba.render import SurfaceInteraction3f
    import numpy as np
    import drjit as dr

    bitmap = load_string("""
    <texture type="bitmap" version="2.0.0">
        <string name="filename" value="resources/data/common/textures/noise_8x8.png"/>
        <string name="wrap_mode" value="%s"/>
    </texture>""" % (wrap_mode))

    def eval_ranges(range_x, range_y):
        xv, yv = dr.meshgrid(range_x, range_y)

        si = dr.zero(SurfaceInteraction3f)
        si.uv = [xv, yv]

        return bitmap.eval_3(si)

    axis_res = 20

    x = dr.linspace(Float, 0, 1, axis_res)
    y = dr.linspace(Float, 0, 1, axis_res)
    ref = eval_ranges(x, y)

    if wrap_mode == 'repeat':
        # Top left
        x = dr.linspace(Float, -1, 0, axis_res)
        y = dr.linspace(Float, -1, 0, axis_res)
        assert dr.allclose(0, ref - eval_ranges(x, y), atol=1e-04)

        # Bottom right
        x = dr.linspace(Float, 1, 2, axis_res)
        y = dr.linspace(Float, 1, 2, axis_res)
        assert dr.allclose(0, ref - eval_ranges(x, y), atol=1e-04)

    elif wrap_mode == 'clamp':
        # Top
        x = dr.linspace(Float, 0, 1, axis_res)
        y = dr.linspace(Float, -1, 0, axis_res)
        top = np.array(ref)[:axis_res]
        assert dr.allclose(0, top - np.reshape(
            eval_ranges(x, y), (axis_res, axis_res, 3)), atol=1e-04)

        # Bottom
        x = dr.linspace(Float, 0, 1, axis_res)
        y = dr.linspace(Float, 1, 2, axis_res)
        bottom = np.array(ref)[-axis_res:]
        assert dr.allclose(0, bottom - np.reshape(
            eval_ranges(x, y), (axis_res, axis_res, 3)), atol=1e-04)

        # Left
        x = dr.linspace(Float, -1, 0, axis_res)
        y = dr.linspace(Float, 0, 1, axis_res)
        left = np.array(ref)[::axis_res]
        assert dr.allclose(0, np.repeat(left, axis_res, axis=0) -
            eval_ranges(x, y), atol=1e-04)

        # Right
        x = dr.linspace(Float, 1, 2, axis_res)
        y = dr.linspace(Float, 0, 1, axis_res)
        right = np.array(ref)[axis_res - 1::axis_res]
        assert dr.allclose(0, np.repeat(right, axis_res, axis=0) -
            eval_ranges(x, y), atol=1e-04)

    elif wrap_mode == 'mirror':
        # Top left
        x = dr.linspace(Float, -1, 0, axis_res)[::-1]
        y = dr.linspace(Float, -1, 0, axis_res)[::-1]
        assert dr.allclose(0, ref - eval_ranges(x, y), atol=1e-04)

        # Bottom right
        x = dr.linspace(Float, 1, 2, axis_res)[::-1]
        y = dr.linspace(Float, 1, 2, axis_res)[::-1]
        assert dr.allclose(0, ref - eval_ranges(x, y), atol=1e-04)


@fresolver_append_path
def test04_eval_rgb(variants_vec_backends_once_rgb):
    from mitsuba.core import Float, load_string, luminance
    from mitsuba.render import SurfaceInteraction3f
    import numpy as np
    import drjit as dr

    # RGB image
    bitmap = load_string("""
    <texture type="bitmap" version="2.0.0">
        <string name="filename" value="resources/data/common/textures/carrot.png"/>
    </texture>""")

    x_res, y_res =  bitmap.resolution()
    # Coordinates of green pixel: (7, 1)
    x = (1 / x_res) * 7 + (1 / (2 * x_res))
    y = (1 / y_res) * 1 + (1 / (2 * y_res))

    si = dr.zero(SurfaceInteraction3f)
    si.uv = [x, y]

    mono = bitmap.eval_1(si)
    color = bitmap.eval_3(si)
    spec = bitmap.eval(si)

    expected = [0.0012, 0.1946, 0.0241]
    assert dr.allclose(0, color - spec)
    assert dr.allclose(expected, color, atol=1e-04)
    assert dr.allclose(luminance(expected), mono, atol=1e-04)

    # Grayscale image
    bitmap = load_string("""
    <texture type="bitmap" version="2.0.0">
        <string name="filename" value="resources/data/common/textures/noise_02.png"/>
    </texture>""")

    x_res, y_res =  bitmap.resolution()
    # Coordinates of gray pixel: (0, 15)
    x = (1 / x_res) * 0 + (1 / (2 * x_res))
    y = (1 / y_res) * 15 + (1 / (2 * y_res))

    si = dr.zero(SurfaceInteraction3f)
    si.uv = [x, y]

    mono = bitmap.eval_1(si)
    with pytest.raises(RuntimeError):
        color = bitmap.eval_3(si)
    spec = bitmap.eval(si)

    expected = 0.5394
    assert dr.allclose(expected, spec, atol=1e-04)
    assert dr.allclose(expected, mono, atol=1e-04)

@fresolver_append_path
def test05_eval_spectral(variants_vec_backends_once_spectral):
    from mitsuba.core import (Float, load_string, luminance, MTS_CIE_MIN,
            MTS_CIE_MAX, MTS_WAVELENGTH_SAMPLES)
    from mitsuba.render import SurfaceInteraction3f, srgb_model_eval
    import numpy as np
    import drjit as dr

    # RGB image
    bitmap = load_string("""
    <texture type="bitmap" version="2.0.0">
        <string name="filename" value="resources/data/common/textures/carrot.png"/>
    </texture>""")

    x_res, y_res =  bitmap.resolution()
    # Coordinates of green pixel: (7, 1)
    x = (1 / x_res) * 7 + (1 / (2 * x_res))
    y = (1 / y_res) * 1 + (1 / (2 * y_res))

    si = dr.zero(SurfaceInteraction3f)
    si.wavelengths = np.linspace(MTS_CIE_MIN, MTS_CIE_MAX, MTS_WAVELENGTH_SAMPLES)
    si.uv = [x, y]

    with pytest.raises(RuntimeError):
        mono = bitmap.eval_1(si)
    with pytest.raises(RuntimeError):
        color = bitmap.eval_3(si)
    spec = bitmap.eval(si)

    expected = [0.0023, 0.1910, 0.0059, 0.0003]
    assert dr.allclose(expected, spec, atol=1e-04)

    # Grayscale image
    bitmap = load_string("""
    <texture type="bitmap" version="2.0.0">
        <string name="filename" value="resources/data/common/textures/noise_02.png"/>
    </texture>""")

    x_res, y_res =  bitmap.resolution()
    # Coordinates of gray pixel: (0, 15)
    x = (1 / x_res) * 0 + (1 / (2 * x_res))
    y = (1 / y_res) * 15 + (1 / (2 * y_res))

    si = dr.zero(SurfaceInteraction3f)
    si.uv = [x, y]

    mono = bitmap.eval_1(si)
    with pytest.raises(RuntimeError):
        color = bitmap.eval_3(si)
    spec = bitmap.eval(si)

    expected = 0.5394
    assert dr.allclose(expected, spec, atol=1e-04)
    assert dr.allclose(expected, mono, atol=1e-04)
