import mitsuba
import pytest
import enoki as ek
import numpy as np

from .utils import check_uniform_scalar_sampler, check_uniform_wavefront_sampler

def test01_orthogonal_scalar(variant_scalar_rgb):
    from mitsuba.core import xml

    sampler = xml.load_dict({
        "type" : "orthogonal",
        "sample_count" : 1369,
    })

    check_uniform_scalar_sampler(sampler, res=4, atol=4.0)


def test02_orthogonal_wavefront(variant_gpu_rgb):
    from mitsuba.core import xml

    sampler = xml.load_dict({
        "type" : "orthogonal",
        "sample_count" : 1369,
    })

    check_uniform_wavefront_sampler(sampler, atol=4.0)
