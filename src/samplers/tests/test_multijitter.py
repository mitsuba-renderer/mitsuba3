import pytest
import drjit as dr
import mitsuba as mi

from .utils import (
    check_uniform_scalar_sampler,
    check_uniform_wavefront_sampler,
    check_deep_copy_sampler_scalar,
    check_deep_copy_sampler_wavefront,
    check_sampler_kernel_hash_wavefront,
)


def test01_multijitter_scalar(variant_scalar_rgb):
    sampler = mi.load_dict({
        "type" : "multijitter",
        "sample_count" : 1024,
    })

    check_uniform_scalar_sampler(sampler)


def test02_multijitter_wavefront(variants_vec_backends_once):
    sampler = mi.load_dict({
        "type" : "multijitter",
        "sample_count" : 1024,
    })

    check_uniform_wavefront_sampler(sampler)


def test03_copy_sampler_scalar(variants_any_scalar):
    sampler = mi.load_dict({
        "type" : "multijitter",
        "sample_count" : 1024,
    })

    check_deep_copy_sampler_scalar(sampler)

def test04_copy_sampler_wavefront(variants_vec_backends_once):
    sampler = mi.load_dict({
        "type" : "multijitter",
        "sample_count" : 1024,
    })

    check_deep_copy_sampler_wavefront(sampler)
    
def test05_jit_seed(variants_vec_rgb):
    sampler = mi.load_dict({
        "type": "independent",
    })
    seed = mi.UInt(0)
    state_before = seed.state
    sampler.seed(seed, 64)
    assert seed.state == state_before

    check_sampler_kernel_hash_wavefront(mi.UInt, sampler)
