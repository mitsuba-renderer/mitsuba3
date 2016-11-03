from __future__ import unicode_literals
from mitsuba.core.chi2 import ChiSquareTest, SphericalDomain, PlanarDomain
from mitsuba.core import warp
import pytest

CHI2_TESTS = [
    # ("Uniform square", SphericalDomain(),
     # lambda x: x,
     # lambda x: np.ones(x.shape[1])),

    ("Uniform disk", PlanarDomain(),
     warp.squareToUniformDisk,
     warp.squareToUniformDiskPdf),

    ("Uniform disk (concentric)", PlanarDomain(),
     warp.squareToUniformDiskConcentric,
     warp.squareToUniformDiskConcentricPdf),

    ("Uniform sphere", SphericalDomain(),
     warp.squareToUniformSphere,
     warp.squareToUniformSpherePdf),

    ("Uniform hemisphere", SphericalDomain(),
     warp.squareToUniformHemisphere,
     warp.squareToUniformHemispherePdf),

    ("Cosine hemisphere", SphericalDomain(),
     warp.squareToCosineHemisphere,
     warp.squareToCosineHemispherePdf)
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
