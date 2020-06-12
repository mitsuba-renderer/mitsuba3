import mitsuba
import pytest
import enoki as ek
import numpy as np

def check_uniform_scalar_sampler(sampler, res=16, atol=0.5):
    sampler.seed(0)
    sample_count = sampler.sample_count()

    hist_1d = np.zeros(res)
    hist_2d = np.zeros((res, res))

    for i in range(sample_count):
        v_1d = sampler.next_1d()
        hist_1d[int(v_1d * res)] += 1
        v_2d = sampler.next_2d()
        hist_2d[int(v_2d.x * res), int(v_2d.y * res)] += 1
        sampler.advance()

    # print(hist_1d)
    # print(hist_2d)

    assert ek.allclose(hist_1d, float(sample_count) / res, atol=atol)
    assert ek.allclose(hist_2d, float(sample_count) / (res * res), atol=atol)


def check_uniform_wavefront_sampler(sampler, res=16, atol=0.5):
    from mitsuba.core import Float, UInt32, UInt64, Vector2u

    sample_count = sampler.sample_count()
    sampler.set_samples_per_wavefront(sample_count)

    sampler.seed(0, sample_count)

    hist_1d = ek.zero(UInt32, res)
    hist_2d = ek.zero(UInt32, res * res)

    v_1d = ek.clamp(sampler.next_1d() * res, 0, res)
    ek.scatter_add(
        target=hist_1d,
        index=UInt32(v_1d),
        source=UInt32(1.0)
    )

    v_2d = Vector2u(ek.clamp(sampler.next_2d() * res, 0, res))
    ek.scatter_add(
        target=hist_2d,
        index=UInt32(v_2d.x * res + v_2d.y),
        source=UInt32(1.0)
    )

    assert ek.allclose(Float(hist_1d), float(sample_count) / res, atol=atol)
    assert ek.allclose(Float(hist_2d), float(sample_count) / (res * res), atol=atol)
