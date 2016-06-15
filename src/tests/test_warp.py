import unittest

from mitsuba import math
from math import ceil, floor
from mitsuba import math, pcg32
from mitsuba.hypothesis import adaptiveSimpson2D, chi2_test
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

    def test02_statistical_tests(self):
        (seed1, seed2) = (42, 1337)  # Fixed for reproducibility
        minExpFrequency = 5
        significanceLevel = 0.01
        # TODO: increase resolution & sample more points
        samplingResolution = 31
        (gridWidth, gridHeight) = (samplingResolution, samplingResolution)
        nBins = gridWidth * gridHeight
        sampleCount = 100 * nBins

        warpFunction = squareToUniformDisk
        pdfFunction = squareToUniformDiskPdf

        def generatePoints():
            # TODO: consider Grid and Stratified sampling strategies
            rng = pcg32()
            rng.seed(seed1, seed2)

            warped = []
            for i in range(sampleCount):
                p = [0. 0.]
                (p[0], p[1]) = (rng.nextFloat(), rng.nextFloat())
                warped.append(warpFunction(p))

            return warped

        def computeHistogram(points):
            hist = [0 for _ in range(nBins)]
            for p in points:
                # TODO: domain needs to be shifted depending on warping type
                x = p[0] * 0.5 + 0.5
                y = p[1] * 0.5 + 0.5

                x = min(gridWidth - 1, max(0, int(floor(x * gridWidth))))
                y = min(gridHeight - 1, max(0, int(floor(y * gridHeight))))
                hist[y * gridWidth + x] += 1
            return hist

        def generateExpectedHistogram():
            # TODO: depends on the type of warping
            scale = 4 * sampleCount

            def integrand(x, y):
                # Need to convert x \in [0, 1] to [-1, 1]
                x = 2 * x - 1
                y = 2 * y - 1
                if (x * x + y * y <= 1.0):
                  return pdfFunction()
                else:
                  return 0.0

            hist = [0 for _ in range(gridWidth * gridHeight)]
            for y in range(gridHeight):
                for x in range(gridWidth):
                    (xStart, yStart) = (float(x) / gridWidth, float(y) / gridHeight)
                    (xEnd, yEnd) = ((x+1.0) / gridWidth, (y+1.0) / gridHeight)
                    v = adaptiveSimpson2D(integrand, xStart, yStart, xEnd, yEnd)
                    hist[y * gridWidth + x] = scale * v
                    if (v < 0):
                        self.fail("PDF returned a negative values: ({}, {}) = {}".format(x, y, v))
            return hist

        def printHistogram(hist):
            print("----------")
            for y in range(gridHeight):
                for x in range(gridWidth):
                    print(hist[y * gridWidth + x], end='  ')
                print(' ')
            print("----------")

        def statisticalTest(observedHistogram, expectedHistogram):
            (result, text) = chi2_test(nBins, observedHistogram, expectedHistogram,
                                       sampleCount, minExpFrequency, significanceLevel, 1)
            print(result, text)
            self.assertTrue(result, text)

        observed = computeHistogram(generatePoints())
        expected = generateExpectedHistogram()
        # Run the chi^2 test
        # printHistogram(observed)
        # printHistogram(expected)
        statisticalTest(observed, expected)

if __name__ == '__main__':
    unittest.main()
