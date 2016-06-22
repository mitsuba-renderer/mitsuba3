try:
    import unittest2 as unittest
except:
    import unittest

from mitsuba import math
from mitsuba.warp import *

class WarpTest(unittest.TestCase):
    # Statistical tests parameters
    (seed1, seed2) = (42, 1337)  # Fixed for reproducibility
    minExpFrequency = 5
    significanceLevel = 0.01

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

    def test02_statistical_tests(self):
        # TODO: just iterate over the enum (pybind enum?)
        warps = [
            WarpType.NoWarp,
            WarpType.UniformSphere,
            WarpType.UniformHemisphere,
            # WarpType.CosineHemisphere,
            # WarpType.UniformCone,
            WarpType.UniformDisk,
            WarpType.UniformDiskConcentric,
            # WarpType.UniformTriangle,
            WarpType.StandardNormal,
            # WarpType.UniformTent,
            # WarpType.NonUniformTent
        ]

        # TODO: cover all sampling types
        samplingType = SamplingType.Independent
        # TODO: also cover several parameter values when relevant
        parameterValue = 0.5

        # TODO: increase sampling resolution and sample count
        samplingResolution = 31
        (gridWidth, gridHeight) = (samplingResolution, samplingResolution)
        nBins = gridWidth * gridHeight
        sampleCount = 100 * nBins

        for warpType in warps:
            with self.subTest(str(warpType)):
                (result, reason) = runStatisticalTest(
                    sampleCount, gridWidth, gridHeight,
                    samplingType, warpType, parameterValue,
                    WarpTest.minExpFrequency, WarpTest.significanceLevel)
                self.assertTrue(result, reason)

if __name__ == '__main__':
    unittest.main()
