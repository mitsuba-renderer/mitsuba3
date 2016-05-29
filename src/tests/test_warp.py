import unittest

from mitsuba import math
from mitsuba.warp import *

class WarpTest(unittest.TestCase):


    def test01_deterministic_calls(self):
        p = [0.5, 0.25]

        self.assertAlmostEqual(squareToUniformSpherePdf(), math.InvFourPi, places = 6)
        self.assertAlmostEqual(squareToUniformHemispherePdf(), math.InvTwoPi, places = 6)
        self.assertAlmostEqual(squareToUniformConePdf(0.5), math.InvTwoPi / 0.5, places = 6)
        self.assertAlmostEqual(squareToUniformDiskPdf(), math.InvPi, places = 6)
        self.assertAlmostEqual(squareToUniformDiskConcentricPdf(), math.InvPi, places = 6)

        # Just checking that these are not crashing, the actual results
        # are tested statistically.
        _ = squareToUniformSphere(p)
        _ = squareToUniformHemisphere(p)
        _ = squareToCosineHemisphere(p)
        _ = squareToUniformCone(0.5, p)
        _ = squareToUniformDisk(p)
        _ = squareToUniformDiskConcentric(p)
        _ = uniformDiskToSquareConcentric(p)
        _ = squareToUniformTriangle(p)
        _ = squareToStdNormal(p)
        _ = squareToStdNormalPdf(p)
        _ = squareToTent(p)
        _ = intervalToNonuniformTent(0.25, 0.5, 1.0, 0.75)

    @unittest.skip("Not implemented yet")
    def test02_statistical_tests(self):
        # TODO: use a chi^2 test to check that the correct distributions
        # are obtained when using the warping functions.
        pass
