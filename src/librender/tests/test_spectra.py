import numpy as np
import pytest

import mitsuba
from mitsuba.scalar_rgb.core import MTS_WAVELENGTH_SAMPLES
from mitsuba.scalar_rgb.core.xml import load_string


def test01_cie1931():
    """CIE 1931 observer"""
    XYZw = mitsuba.core.cie1931_xyz([600])
    assert np.allclose(XYZw[0, 0], 1.0622)
    assert np.allclose(XYZw[0, 1], 0.631)
    assert np.allclose(XYZw[0, 2], 0.0008)

    Y = mitsuba.core.cie1931_y([600])
    assert np.allclose(Y[0], 0.631)

def test02_interpolated():
    """interpolated: Check the interpolated spectrum (using D64 spectrum values)"""
    d65 = load_string("""<spectrum version='2.0.0' type='interpolated'>
        <string name="values" value='46.6383, 49.3637, 52.0891, 51.0323, 49.9755, 52.3118, 54.6482, 68.7015, 82.7549, 87.1204, 91.486, 92.4589, 93.4318, 90.057, 86.6823, 95.7736, 104.865, 110.936, 117.008, 117.41, 117.812, 116.336, 114.861, 115.392, 115.923, 112.367, 108.811, 109.082, 109.354, 108.578, 107.802, 106.296, 104.79, 106.239, 107.689, 106.047, 104.405, 104.225, 104.046, 102.023, 100.0, 98.1671, 96.3342, 96.0611, 95.788, 92.2368, 88.6856, 89.3459, 90.0062, 89.8026, 89.5991, 88.6489, 87.6987, 85.4936, 83.2886, 83.4939, 83.6992, 81.863, 80.0268, 80.1207, 80.2146, 81.2462, 82.2778, 80.281, 78.2842, 74.0027, 69.7213, 70.6652, 71.6091, 72.979, 74.349, 67.9765, 61.604, 65.7448, 69.8856, 72.4863, 75.087, 69.3398, 63.5927, 55.0054, 46.4182, 56.6118, 66.8054, 65.0941, 63.3828, 63.8434, 64.304, 61.8779, 59.4519, 55.7054, 51.959, 54.6998, 57.4406, 58.8765, 60.3125'/>
        <float name='lambda_min' value='360'/>
        <float name='lambda_max' value='830'/>
    </spectrum>
    """)
    assert np.allclose(d65.eval([350, 456, 600, 700, 840]), [0, 117.49, 90.0062, 71.6091, 0])

def test04_d65():
    """d65: Spot check the model in a few places, the chi^2 test will ensure
    that sampling works."""
    d65 = load_string("<spectrum version='2.0.0' type='d65'/>").expand()[0]
    assert np.allclose(d65.eval([350, 456, 600, 700, 840]),
                       np.array([0, 117.49, 90.0062, 71.6091, 0]) / 10568.0)

def test05_blackbody():
    """blackbody: Spot check the model in a few places, the chi^2 test will
    ensure that sampling works."""
    bb = load_string("""<spectrum version='2.0.0' type='blackbody'>
        <float name='temperature' value='5000'/>
    </spectrum>""")
    assert np.allclose(bb.eval([350, 456, 600, 700, 840]),
                       [0, 10997.9, 12762.4, 11812, 0])

def test06_srgb_d65():
    """srgb_d65 emitters should evaluate to the product of D65 and sRGB spectra,
    with intensity factored out when evaluating the sRGB model."""
    np.random.seed(12345)
    wavelengths = np.linspace(300, 800, 30)
    wavelengths += (10 * np.random.uniform(size=wavelengths.shape)).astype(np.int)
    d65 = load_string("<spectrum version='2.0.0' type='d65'/>").expand()[0]
    d65_eval = d65.eval(wavelengths)

    for color in [[0, 0, 0], [1, 1, 1], [0.1, 0.2, 0.3], [34, 0.1, 62],
                  [0.001, 0.02, 11.4]]:
        intensity  = (np.max(color) * 2.0) or 1.0
        normalized = np.array(color) / intensity

        srgb = load_string("""
            <spectrum version="2.0.0" type="srgb">
                <color name="color" value="{}"/>
            </spectrum>
        """.format(', '.join(map(str, normalized))))

        srgb_d65 = load_string("""
            <spectrum version="2.0.0" type="srgb_d65">
                <color name="color" value="{}"/>
            </spectrum>
        """.format(', '.join(map(str, color))))

        assert np.allclose(srgb_d65.eval(wavelengths),
                           d65_eval * intensity * srgb.eval(wavelengths))

