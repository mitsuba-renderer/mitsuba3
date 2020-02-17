import mitsuba
import pytest
import enoki as ek
from enoki.dynamic import Float32 as Float


@pytest.fixture(scope="module")
def sampler():
    return make_sampler()
def make_sampler(sample_count=8):
    from mitsuba.core.xml import load_string
    s = load_string("""<sampler version="2.0.0" type="independent">
            <integer name="sample_count" value="%d"/>
        </sampler>""" % sample_count)
    assert s is not None
    return s


def next_float(rng, shape = None):
    return rng.next_float32(shape=shape) if shape else rng.next_float32()


def test01_construct(variant_scalar_rgb):
    s = make_sampler(sample_count=58)
    assert s.sample_count() == 58


def test02_sample_vs_pcg32(variant_scalar_rgb, sampler):
    # IndependentSampler uses the default-constructed PCG if no seed is provided.
    ref = ek.scalar.PCG32()
    for i in range(10):
        assert ek.all(sampler.next_1d() == next_float(ref))
        assert ek.all(sampler.next_2d() == [next_float(ref), next_float(ref)])


def test03_clone(variant_scalar_rgb, sampler):
    """After cloning, the new sampler should *not* yield the same values: it
    should automatically be seeded differently rather than copying the exact
    state of the original sampler."""
    sampler2 = sampler.clone()
    for i in range(5):
        assert ek.all(sampler.next_1d() != sampler2.next_1d())
        assert ek.all(sampler.next_2d() != sampler2.next_2d())


def test04_seed_vectorized(variant_scalar_rgb, sampler):
    """For a given seed, the first lane of a sampled packet should be equal
    to the sample of a scalar independent sampler."""

    try:
        mitsuba.set_variant('packet_rgb')
    except:
        pytest.skip("packet_rgb mode not enabled")

    from mitsuba.core.xml import load_string

    sampler_p = load_string("""<sampler version="2.0.0" type="independent">
                <integer name="sample_count" value="%d"/>
            </sampler>""" % 8)

    for seed in range(10):
        sampler.seed(seed)
        sampler_p.seed(seed)
        assert sampler.next_1d() == sampler_p.next_1d()[0]
        assert ek.allclose(sampler_p.next_2d(), ek.dynamic.Vector2f(sampler.next_2d()))
