import mitsuba
import pytest
import enoki as ek

def approx(v, eps=1e-6):
    return pytest.approx(v, abs=eps)

@pytest.fixture()
def variant():
    try:
        mitsuba.set_variant('packet_rgb')
    except ImportError:
        pytest.skip("packet_rgb mode not enabled")

def test01_discr_empty(variant):
    from mitsuba.core import DiscreteDistribution

    d = DiscreteDistribution()
    assert d.empty()

    with pytest.raises(RuntimeError) as excinfo:
        d.update()
    assert 'empty distribution' in str(excinfo.value)

def test02_discr_zero_prob(variant):
    from mitsuba.core import DiscreteDistribution

    with pytest.raises(RuntimeError) as excinfo:
        d = DiscreteDistribution([0, 0, 0])
    assert "no probability mass found" in str(excinfo.value)

def test03_discr_neg_prob(variant):
    from mitsuba.core import DiscreteDistribution

    with pytest.raises(RuntimeError) as excinfo:
        d = DiscreteDistribution([1, -1, 1])
    assert "entries must be non-negative" in str(excinfo.value)

def test04_discr_basic(variant):
    from mitsuba.core import DiscreteDistribution, Float

    x = DiscreteDistribution([1, 3, 2])
    assert len(x) == 3

    assert x.sum() == 6
    assert x.normalization() == approx(1.0 / 6.0)
    assert x.pmf() == [1, 3, 2]
    assert x.cdf() == [1, 4, 6]
    assert x.eval_pmf([1, 2, 0]) == [3, 2, 1]

    assert x.eval_pmf_normalized([1, 2, 0]) == approx(Float([3, 2, 1]) / 6.0)
    assert x.eval_cdf_normalized([1, 2, 0]) == approx(Float([4, 6, 1]) / 6.0)

    assert repr(x) == 'DiscreteDistribution[size=3, sum=6, pmf=[1, 3, 2]]'

    x.pmf()[:] = [1, 1, 1]
    x.update()
    assert x.cdf() == [1, 2, 3]
    assert x.sum() == 3
    assert x.normalization() == approx(1.0 / 3.0)

def test05_discr_sample(variant):
    from mitsuba.core import DiscreteDistribution, Float
    eps = 1e-7

    x = DiscreteDistribution([1, 3, 2])
    assert x.sample([-1, 0, 1, 2]) == [0, 0, 2, 2]
    assert x.sample([1/6.0 - eps, 1/6.0 + eps]) == [0, 1]
    assert x.sample([4/6.0 - eps, 4/6.0 + eps]) == [1, 2]

    assert x.sample_pmf([-1, 0, 1, 2]) == (
            [0, 0, 2, 2],
            approx(Float([1, 1, 2, 2]) / 6)
    )

    assert x.sample_pmf([1/6.0 - eps, 1/6.0 + eps]) == (
            [0, 1],
            approx(Float([1, 3]) / 6)
    )

    assert x.sample_pmf([4/6.0 - eps, 4/6.0 + eps]) == (
            [1, 2],
            approx(Float([3, 2]) / 6)
    )

    assert x.sample_reuse([0, 1/12.0, 1/6.0 - eps, 1/6.0 + eps]) == (
            [0, 0, 0, 1],
            approx(Float([0, .5, 1, 0]))
    )

    assert x.sample_reuse_pmf([0, 1/12.0, 1/6.0 - eps, 1/6.0 + eps]) == (
            [0, 0, 0, 1],
            approx(Float([0, .5, 1, 0])),
            approx(Float([1, 1, 1, 3]) / 6),
    )

def test06_discr_bruteforce(variant):
    from mitsuba.core import DiscreteDistribution, Float, PCG32, UInt64

    rng = PCG32(initseq=UInt64.arange(50))

    for size in range(2, 20):
        for i in range(2, 50):
            density = Float(rng.next_uint32_bounded(i)[0:size])
            if ek.hsum(density) == 0:
                continue
            ddistr = DiscreteDistribution(density)

            x = Float.linspace(0, 1, 20)
            y = ddistr.sample(x)
            z = ek.gather(ddistr.cdf(), y - 1, y > 0)
            x *= ddistr.sum()

            assert ek.all((x > z) | (ek.eq(x, 0) & (x >= z)))

