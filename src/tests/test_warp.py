import unittest

from mitsuba import math
from math import ceil, floor, atan2, sqrt, cos, sin
from mitsuba import math, pcg32
from mitsuba.hypothesis import adaptiveSimpson2D, chi2_test
from mitsuba.warp import *

class WarpTest(unittest.TestCase):
    # Statistical tests parameters
    (seed1, seed2) = (42, 1337)  # Fixed for reproducibility
    minExpFrequency = 5
    significanceLevel = 0.01

    def setUp(self):
        scaleDisk = 4
        scalePlane = 100 # TODO: why?
        scaleSphere = 4 * math.Pi
        scaleHemisphere = scaleSphere

        def toUnitBox(x, y):
            p = Point2f()
            (p[0], p[1]) = (2 * x - 1, 2 * y - 1)
            return p
        def fromUnitBox(p):
            return (0.5 * p[0] + 0.5, 0.5 * p[1] + 0.5)

        # In practice, the standard normal distribution is
        # almost zero outside of the 5-box in R^2.
        def toPlane(x, y):
            p = toUnitBox(x, y)
            p[0] *= 5.0
            p[1] *= 5.0
            return p
        def fromPlane(p):
            p[0] /= 5.0
            p[1] /= 5.0
            return fromUnitBox(p)

        def to3d(x, y):
            x *= 2 * math.Pi;
            y = y * 2 - 1;
            sinTheta = sqrt(1 - y * y)
            (cosPhi, sinPhi) = (cos(x), sin(x));
            v = Vector3f()
            (v[0], v[1], v[2]) = (sinTheta * cosPhi, sinTheta * sinPhi, y)
            return v
        def from3d(v):
            x = atan2(v[1], v[0]) * math.InvTwoPi
            if (x < 0):
                x += 1
            y = 0.5 * v[2] + 0.5
            return (x, y)


        # name -> (warping function, pdf function, domain indicator, scale,
        #          coordinates to domain function, domain to coordinates function)
        self.warps = dict()
        self.warps["Square to uniform sphere"] = (
            squareToUniformSphere, lambda _: squareToUniformSpherePdf(),
            scaleSphere, unitSphereIndicator, to3d, from3d)
        self.warps["Square to uniform hemisphere"] = (
            squareToUniformHemisphere, lambda _: squareToUniformHemispherePdf(),
            scaleHemisphere, unitHemisphereIndicator, to3d, from3d)

        # self.warps["Square to cosine hemisphere"] = (
        #     squareToCosineHemisphere, lambda _: squareToCosineHemispherePdf(),
        #     scaleHemisphere, unitHemisphereIndicator, to3d, from3d)
        # self.warps["Square to uniform cone"] = (
        #     squareToUniformCone, lambda _: squareToUniformConePdf(),
        #     scale3d, unitConeIndicator, to3d, from3d)

        self.warps["Square to uniform disk"] = (
            squareToUniformDisk, lambda _: squareToUniformDiskPdf(),
            scaleDisk, unitDiskIndicator, toUnitBox, fromUnitBox)
        self.warps["Square to uniform disk concentric"] = (
            squareToUniformDiskConcentric, lambda _: squareToUniformDiskConcentricPdf(),
            scaleDisk, unitDiskIndicator, toUnitBox, fromUnitBox)
        self.warps["Square to standard normal distribution"] = (
            squareToStdNormal, squareToStdNormalPdf, scalePlane,
            lambda _: True, toPlane, fromPlane)

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
        # TODO: increase resolution & sample more points
        samplingResolution = 31
        (gridWidth, gridHeight) = (samplingResolution, samplingResolution)
        nBins = gridWidth * gridHeight
        sampleCount = 100 * nBins

        def generatePoints(warpFunction):
            # TODO: consider Grid and Stratified sampling strategies
            rng = pcg32()
            rng.seed(WarpTest.seed1, WarpTest.seed2)

            warped = []
            for i in range(sampleCount):
                p = [0. 0.]
                (p[0], p[1]) = (rng.nextFloat(), rng.nextFloat())
                warped.append(warpFunction(p))

            return warped

        def computeHistogram(points, pointFromDomain):
            hist = [0 for _ in range(nBins)]
            for p in points:
                (x, y) = pointFromDomain(p)
                x = min(gridWidth - 1, max(0, int(floor(x * gridWidth))))
                y = min(gridHeight - 1, max(0, int(floor(y * gridHeight))))
                hist[y * gridWidth + x] += 1
            return hist

        def generateExpectedHistogram(pdfFunction, pdfScale, domainIndicator, pointToDomain):
            scale = pdfScale * sampleCount

            def integrand(x, y):
                v = pointToDomain(x, y)
                if domainIndicator(v):
                  return pdfFunction(v)
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

        def statisticalTest(observedHistogram, expectedHistogram):
            (result, text) = chi2_test(nBins, observedHistogram, expectedHistogram,
                                       sampleCount, WarpTest.minExpFrequency,
                                       WarpTest.significanceLevel, 1)
            self.assertTrue(result, text)

        def printHistogram(hist):
            print("----------")
            for y in range(gridHeight):
                for x in range(gridWidth):
                    print(hist[y * gridWidth + x], end='  ')
                print(' ')
            print("----------")


        for (name, (warpFunction, pdfFunction, pdfScale, indicator,
                    toDomain, fromDomain)) in self.warps.items():
            with self.subTest(name):
                observed = computeHistogram(generatePoints(warpFunction), fromDomain)
                expected = generateExpectedHistogram(pdfFunction, pdfScale, indicator, toDomain)
                # Run the chi^2 test
                # printHistogram(observed)
                # printHistogram(expected)
                statisticalTest(observed, expected)

if __name__ == '__main__':
    unittest.main()
