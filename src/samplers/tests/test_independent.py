import mitsuba
import pytest
import enoki as ek


def test01_construct(variant_scalar_rgb):
    from mitsuba.core.xml import load_string
    sampler = load_string("""<sampler version="2.0.0" type="independent">
                                <integer name="sample_count" value="%d"/>
                             </sampler>""" % 58)
    assert sampler is not None
    return sampler
    assert sampler.sample_count() == 58


def test02_sample_vs_pcg32(variant_scalar_rgb):
    from mitsuba.core.xml import load_string
    sampler = load_string("""<sampler version="2.0.0" type="independent">
                                <integer name="sample_count" value="%d"/>
                             </sampler>""" % 8)

    # IndependentSampler uses the default-constructed PCG if no seed is provided.
    rng = ek.scalar.PCG32()
    for i in range(10):
        assert ek.all(sampler.next_1d() == rng.next_float32())
        assert ek.all(sampler.next_2d() == [rng.next_float32(), rng.next_float32()])

