import numpy as np

import mitsuba
import pytest
import enoki as ek
from enoki.dynamic import Float32 as Float


def test01_create(variant_scalar_rgb):
    from mitsuba.core.xml import load_string
    p = load_string("<phase version='2.0.0' type='isotropic'/>")
    assert p is not None


def test02_eval(variant_scalar_rgb):
    from mitsuba.core.math import Pi
    from mitsuba.render import PhaseFunctionContext, MediumInteraction3f
    from mitsuba.core.xml import load_string

    p = load_string("<phase version='2.0.0' type='isotropic'/>")
    ctx = PhaseFunctionContext(None)
    mi = MediumInteraction3f()
    for theta in np.linspace(0, np.pi / 2, 4):
        for ph in np.linspace(0, np.pi, 4):
            wo = [np.sin(theta), 0, np.cos(theta)]
            v_eval = p.eval(ctx, mi, wo)
            assert np.allclose(v_eval, 1.0 / (4 * Pi))

def test03_chi2(variant_packet_rgb):
    from mitsuba.python.chi2 import PhaseFunctionAdapter, ChiSquareTest, SphericalDomain
    from mitsuba.core import ScalarBoundingBox2f

    sample_func, pdf_func = PhaseFunctionAdapter("isotropic", '')

    chi2 = ChiSquareTest(
        domain = SphericalDomain(),
        sample_func = sample_func,
        pdf_func = pdf_func,
        sample_dim = 2
    )

    result = chi2.run(0.1)
    chi2._dump_tables()
    assert result
