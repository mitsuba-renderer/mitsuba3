from mitsuba.core import warp
from mitsuba.core.chi2 import SphericalDomain, PlanarDomain
import numpy as np


def deg2rad(value):
    return value * np.pi / 180

DEFAULT_SETTINGS = {'sample_dim': 2, 'ires': 10, 'res': 101, 'parameters': []}

DISTRIBUTIONS = [
    ('Uniform square', PlanarDomain(np.array([[0, 1],
                                              [0, 1]])),
    lambda x: x,
    lambda x: np.ones(x.shape[0]),
     DEFAULT_SETTINGS),

    ('Uniform triangle', PlanarDomain(np.array([[0, 1],
                                                [0, 1]])),
     warp.squareToUniformTriangle,
     warp.squareToUniformTrianglePdf,
     dict(DEFAULT_SETTINGS, res=100)),

    ('Tent function', PlanarDomain(np.array([[-1, 1],
                                             [-1, 1]])),
     warp.squareToTent,
     warp.squareToTentPdf,
     DEFAULT_SETTINGS),

    ('Uniform disk', PlanarDomain(),
     warp.squareToUniformDisk,
     warp.squareToUniformDiskPdf,
     DEFAULT_SETTINGS),

    ('Uniform disk (concentric)', PlanarDomain(),
     warp.squareToUniformDiskConcentric,
     warp.squareToUniformDiskConcentricPdf,
     DEFAULT_SETTINGS),

    ('Uniform sphere', SphericalDomain(),
     warp.squareToUniformSphere,
     warp.squareToUniformSpherePdf,
     DEFAULT_SETTINGS),

    ('Uniform hemisphere', SphericalDomain(),
     warp.squareToUniformHemisphere,
     warp.squareToUniformHemispherePdf,
     DEFAULT_SETTINGS),

    ('Cosine hemisphere', SphericalDomain(),
     warp.squareToCosineHemisphere,
     warp.squareToCosineHemispherePdf,
     DEFAULT_SETTINGS),

    ('Uniform cone', SphericalDomain(),
    lambda sample, angle:
        warp.squareToUniformCone(sample, np.cos(deg2rad(angle))),
    lambda v, angle:
        warp.squareToUniformConePdf(v, np.cos(deg2rad(angle))),
     dict(DEFAULT_SETTINGS,
         parameters=[
             ('Cutoff angle', [1e-4, 180, 20])
         ])),

    ('Beckmann distribution', SphericalDomain(),
    lambda sample, value:
        warp.squareToBeckmann(sample,
            np.exp(np.log(0.05) * (1 - value) + np.log(1) * value)),
    lambda v, value:
        warp.squareToBeckmannPdf(v,
            np.exp(np.log(0.05) * (1 - value) + np.log(1) * value)),
     dict(DEFAULT_SETTINGS,
         parameters=[
             ('Roughness', [0, 1, 0.2])
         ])),

    ('von Mises-Fisher distribution', SphericalDomain(),
     warp.squareToVonMisesFisher,
     warp.squareToVonMisesFisherPdf,
     dict(DEFAULT_SETTINGS,
         parameters=[
             ('Concentration', [0, 100, 10])
         ])),

    ('Rough fiber distribution', SphericalDomain(),
     lambda sample, kappa, incl: warp.squareToRoughFiber(
         sample, [np.sin(deg2rad(incl)), 0, np.cos(deg2rad(incl))],
         [1, 0, 0], kappa),
     lambda v, kappa, incl: warp.squareToRoughFiberPdf(
         v, [np.sin(deg2rad(incl)), 0, np.cos(deg2rad(incl))],
         [1, 0, 0], kappa),
     dict(DEFAULT_SETTINGS,
         sample_dim=3,
         parameters=[
             ('Concentration', [0, 100, 10]),
             ('Inclination', [0, 90, 20])
         ]))
]

