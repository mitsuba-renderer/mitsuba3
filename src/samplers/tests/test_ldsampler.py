import mitsuba
import pytest
import enoki as ek
import numpy as np

from .utils import check_uniform_scalar_sampler, check_uniform_wavefront_sampler

def test01_ldsampler_scalar(variant_scalar_rgb):
    from mitsuba.core import xml

    sampler = xml.load_dict({
        "type" : "ldsampler",
        "sample_count" : 1024,
    })

    check_uniform_scalar_sampler(sampler)


def test02_ldsampler_wavefront(variant_gpu_rgb):
    from mitsuba.core import xml

    sampler = xml.load_dict({
        "type" : "ldsampler",
        "sample_count" : 1024,
    })

    check_uniform_wavefront_sampler(sampler)


def test03_ldsampler_deterministic_values(variant_scalar_rgb):
    from mitsuba.core import xml

    sampler = xml.load_dict({
        "type" : "ldsampler",
        "sample_count" : 1024,
    })

    sampler.seed(0)

    values_1d_dim0 = [0.1091986894, 0.0291205644, 0.8533393144, 0.3924018144, 0.6511908769,
                      0.9900580644, 0.7781440019, 0.2771674394, 0.4353705644, 0.3601752519]

    values_2d_dim0 = [[0.09034, 0.975105], [0.332528, 0.576667], [0.521004, 0.88819],
                      [0.00244939, 0.0629952], [0.182137, 0.367683], [0.679207, 0.296394],
                      [0.0219806, 0.137214], [0.308113, 0.995612], [0.548348, 0.579597]]

    values_1d_dim1 = [0.934394001, 0.534003376, 0.258612751, 0.856269001, 0.279120564, 0.434394001,
                      0.235175251, 0.654120564, 0.811347126, 0.958808064]

    values_2d_dim1 = [[0.771981, 0.387214], [0.732918, 0.113776], [0.0708088, 0.791511],
                      [0.966317, 0.849128], [0.959481, 0.199714], [0.392098, 0.985847],
                      [0.767098, 0.860847], [0.989754, 0.98194], [0.0932697, 0.726081]]

    for v in values_1d_dim0:
        assert ek.allclose(sampler.next_1d(), v)

    for v in values_2d_dim0:
        assert ek.allclose(sampler.next_2d(), v)

    sampler.advance()

    for v in values_1d_dim1:
        assert ek.allclose(sampler.next_1d(), v)

    for v in values_2d_dim1:
        assert ek.allclose(sampler.next_2d(), v)
