import pytest
import drjit as dr
import mitsuba as mi

from mitsuba.scalar_rgb.test.util import fresolver_append_path


@fresolver_append_path
@pytest.mark.parametrize('filter_type', ['nearest', 'bilinear'])
@pytest.mark.parametrize('wrap_mode', ['repeat', 'clamp', 'mirror'])
def test01_sample_position(variants_vec_backends_once, filter_type, wrap_mode):
    bitmap = mi.load_dict({
        "type" : "bitmap",
        "filename" : "resources/data/common/textures/carrot.png",
        "filter_type" : filter_type,
        "wrap_mode" : wrap_mode
    })

    chi2 = mi.chi2.ChiSquareTest(
        domain=mi.chi2.PlanarDomain(mi.ScalarBoundingBox2f([0, 0], [1, 1])),
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
    import numpy as np
    delta = 1e-4
    si = mi.SurfaceInteraction3f()
    for u01 in np_rng.random((10, 1)):
        angle = 360.0*u01[0]
        bitmap = mi.load_dict({
            "type" : "bitmap",
            "filename" : "resources/data/common/textures/noise_8x8.png",
            "to_uv" : mi.ScalarTransform4f().rotate([0, 0, 1], angle)
        })
        for uv in np_rng.random((10, 2)):
            si.uv = mi.Vector2f(uv)
            f = bitmap.eval_1(si)
            si.uv = uv + mi.Vector2f(delta, 0)
            fu = bitmap.eval_1(si)
            si.uv = uv + mi.Vector2f(0, delta)
            fv = bitmap.eval_1(si)
            gradient_finite_difference = mi.Vector2f((fu - f)/delta, (fv - f)/delta)
            gradient_analytic = bitmap.eval_1_grad(si)
            assert dr.allclose(0, dr.abs(gradient_finite_difference/gradient_analytic - 1.0), atol = 1e04)


@fresolver_append_path
@pytest.mark.parametrize('wrap_mode', ['repeat', 'clamp', 'mirror'])
def test03_wrap(variants_vec_backends_once_rgb, wrap_mode):
    import numpy as np

    bitmap = mi.load_dict({
        "type"      : "bitmap",
        "filename"  : "resources/data/common/textures/noise_8x8.png",
        "wrap_mode" : wrap_mode,
        "format"    : "variant"
    })

    def eval_ranges(range_x, range_y):
        xv, yv = dr.meshgrid(range_x, range_y)

        si = dr.zeros(mi.SurfaceInteraction3f)
        si.uv = [xv, yv]

        return bitmap.eval_3(si)

    axis_res = 20

    x = dr.linspace(mi.Float, 0, 1, axis_res)
    y = dr.linspace(mi.Float, 0, 1, axis_res)
    ref = eval_ranges(x, y)

    if wrap_mode == 'repeat':
        # Top left
        x = dr.linspace(mi.Float, -1, 0, axis_res)
        y = dr.linspace(mi.Float, -1, 0, axis_res)
        assert dr.allclose(0, ref - eval_ranges(x, y), atol=1e-04)

        # Bottom right
        x = dr.linspace(mi.Float, 1, 2, axis_res)
        y = dr.linspace(mi.Float, 1, 2, axis_res)
        assert dr.allclose(0, ref - eval_ranges(x, y), atol=1e-04)

    elif wrap_mode == 'clamp':
        # Top
        x           = dr.linspace(mi.Float, 0, 1, axis_res)
        y           = dr.linspace(mi.Float, -1, 0, axis_res)
        y_expected  = dr.linspace(mi.Float, 0, 0, axis_res)
        assert dr.allclose(eval_ranges(x, y_expected), eval_ranges(x,y),
            atol=1e-04)

        # Bottom
        x           = dr.linspace(mi.Float, 0, 1, axis_res)
        y           = dr.linspace(mi.Float, 1, 2, axis_res)
        y_expected  = dr.linspace(mi.Float, 1, 1, axis_res)
        assert dr.allclose(eval_ranges(x, y_expected), eval_ranges(x,y),
            atol=1e-04)

        # Left
        x           = dr.linspace(mi.Float, -1, 0, axis_res)
        y           = dr.linspace(mi.Float, 0, 1, axis_res)
        x_expected  = dr.linspace(mi.Float, 0, 0, axis_res)
        assert dr.allclose(eval_ranges(x_expected, y), eval_ranges(x,y),
            atol=1e-04)

        # Right
        x           = dr.linspace(mi.Float, 1, 2, axis_res)
        y           = dr.linspace(mi.Float, 0, 1, axis_res)
        x_expected  = dr.linspace(mi.Float, 1, 1, axis_res)
        assert dr.allclose(eval_ranges(x_expected, y), eval_ranges(x,y),
            atol=1e-04)

    elif wrap_mode == 'mirror':
        # Top left
        x = dr.linspace(mi.Float, 0, -1, axis_res)
        y = dr.linspace(mi.Float, 0, -1, axis_res)
        assert dr.allclose(0, ref - eval_ranges(x, y), atol=1e-04)

        # Bottom right
        x = dr.linspace(mi.Float, 2, 1, axis_res)
        y = dr.linspace(mi.Float, 2, 1, axis_res)
        assert dr.allclose(0, ref - eval_ranges(x, y), atol=1e-04)


@fresolver_append_path
def test04_eval_rgb(variants_vec_backends_once_rgb):
    import numpy as np

    # RGB image
    bitmap = mi.load_dict({
        'type' : 'bitmap',
        'filename' : 'resources/data/common/textures/carrot.png'
    })

    x_res, y_res =  bitmap.resolution()
    # Coordinates of green pixel: (7, 1)
    x = (1 / x_res) * 7 + (1 / (2 * x_res))
    y = (1 / y_res) * 1 + (1 / (2 * y_res))

    si = dr.zeros(mi.SurfaceInteraction3f)
    si.uv = [x, y]

    mono = bitmap.eval_1(si)
    color = bitmap.eval_3(si)
    spec = bitmap.eval(si)

    expected = [0.0012, 0.1946, 0.0241]
    assert dr.allclose(0, color - spec)
    assert dr.allclose(expected, color, atol=1e-04)
    assert dr.allclose(mi.luminance(expected), mono, atol=1e-04)

    # Grayscale image
    bitmap = mi.load_dict({
        'type' : 'bitmap',
        'filename' : 'resources/data/common/textures/noise_02.png'
    })

    x_res, y_res =  bitmap.resolution()
    # Coordinates of gray pixel: (0, 15)
    x = (1 / x_res) * 0 + (1 / (2 * x_res))
    y = (1 / y_res) * 15 + (1 / (2 * y_res))

    si = dr.zeros(mi.SurfaceInteraction3f)
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
    import numpy as np

    # RGB image
    bitmap = mi.load_dict({
        'type' : 'bitmap',
        'filename' : 'resources/data/common/textures/carrot.png'
    })

    x_res, y_res =  bitmap.resolution()
    # Coordinates of green pixel: (7, 1)
    x = (1 / x_res) * 7 + (1 / (2 * x_res))
    y = (1 / y_res) * 1 + (1 / (2 * y_res))

    si = dr.zeros(mi.SurfaceInteraction3f)
    si.wavelengths = np.linspace(mi.MI_CIE_MIN, mi.MI_CIE_MAX, mi.MI_WAVELENGTH_SAMPLES)
    si.uv = [x, y]

    with pytest.raises(RuntimeError):
        mono = bitmap.eval_1(si)
    with pytest.raises(RuntimeError):
        color = bitmap.eval_3(si)
    spec = bitmap.eval(si)

    expected = [0.0023, 0.1910, 0.0059, 0.0003]
    assert dr.allclose(expected, spec, atol=1e-04)

    # Grayscale image
    bitmap = mi.load_dict({
        'type' : 'bitmap',
        'filename' : 'resources/data/common/textures/noise_02.png'
    })

    x_res, y_res =  bitmap.resolution()
    # Coordinates of gray pixel: (0, 15)
    x = (1 / x_res) * 0 + (1 / (2 * x_res))
    y = (1 / y_res) * 15 + (1 / (2 * y_res))

    si = dr.zeros(mi.SurfaceInteraction3f)
    si.uv = [x, y]

    mono = bitmap.eval_1(si)
    with pytest.raises(RuntimeError):
        color = bitmap.eval_3(si)
    spec = bitmap.eval(si)

    expected = 0.5394
    assert dr.allclose(expected, spec, atol=1e-04)
    assert dr.allclose(expected, mono, atol=1e-04)


def test06_tensor_load(variants_all_rgb):
    bitmap = mi.load_dict({
        'type' : 'bitmap',
        'data' : dr.ones(mi.TensorXf, shape = [30, 30, 3]),
        'raw' : True
    })

    assert dr.allclose(bitmap.mean(), 1.0);

    bitmap = mi.load_dict({
        'type' : 'bitmap',
        'data' : dr.full(mi.TensorXf, 3.0, shape = [30, 30, 3]),
        'raw' : True
    })

    assert dr.allclose(bitmap.mean(), 3.0);