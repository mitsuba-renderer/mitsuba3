import numpy as np

from mitsuba.scalar_rgb.core.math import Pi
from mitsuba.scalar_rgb.core.xml import load_string


def test01_create():
    p = load_string("<phase version='2.0.0' type='isotropic'/>")
    assert p is not None


def test02_eval():
    p = load_string("<phase version='2.0.0' type='isotropic'/>")
    for theta in np.linspace(0, np.pi / 2, 4):
        for ph in np.linspace(0, np.pi, 4):
            wo = [np.sin(theta), 0, np.cos(theta)]
            v_eval = p.eval(wo)
            assert np.allclose(v_eval, 1.0 / (4 * Pi))
