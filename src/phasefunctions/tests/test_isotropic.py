import numpy as np

from mitsuba.scalar_rgb.core.math import Pi
from mitsuba.scalar_rgb.core.xml import load_string
from mitsuba.scalar_rgb.render import PhaseFunctionContext, MediumInteraction3f

def test01_create():
    p = load_string("<phase version='2.0.0' type='isotropic'/>")
    assert p is not None


def test02_eval():
    p = load_string("<phase version='2.0.0' type='isotropic'/>")
    ctx = PhaseFunctionContext(None)
    mi = MediumInteraction3f()
    for theta in np.linspace(0, np.pi / 2, 4):
        for ph in np.linspace(0, np.pi, 4):
            wo = [np.sin(theta), 0, np.cos(theta)]
            v_eval = p.eval(ctx, mi, wo)
            assert np.allclose(v_eval, 1.0 / (4 * Pi))
