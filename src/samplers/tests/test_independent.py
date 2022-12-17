import pytest
import drjit as dr
import mitsuba as mi

from .utils import check_deep_copy_sampler_scalar ,check_deep_copy_sampler_wavefront

def test01_construct(variant_scalar_rgb):
    sampler = mi.load_dict({
        "type": "independent",
        "sample_count": 58
    })
    assert sampler is not None
    assert sampler.sample_count() == 58


def test02_sample_vs_pcg32(variant_scalar_rgb):
    sampler = mi.load_dict({
        "type": "independent",
        "sample_count": 8
    })
    sampler.seed(0)

    # IndependentSampler uses the default-constructed PCG if no seed is provided.
    rng = dr.scalar.PCG32(initstate=0)
    for i in range(10):
        assert dr.all(sampler.next_1d() == rng.next_float32())
        assert dr.all(sampler.next_2d() == [rng.next_float32(), rng.next_float32()])


def test03_copy_sampler_scalar(variants_any_scalar):
    sampler = mi.load_dict({
        "type": "independent",
        "sample_count": 1024
    })

    check_deep_copy_sampler_scalar(sampler)

def test04_copy_sampler_wavefront(variants_vec_backends_once):
    sampler = mi.load_dict({
        "type": "independent",
        "sample_count": 1024
    })

    check_deep_copy_sampler_wavefront(sampler)
