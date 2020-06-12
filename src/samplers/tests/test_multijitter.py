import mitsuba
import pytest
import enoki as ek
import numpy as np

from .utils import check_uniform_scalar_sampler, check_uniform_wavefront_sampler

def test01_multijitter_scalar(variant_scalar_rgb):
    from mitsuba.core import xml

    sampler = xml.load_dict({
        "type" : "multijitter",
        "sample_count" : 1024,
    })

    check_uniform_scalar_sampler(sampler)


def test02_multijitter_wavefront(variant_gpu_rgb):
    from mitsuba.core import xml

    sampler = xml.load_dict({
        "type" : "multijitter",
        "sample_count" : 1024,
    })

    check_uniform_wavefront_sampler(sampler)
