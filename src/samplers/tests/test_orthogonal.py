import mitsuba
import pytest
import enoki as ek
import numpy as np

from .utils import ( check_uniform_scalar_sampler, check_uniform_wavefront_sampler,
                     check_deep_copy_sampler_scalar, check_deep_copy_sampler_wavefront )

def test01_orthogonal_scalar(variant_scalar_rgb):
    from mitsuba.core import xml

    sampler = xml.load_dict({
        "type" : "orthogonal",
        "sample_count" : 1369,
    })

    check_uniform_scalar_sampler(sampler, res=4, atol=4.0)


def test02_orthogonal_wavefront(variants_vec_backends_once):
    from mitsuba.core import xml

    sampler = xml.load_dict({
        "type" : "orthogonal",
        "sample_count" : 1369,
    })

    check_uniform_wavefront_sampler(sampler, atol=4.0)


def test03_copy_sampler_scalar(variants_any_scalar):
    from mitsuba.core import xml

    sampler = xml.load_dict({
        "type" : "orthogonal",
        "sample_count" : 1369,
    })

    check_deep_copy_sampler_scalar(sampler)

def test04_copy_sampler_wavefront(variants_vec_backends_once):
    from mitsuba.core import xml

    sampler = xml.load_dict({
        "type" : "orthogonal",
        "sample_count" : 1369,
    })

    check_deep_copy_sampler_wavefront(sampler, factor=37)
