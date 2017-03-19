import mitsuba
import numpy as np
from mitsuba.core.xml import load_string
import os

n_samples = mitsuba.core.MTS_WAVELENGTH_SAMPLES

# CIE 1931 observer
def test01_cie1931():
    X, Y, Z = mitsuba.core.cie1931_xyz([600]*n_samples)
    # Actual values: 1.0622, 0.631, 0.0008
    assert np.allclose(X[0], 1.05593)
    assert np.allclose(Y[0], 0.634136)
    assert np.allclose(Z[0], 4.23136e-05)

    Y = mitsuba.core.cie1931_y([600]*n_samples)
    assert np.allclose(Y[0], 0.634136)

# rgb_importance: Spot check the model in a few places, the chi^2 test will ensure that sampling works
def test02_rgb_importance():
    rgb_importance = load_string("<spectrum version='2.0.0' type='rgb_importance'/>")
    assert np.allclose(rgb_importance.eval([360, 500, 830]), [0.00104682, 0.003659, 0.00022832])

# d65: Spot check the model in a few places, the chi^2 test will ensure that sampling works
def test03_d65():
    d65 = load_string("<spectrum version='2.0.0' type='d65'/>")
    assert np.allclose(d65.eval([350, 456, 600, 700, 840]), [0, 117.49, 90.0062, 71.6091, 0])

# blackbody: Spot check the model in a few places, the chi^2 test will ensure that sampling works
def test04_blackbody():
    bb = load_string("<spectrum version='2.0.0' type='blackbody'><float name='temperature' value='5000'/></spectrum>")
    assert np.allclose(bb.eval([350, 456, 600, 700, 840]), [0, 10997.9, 12762.4, 11812, 0])
