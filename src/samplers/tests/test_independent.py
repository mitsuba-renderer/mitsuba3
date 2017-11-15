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
    assert s.sample_index() == 0

def test02_sample_vs_pcg32(sampler):
    ref = PCG32()
    ref.seed(0, 1)

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

def test04_request_and_arrays(sampler):
    ref = PCG32()
    ref.seed(0, 1)

    sample_count = sampler.sample_count()
    sampler.request_1d_array(3)
    sampler.request_2d_array(7)
    sampler.request_1d_array(11)

    # Generate the arrays
    sampler.generate(offset=[-1, -1])  # Dummy offset value
    # To check against the reference, we need to generate the arrays in the
    # same order.
    ref_samples_1d = [
        [next_float(ref, shape=(3,)) for _ in range(sample_count)],
        [next_float(ref, shape=(11,)) for _ in range(sample_count)]
    ]
    ref_samples_2d = [
        [next_float(ref, shape=(7,2)) for _ in range(sample_count)]
    ]

    # Check array values and sizes.
    for i in range(sample_count):
        # For each sample index, the set of requested arrays is ready.
        assert np.all(sampler.next_1d_array(3) == ref_samples_1d[0][i])
        assert np.all(sampler.next_2d_array(7) == ref_samples_2d[0][i])
        assert np.all(sampler.next_1d_array(11) == ref_samples_1d[1][i])
        # No more arrays available, should throw.
        with pytest.raises(RuntimeError):
            sampler.next_1d_array(5)
        with pytest.raises(RuntimeError):
            sampler.next_2d_array(5)
        # Advance to the next sample, which resets the arrays indices.
        sampler.advance()

