from __future__ import unicode_literals
from mitsuba.core.chi2 import ChiSquareTest, SphericalDomain
from mitsuba.core import warp
import pytest
import numpy as np

CHI2_TESTS = [
    # ("Uniform square", SphericalDomain(),
     # lambda x: x,
     # lambda x: np.ones(x.shape[1])),

    ("Uniform sphere", SphericalDomain(),
     warp.squareToUniformSphere,
     warp.squareToUniformSpherePdf),

    ("Uniform hemisphere", SphericalDomain(),
     warp.squareToUniformHemisphere,
     warp.squareToUniformHemispherePdf),
]


@pytest.mark.parametrize("name, domain, sample, pdf", CHI2_TESTS)
def test_chi2(name, domain, sample, pdf):
    test = ChiSquareTest(
        domain,
        sample,
        pdf
    )
    result = test.run(0.01, len(CHI2_TESTS))
    print(test.messages)
    assert result
