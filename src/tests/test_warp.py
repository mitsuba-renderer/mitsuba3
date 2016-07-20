try:
    import unittest2 as unittest
except:
    import unittest

from mitsuba.core import math, BoundingBox3f
from mitsuba.core.warp import *

import warp_factory

class WarpTest(unittest.TestCase):
    # Statistical tests parameters
    # TODO: consider fixing seed
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

        self.assertAlmostEqual(squareToStdNormalPdf(zero2D), math.InvTwoPi, places = 6)
        self.assertTrue(squareToStdNormalPdf(ten2D) < 0.0001)

        self.assertAlmostEqual(squareToTentPdf(zero2D), 1, places = 6)
        self.assertAlmostEqual(squareToTentPdf(ten2D), 0, places = 6)

        # Just checking that these are not crashing, the actual results
        # are tested statistically.
        _ = squareToUniformSphere(p)
        _ = squareToUniformHemisphere(p)
        _ = squareToCosineHemisphere(p)
        _ = squareToUniformCone(p, 0.5)
        _ = squareToUniformDisk(p)
        _ = squareToUniformDiskConcentric(p)
        _ = uniformDiskToSquareConcentric(p)
        _ = squareToUniformTriangle(p)
        _ = squareToStdNormal(p)
        _ = squareToStdNormalPdf(p)
        _ = squareToTent(p)
        _ = squareToTentPdf(p)
        _ = intervalToNonuniformTent(0.25, 0.5, 1.0, 0.75)

    def test02_indentity(self):
        w = IdentityWarpAdapter()
        self.assertTrue(w.isIdentity())
        self.assertEqual(w.inputDimensionality(), 2)
        self.assertEqual(w.domainDimensionality(), 2)
        p1 = [0.5, 0.3]
        (p2, weight) = w.warpSample(p1)
        self.assertAlmostEqual(weight, 1.0, places=6)
        self.assertAlmostEqual(p1[0], p2[0], places=6)
        self.assertAlmostEqual(p1[1], p2[1], places=6)
        self.assertEqual(w.name(), "Identity")

    def test03_statistical_tests(self):
        def warpWithUnitWeight(f):
            return lambda p: (f(p), 1.0)

        def createValueGrid(arguments, n):
            """Returns a list of dicts, each representing a set of parameter
            values to try out in order to explore the parameter space.
            n: number of values to try out for each parameter."""
            if not arguments:
                return [dict()]

            import itertools
            values = [[arg.map(i / float(n-1)) for i in range(n)] for arg in arguments]
            candidates = []
            for t in list(itertools.product(*values)):
                args = dict()
                for i in range(len(arguments)):
                    args[arguments[i].name] = t[i]
                candidates.append(args)
            return candidates


        # Import pre-defined factories for all available warping functions
        warps = warp_factory.factories
        # Number of parameter values to try out for each argument
        nParametersValues = 4

        def runTest(warpAdapter):
            samplingType = SamplingType.Independent

            # TODO: increase sampling resolution and sample count if needed
            samplingResolution = 31
            (gridWidth, gridHeight) = (samplingResolution, samplingResolution)
            nBins = gridWidth * gridHeight
            sampleCount = 100 * nBins

            if warpAdapter.domainDimensionality() <= 1:
                gridHeight = 1;
            elif warpAdapter.domainDimensionality() >= 3:
                gridWidth *= 2;

            return runStatisticalTest(
                sampleCount, gridWidth, gridHeight,
                samplingType, warpAdapter,
                WarpTest.minExpFrequency, WarpTest.significanceLevel)

        for warpFactory in warps.values():
            # Cover a few values for each of the warping function's parameters
            for args in createValueGrid(warpFactory.arguments, nParametersValues):
                warpAdapter = warpFactory.bind(args)

                with self.subTest("Warp: " + warpAdapter.name() + ", args: " + str(args)):
                    (result, reason) = runTest(warpAdapter)
                    self.assertTrue(result, reason)

if __name__ == '__main__':
    unittest.main()
