import mitsuba
import numpy as np
from mitsuba.core.xml import load_string
from mitsuba.core import ContinuousSpectrum
import os

n_samples = mitsuba.core.MTS_WAVELENGTH_SAMPLES

# CIE 1931 observer
def test01_cie1931():
    X, Y, Z = mitsuba.core.cie1931_xyz([600])
    assert np.allclose(X[0], 1.0622)
    assert np.allclose(Y[0], 0.631)
    assert np.allclose(Z[0], 0.0008)

    Y = mitsuba.core.cie1931_y([600])
    assert np.allclose(Y[0], 0.631)

# interpolated: Check the interpolated spectrum (using D64 spectrum values)
def test02_interpolated():
    d65 = load_string("""<spectrum version='2.0.0' type='interpolated'>
        <string name="values" value='46.6383, 49.3637, 52.0891, 51.0323, 49.9755, 52.3118, 54.6482, 68.7015, 82.7549, 87.1204, 91.486, 92.4589, 93.4318, 90.057, 86.6823, 95.7736, 104.865, 110.936, 117.008, 117.41, 117.812, 116.336, 114.861, 115.392, 115.923, 112.367, 108.811, 109.082, 109.354, 108.578, 107.802, 106.296, 104.79, 106.239, 107.689, 106.047, 104.405, 104.225, 104.046, 102.023, 100.0, 98.1671, 96.3342, 96.0611, 95.788, 92.2368, 88.6856, 89.3459, 90.0062, 89.8026, 89.5991, 88.6489, 87.6987, 85.4936, 83.2886, 83.4939, 83.6992, 81.863, 80.0268, 80.1207, 80.2146, 81.2462, 82.2778, 80.281, 78.2842, 74.0027, 69.7213, 70.6652, 71.6091, 72.979, 74.349, 67.9765, 61.604, 65.7448, 69.8856, 72.4863, 75.087, 69.3398, 63.5927, 55.0054, 46.4182, 56.6118, 66.8054, 65.0941, 63.3828, 63.8434, 64.304, 61.8779, 59.4519, 55.7054, 51.959, 54.6998, 57.4406, 58.8765, 60.3125'/>
        <float name='lambda_min' value='360'/>
        <float name='lambda_max' value='830'/>
    </spectrum>
    """)
    assert np.allclose(d65.eval([350, 456, 600, 700, 840]), [0, 117.49, 90.0062, 71.6091, 0])

# d65: Spot check the model in a few places, the chi^2 test will ensure that sampling works
def test04_d65():
    d65 = load_string("<spectrum version='2.0.0' type='d65'/>").expand()[0]
    assert np.allclose(d65.eval([350, 456, 600, 700, 840]), np.array([0, 117.49, 90.0062, 71.6091, 0]) / 10568.0)

# blackbody: Spot check the model in a few places, the chi^2 test will ensure that sampling works
def test05_blackbody():
    bb = load_string("<spectrum version='2.0.0' type='blackbody'><float name='temperature' value='5000'/></spectrum>")
    assert np.allclose(bb.eval([350, 456, 600, 700, 840]), [0, 10997.9, 12762.4, 11812, 0])

def test06_srgb_d65():
    """srgb_d65 emitters should evaluate to the product of D65 and sRGB spectra."""
    wavelengths = np.linspace(300, 800, 30)
    wavelengths += (10 * np.random.uniform(size=wavelengths.shape)).astype(np.int)
    d65 = load_string("<spectrum version='2.0.0' type='d65'/>").expand()[0]
    d65_eval = d65.eval(wavelengths)

    for color in ["0, 0, 0", "1, 1, 1", "0.1, 0.2, 0.3", "34, 0.1, 62",
                  "0.001, 0.02, 11.4"]:
        srgb = load_string("""<spectrum version="2.0.0" type="srgb">
                <color name="color" value="{}"/>
            </spectrum>
            """.format(color))
        srgb_d65 = load_string("""<spectrum version="2.0.0" type="srgb_d65">
                <color name="color" value="{}"/>
            </spectrum>
            """.format(color))

        assert np.allclose(srgb_d65.eval(wavelengths), d65_eval * srgb.eval(wavelengths))
