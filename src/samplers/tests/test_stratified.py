import pytest
import drjit as dr
import mitsuba as mi

from .utils import ( check_uniform_scalar_sampler, check_uniform_wavefront_sampler,
                     check_deep_copy_sampler_scalar, check_deep_copy_sampler_wavefront )

def test01_stratified_scalar(variant_scalar_rgb):
    sampler = mi.load_dict({
        "type" : "stratified",
        "sample_count" : 4096,
    })

    check_uniform_scalar_sampler(sampler, res=4)


def test02_stratified_wavefront(variants_vec_backends_once):
    sampler = mi.load_dict({
        "type" : "stratified",
        "sample_count" : 4096,
    })

    check_uniform_wavefront_sampler(sampler)


def test03_copy_sampler_scalar(variants_any_scalar):
    sampler = mi.load_dict({
        "type" : "stratified",
        "sample_count" : 4096,
    })

    check_deep_copy_sampler_scalar(sampler)


def test04_copy_sampler_wavefront(variants_vec_backends_once):
    sampler = mi.load_dict({
        "type" : "stratified",
        "sample_count" : 4096,
    })

    check_deep_copy_sampler_wavefront(sampler)
