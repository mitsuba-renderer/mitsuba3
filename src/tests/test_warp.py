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

    def test01_pdfs(self):
        p = [0.5, 0.25]
        unit = [0.0, 1.0, 0.0]
        ten = [10.0, 10.0, 10.0]
        zero2D = [0.0, 0.0]
        ten2D = [10.0, 10.0]

        # The PDF functions are > 0 inside of the warping function's target
        # domain, null outide.
        self.assertAlmostEqual(squareToUniformSpherePdf(unit), math.InvFourPi, places = 6)
        self.assertAlmostEqual(squareToUniformSpherePdf(ten), 0, places = 6)

        self.assertAlmostEqual(squareToUniformHemispherePdf(unit), math.InvTwoPi, places = 6)
        self.assertAlmostEqual(squareToUniformHemispherePdf(ten), 0, places = 6)

        self.assertAlmostEqual(squareToUniformConePdf([0.0, 0.0, 1.0], 0.5), math.InvTwoPi / 0.5, places = 6)
        self.assertAlmostEqual(squareToUniformConePdf(unit, 0.5), 0, places = 6)
        self.assertAlmostEqual(squareToUniformConePdf(ten, 0.5), 0, places = 6)

        self.assertAlmostEqual(squareToUniformDiskPdf(zero2D), math.InvPi, places = 6)
        self.assertAlmostEqual(squareToUniformDiskPdf(ten2D), 0, places = 6)

        self.assertAlmostEqual(squareToUniformDiskConcentricPdf(zero2D), math.InvPi, places = 6)
        self.assertAlmostEqual(squareToUniformDiskConcentricPdf(ten2D), 0, places = 6)

        # TODO: same thing for 1D functions

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
        _ = squareToTentPdf(p)
        _ = intervalToNonuniformTent(0.25, 0.5, 1.0, 0.75)

    def test02_statistical_tests(self):
        # TODO: just iterate over the enum (pybind enum?)
        # warps = [
        #     WarpType.NoWarp,
        #     WarpType.UniformSphere,
        #     WarpType.UniformHemisphere,
        #     # WarpType.CosineHemisphere,
        #     # WarpType.UniformCone,
        #     WarpType.UniformDisk,
        #     WarpType.UniformDiskConcentric,
        #     # WarpType.UniformTriangle,
        #     WarpType.StandardNormal,
        #     # WarpType.UniformTent,
        #     # WarpType.NonUniformTent
        # ]

        warps = [
            PlaneWarpAdapter("Square to uniform disk",
                lambda p: (squareToUniformDisk(p), 1.0),
                squareToUniformDiskPdf)
            # PlaneWarpAdapter("Square to tent", squareToTent, squareToTentPdf)
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

        for warpAdapter in warps:
            with self.subTest(str(warpAdapter)):

                print(warpAdapter)
                print(type(warpAdapter))

                # (result, reason) = runStatisticalTest("hello", 5, 5, SamplingType.Independent, warpAdapter, 5, 0.01)
                (result, reason) = runStatisticalTest(
                    sampleCount, gridWidth, gridHeight,
                    samplingType, warpAdapter,
                    WarpTest.minExpFrequency, WarpTest.significanceLevel)

                self.assertTrue(result, reason)

if __name__ == '__main__':
    unittest.main()