def test07_sample_rgb_spectrum():
    """rgb_spectrum: Spot check the model in a few places, the chi^2 test will
    ensure that sampling works."""

    try:
        from mitsuba.scalar_spectral.core import sample_rgb_spectrum, pdf_rgb_spectrum
        from mitsuba.scalar_spectral.core import MTS_WAVELENGTH_MIN, MTS_WAVELENGTH_MAX
    except ImportError:
        pytest.skip("scalar_spectral mode not enabled")

    def spot_check(sample, expected_wav, expected_weight):
        wav, weight = sample_rgb_spectrum(sample)
        pdf         = pdf_rgb_spectrum(wav)
        assert np.allclose(wav, expected_wav)
        assert np.allclose(weight, expected_weight)
        assert np.allclose(pdf, 1.0 / weight)

    if (MTS_WAVELENGTH_MIN == 360 and MTS_WAVELENGTH_MAX == 830):
        spot_check(0.1, 424.343, 465.291)
        spot_check(0.5, 545.903, 254.643)
        spot_check(0.8, 635.381, 400.432)
        # PDF is zero outside the range
        assert pdf_rgb_spectrum(MTS_WAVELENGTH_MIN - 10.0) == 0.0
        assert pdf_rgb_spectrum(MTS_WAVELENGTH_MAX + 0.5)  == 0.0

def test08_rgb2spec_fetch_eval_mean():
    try:
        from mitsuba.scalar_spectral.render import srgb_model_fetch, srgb_model_eval, srgb_model_mean
        from mitsuba.scalar_spectral.core import MTS_WAVELENGTH_MIN, MTS_WAVELENGTH_MAX
    except ImportError:
        pytest.skip("scalar_spectral mode not enabled")

    rgb_values = np.array([
        [0, 0, 0],
        [0.0129831, 0.0129831, 0.0129831],
        [0.0241576, 0.0241576, 0.0241576],
        [0.0423114, 0.0423114, 0.0423114],
        [0.0561285, 0.0561285, 0.0561285],
        [0.0703601, 0.0703601, 0.0703601],
        [0.104617, 0.104617, 0.104617],
        [0.171441, 0.171441, 0.171441],
        [0.242281, 0.242281, 0.242281],
        [0.814846, 0.814846, 0.814846],
        [0.913098, 0.913098, 0.913098],
        [0.930111, 0.930111, 0.930111],
        [0.955973, 0.955973, 0.955973],
        [0.973445, 0.973445, 0.973445],
        [0.982251, 0.982251, 0.982251],
        [0.991102, 0.991102, 0.991102],
        [1, 1, 1],
    ])
    wavelengths = np.linspace(MTS_WAVELENGTH_MIN, MTS_WAVELENGTH_MAX, MTS_WAVELENGTH_SAMPLES)

    for i in range(rgb_values.shape[0]):
        rgb = rgb_values[i, :]
        assert np.all((rgb >= 0.0) & (rgb <= 1.0)), "Invalid RGB attempted"
        coeff = srgb_model_fetch(rgb)
        mean  = srgb_model_mean(coeff)
        value = srgb_model_eval(coeff, wavelengths)

        assert not np.any(np.isnan(coeff)), "{} => coeff = {}".format(rgb, coeff)
        assert not np.any(np.isnan(mean)),  "{} => mean = {}".format(rgb, mean)
        assert not np.any(np.isnan(value)), "{} => value = {}".format(rgb, value)
