from mitsuba.core.xml import load_string
import numpy as np
import os

# Spot check the model in a few places, the chi^2 test will ensure that sampling works
def test01_rgb_importance():
    rgb_importance = load_string("<spectrum version='2.0.0' type='rgb_importance'/>")
    assert np.allclose(rgb_importance.eval([360, 500, 830]), [0.00104682, 0.003659, 0.00022832])
