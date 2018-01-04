import numpy as np
import pytest

from mitsuba.core import PCG32, float_dtype
from mitsuba.core.xml import load_string
from mitsuba.render import Sampler


@pytest.fixture
def sampler(sample_count = 8):
    s = load_string("""<sampler version="2.0.0" type="independent">
            <integer name="sample_count" value="%d"/>
        </sampler>""" % sample_count)
    assert s is not None
    return s

def next_float(rng, shape = None):
    if float_dtype == np.float32:
        return rng.next_float32(shape=shape) if shape else rng.next_float32()
    return rng.next_float64(shape=shape) if shape else rng.next_float64()


def test01_construct():
    s = sampler(sample_count=58)
    assert s.sample_count() == 58

def test02_sample_vs_pcg32(sampler):
    # IndependentSampler uses the default-constructed PCG if no seed is provided.
    ref = PCG32()

    for i in range(10):
        assert np.all(sampler.next_1d() == next_float(ref))
        assert np.all(sampler.next_2d() == [next_float(ref), next_float(ref)])

    # PCG32P is not exposed in Python, so it's harder to check the vectorized
    # variants against the reference, but we check at least that the samples
    # returned are not all equal in a packet.
    for i in range(5):
        res = sampler.next_1d_p()
        assert np.unique(res).size == res.size
        res = sampler.next_2d_p()
        assert np.unique(res).size == res.size

def test03_clone(sampler):
    """After cloning, the new sampler should *not* yield the same values: it
    should automatically be seeded differently rather than copying the exact
    state of the original sampler."""
    sampler2 = sampler.clone()
    for i in range(5):
        assert np.all(sampler.next_1d() != sampler2.next_1d())
        assert np.all(sampler.next_2d() != sampler2.next_2d())
        assert np.all(sampler.next_1d_p() != sampler2.next_1d_p())
        assert np.all(sampler.next_2d_p() != sampler2.next_2d_p())
