import pytest

def test01_interpolation():
    try:
        from mitsuba.scalar_spectral.core.xml import load_string
        from mitsuba.scalar_spectral.core import MTS_WAVELENGTH_SAMPLES
        from mitsuba.scalar_spectral.render import PositionSample3f
        from mitsuba.scalar_spectral.render import SurfaceInteraction3f
    except ImportError:
        pytest.skip("scalar_spectral mode not enabled")

    s = load_string('''
            <spectrum version='2.0.0' type='interpolated'>
                <float name="lambda_min" value="500"/>
                <float name="lambda_max" value="600"/>
                <string name="values" value="1, 2"/>
            </spectrum>''')

    ps = PositionSample3f()

    assert all(s.eval(SurfaceInteraction3f(ps, [400] * MTS_WAVELENGTH_SAMPLES)) == 1)
    assert all(s.eval(SurfaceInteraction3f(ps, [500] * MTS_WAVELENGTH_SAMPLES)) == 1)
    assert all(s.eval(SurfaceInteraction3f(ps, [600] * MTS_WAVELENGTH_SAMPLES)) == 2)
    assert all(s.eval(SurfaceInteraction3f(ps, [700] * MTS_WAVELENGTH_SAMPLES)) == 2)
    assert all(s.sample(SurfaceInteraction3f(), [0] * MTS_WAVELENGTH_SAMPLES)[0] == 300)
    assert all(s.sample(SurfaceInteraction3f(), [1] * MTS_WAVELENGTH_SAMPLES)[0] == 900)
