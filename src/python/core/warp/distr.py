from mitsuba.core import warp
from mitsuba.core.chi2 import SphericalDomain, PlanarDomain, LineDomain
import numpy as np

def deg2rad(value):
    return value * np.pi / 180

DEFAULT_SETTINGS = {'sample_dim': 2, 'ires': 10, 'res': 101, 'parameters': []}

def SpectrumAdapter(xml_string):
    from mitsuba.core.xml import load_string
    plugin = load_string(xml_string)

    def sample_functor(sample):
        wavelength, weight, _ = plugin.sample(sample)
        return wavelength[:, 0], weight[:, 0]

    def pdf_functor(pdf):
        return plugin.pdf(pdf)

    return sample_functor, pdf_functor

DISTRIBUTIONS = [
    ('Uniform square', PlanarDomain(np.array([[0, 1],
                                              [0, 1]])),
    lambda x: x,
    lambda x: np.ones(x.shape[0]),
     DEFAULT_SETTINGS),

    ('Uniform triangle', PlanarDomain(np.array([[0, 1],
                                                [0, 1]])),
     warp.square_to_uniform_triangle,
     warp.square_to_uniform_triangle_pdf,
     dict(DEFAULT_SETTINGS, res=100)),

    ('Tent function', PlanarDomain(np.array([[-1, 1],
                                             [-1, 1]])),
     warp.square_to_tent,
     warp.square_to_tent_pdf,
     DEFAULT_SETTINGS),

    ('Uniform disk', PlanarDomain(),
     warp.square_to_uniform_disk,
     warp.square_to_uniform_disk_pdf,
     DEFAULT_SETTINGS),

    ('Uniform disk (concentric)', PlanarDomain(),
     warp.square_to_uniform_disk_concentric,
     warp.square_to_uniform_disk_concentric_pdf,
     DEFAULT_SETTINGS),

    ('Uniform sphere', SphericalDomain(),
     warp.square_to_uniform_sphere,
     warp.square_to_uniform_sphere_pdf,
     DEFAULT_SETTINGS),

    ('Uniform hemisphere', SphericalDomain(),
     warp.square_to_uniform_hemisphere,
     warp.square_to_uniform_hemisphere_pdf,
     DEFAULT_SETTINGS),

    ('Cosine hemisphere', SphericalDomain(),
     warp.square_to_cosine_hemisphere,
     warp.square_to_cosine_hemisphere_pdf,
     DEFAULT_SETTINGS),

    ('Uniform cone', SphericalDomain(),
    lambda sample, angle:
        warp.square_to_uniform_cone(sample, np.cos(deg2rad(angle))),
    lambda v, angle:
        warp.square_to_uniform_cone_pdf(v, np.cos(deg2rad(angle))),
     dict(DEFAULT_SETTINGS,
         parameters=[
             ('Cutoff angle', [1e-4, 180, 20])
         ])),

    ('Beckmann distribution', SphericalDomain(),
    lambda sample, value:
        warp.square_to_beckmann(sample,
            np.exp(np.log(0.05) * (1 - value) + np.log(1) * value)),
    lambda v, value:
        warp.square_to_beckmann_pdf(v,
            np.exp(np.log(0.05) * (1 - value) + np.log(1) * value)),
     dict(DEFAULT_SETTINGS,
         parameters=[
             ('Roughness', [0, 1, 0.6])
         ])),

    ('von Mises-Fisher distribution', SphericalDomain(),
     warp.square_to_von_mises_fisher,
     warp.square_to_von_mises_fisher_pdf,
     dict(DEFAULT_SETTINGS,
         parameters=[
             ('Concentration', [0, 100, 10])
         ])),

    ('Rough fiber distribution', SphericalDomain(),
     lambda sample, kappa, incl: warp.square_to_rough_fiber(
         sample, np.tile([np.sin(deg2rad(incl)), 0, np.cos(deg2rad(incl))], [sample.shape[0], 1]),
         np.tile([1, 0, 0], [sample.shape[0], 1]), kappa),
     lambda v, kappa, incl: warp.square_to_rough_fiber_pdf(
         v, np.tile([np.sin(deg2rad(incl)), 0, np.cos(deg2rad(incl))], [v.shape[0], 1]),
         np.tile([1, 0, 0], [v.shape[0], 1]), kappa),
     dict(DEFAULT_SETTINGS,
         sample_dim=3,
         parameters=[
             ('Concentration', [0, 100, 10]),
             ('Inclination', [0, 90, 20])
         ])
    ),

    ('Spectrum[importance]', LineDomain([360.0, 830.0]),
     *SpectrumAdapter('<spectrum version="2.0.0" type="importance"/>'),
     dict(DEFAULT_SETTINGS, sample_dim=1))
]
