from __future__ import unicode_literals
from mitsuba.scalar_rgb.core.chi2 import ChiSquareTest
from mitsuba.scalar_rgb.core.warp.distr import DISTRIBUTIONS
from mitsuba.test.util import fresolver_append_path
import pytest

@fresolver_append_path
def run_chi2(name, domain, adapter, settings):
    parameters = [o[1][2] for o in settings['parameters']]
    sample, pdf = adapter

    test = ChiSquareTest(
        domain=domain,
        sample_func=lambda *args: sample(*(list(args) + parameters)),
        pdf_func=lambda *args: pdf(*(list(args) + parameters)),
        res=settings['res'],
        ires=settings['ires'],
        sample_dim=settings['sample_dim'],
    )

    result = test.run(0.01, len(DISTRIBUTIONS))
    print(test.messages)
    assert result

@pytest.mark.parametrize("name, domain, adapter, settings", DISTRIBUTIONS)
def test_chi2(name, domain, adapter, settings):
    run_chi2(name, domain, adapter, settings)