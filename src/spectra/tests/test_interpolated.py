def test01_interpolation():
    try:
        from mitsuba.scalar_spectral.core.xml import load_string
        from mitsuba.scalar_spectral.core import MTS_WAVELENGTH_SAMPLES
    except ImportError:
        pytest.skip("scalar_spectral mode not enabled")

    s = load_string('''
            <spectrum version='2.0.0' type='interpolated'>
                <float name="lambda_min" value="500"/>
                <float name="lambda_max" value="600"/>
                <string name="values" value="1, 2"/>
            </spectrum>''')

    assert all(s.eval([400] * MTS_WAVELENGTH_SAMPLES) == 1)
    assert all(s.eval([500] * MTS_WAVELENGTH_SAMPLES) == 1)
    assert all(s.eval([600] * MTS_WAVELENGTH_SAMPLES) == 2)
    assert all(s.eval([700] * MTS_WAVELENGTH_SAMPLES) == 2)
    assert all(s.sample([0] * MTS_WAVELENGTH_SAMPLES)[0] == 300)
    assert all(s.sample([1] * MTS_WAVELENGTH_SAMPLES)[0] == 900)