def test07_discr_leading_trailing_zeros(variant):
    from mitsuba.core import DiscreteDistribution
    x = DiscreteDistribution([0, 0, 1, 0, 1, 0, 0, 0])
    index, pmf = x.sample_pmf([-100, 0, 0.5, 0.5 + 1e-6, 1, 100])
    assert index == [2, 2, 2, 4, 4, 4]
    assert pmf == [.5] * 6

def test08_cont_empty(variant):
    from mitsuba.core import ContinuousDistribution

    d = ContinuousDistribution()
    assert d.empty()
    d.range()[:] = [1, 2]

    with pytest.raises(RuntimeError) as excinfo:
        d.update()
    assert 'needs at least two entries' in str(excinfo.value)

    with pytest.raises(RuntimeError) as excinfo:
        d = ContinuousDistribution([1, 2], [1])
    assert 'needs at least two entries' in str(excinfo.value)

def test09_cont_empty_invalid_range(variant):
    from mitsuba.core import ContinuousDistribution

    with pytest.raises(RuntimeError) as excinfo:
        d = ContinuousDistribution([1, 1], [1, 1])
    assert 'invalid range' in str(excinfo.value)


    with pytest.raises(RuntimeError) as excinfo:
        d = ContinuousDistribution([2, 1], [1, 1])
    assert 'invalid range' in str(excinfo.value)

def test10_cont_zero_prob(variant):
    from mitsuba.core import ContinuousDistribution

    with pytest.raises(RuntimeError) as excinfo:
        d = ContinuousDistribution([1, 2], [0, 0, 0])
    assert "no probability mass found" in str(excinfo.value)

def test11_cont_neg_prob(variant):
    from mitsuba.core import ContinuousDistribution

    with pytest.raises(RuntimeError) as excinfo:
        d = ContinuousDistribution([1, 2], [1, -1, 1])
    assert "entries must be non-negative" in str(excinfo.value)

def test12_cont_eval(variant):
    from mitsuba.core import ContinuousDistribution
    d = ContinuousDistribution([2, 3], [1, 2])
    eps = 1e-6

    assert d.integral() == approx(3.0/2.0)
    assert d.normalization() == approx(2.0/3.0)
    assert d.eval_pdf_normalized([1, 2-eps, 2, 2.5, 3, 3+eps, 4]) == \
        approx([0, 0, 2.0/3.0, 3.0/3.0, 4.0/3.0, 0, 0])
    assert d.eval_cdf_normalized([1, 2, 2.5, 3, 4]) == approx([0, 0, 5.0/12.0, 1, 1])

    assert d.sample([0, 1]) == [2, 3]
    x, pdf = d.sample_pdf([0, 0.5, 1])
    dx =  (ek.sqrt(10) - 2) / 2
    assert x == [2, 2+dx, 3]
    assert pdf == approx([2.0/3.0, (4*dx + 2*(1-dx)) / 3.0, 4.0/3.0])

def test13_cont_func(variant):
    from mitsuba.core import ContinuousDistribution, Float

    import numpy as np
    x = np.linspace(-2, 2, 15110)
    y = np.sin(x)
    y = y*y
    y = Float(y)

    # x = Float.linspace(-2, 2, 51201)
    # y = ek.sqr(ek.sin(Float(np.float32(x))))

    d = ContinuousDistribution([-2, 2], y)
    assert d.integral() == approx(2 - ek.sin(4) / 2, 1e-5)
    assert d.eval_pdf([1]) == approx(ek.sqr(ek.sin(Float(1))), 1e-5)
    print(d.cdf())
    # assert d.sample([0, 0.5, 1]) == [-2, 0, 2]
    assert d.sample([0.5]) == [0]
