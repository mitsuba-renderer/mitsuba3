import pytest
import enoki as ek


def test01_discr_empty(variant_packet_rgb):
    # Test that operations involving the empty distribution throw
    from mitsuba.core import DiscreteDistribution

    d = DiscreteDistribution()
    assert d.empty()

    with pytest.raises(RuntimeError) as excinfo:
        d.update()
    assert 'empty distribution' in str(excinfo.value)


def test02_discr_zero_prob(variant_packet_rgb):
    # Test that operations involving zero probability mass throw
    from mitsuba.core import DiscreteDistribution

    with pytest.raises(RuntimeError) as excinfo:
        DiscreteDistribution([0, 0, 0])
    assert "no probability mass found" in str(excinfo.value)


def test03_discr_neg_prob(variant_packet_rgb):
    # Test that operations involving negative probability mass throw
    from mitsuba.core import DiscreteDistribution

    with pytest.raises(RuntimeError) as excinfo:
        DiscreteDistribution([1, -1, 1])
    assert "entries must be non-negative" in str(excinfo.value)


def test04_discr_basic(variant_packet_rgb):
    # Validate discrete distribution cdf/pmf against hand-computed reference
    from mitsuba.core import DiscreteDistribution, Float

    x = DiscreteDistribution([1, 3, 2])
    assert len(x) == 3

    assert x.sum() == 6
    assert ek.allclose(x.normalization(), 1.0 / 6.0)
    assert x.pmf() == [1, 3, 2]
    assert x.cdf() == [1, 4, 6]
    assert x.eval_pmf([1, 2, 0]) == [3, 2, 1]

    assert ek.allclose(
        x.eval_pmf_normalized([1, 2, 0]),
        Float([3, 2, 1]) / 6.0
    )
    assert ek.allclose(
        x.eval_cdf_normalized([1, 2, 0]),
        Float([4, 6, 1]) / 6.0
    )

    assert repr(x) == 'DiscreteDistribution[\n  size = 3,' \
        '\n  sum = 6,\n  pmf = [1, 3, 2]\n]'

    x.pmf()[:] = [1, 1, 1]
    x.update()
    assert x.cdf() == [1, 2, 3]
    assert x.sum() == 3
    assert ek.allclose(x.normalization(), 1.0 / 3.0)


def test05_discr_sample(variant_packet_rgb):
    # Validate discrete distribution sampling against hand-computed reference
    from mitsuba.core import DiscreteDistribution, Float
    eps = 1e-7

    x = DiscreteDistribution([1, 3, 2])
    assert x.sample([-1, 0, 1, 2]) == [0, 0, 2, 2]
    assert x.sample([1 / 6.0 - eps, 1 / 6.0 + eps]) == [0, 1]
    assert x.sample([4 / 6.0 - eps, 4 / 6.0 + eps]) == [1, 2]

    assert ek.allclose(
        x.sample_pmf([-1, 0, 1, 2]),
        ([0, 0, 2, 2], Float([1, 1, 2, 2]) / 6)
    )

    assert ek.allclose(
        x.sample_pmf([1 / 6.0 - eps, 1 / 6.0 + eps]),
        ([0, 1], Float([1, 3]) / 6)
    )

    assert ek.allclose(
        x.sample_pmf([4 / 6.0 - eps, 4 / 6.0 + eps]),
        ([1, 2], Float([3, 2]) / 6)
    )

    assert ek.allclose(
        x.sample_reuse([0, 1 / 12.0, 1 / 6.0 - eps, 1 / 6.0 + eps]),
        ([0, 0, 0, 1], Float([0, .5, 1, 0])),
        atol=3 * eps
    )

    assert ek.allclose(
        x.sample_reuse_pmf([0, 1 / 12.0, 1 / 6.0 - eps, 1 / 6.0 + eps]),
        ([0, 0, 0, 1], Float([0, .5, 1, 0]), Float([1, 1, 1, 3]) / 6),
        atol=3 * eps
    )


def test06_discr_bruteforce(variant_packet_rgb):
    # Brute force validation of discrete distribution sampling
    from mitsuba.core import DiscreteDistribution, Float, PCG32, UInt64

    rng = PCG32(initseq=UInt64.arange(50))

    for size in range(2, 20):
        for i in range(2, 50):
            density = Float(rng.next_uint32_bounded(i)[0:size])
            if ek.hsum(density) == 0:
                continue
            ddistr = DiscreteDistribution(density)

            x = ek.linspace(Float, 0, 1, 20)
            y = ddistr.sample(x)
            z = ek.gather(ddistr.cdf(), y - 1, y > 0)
            x *= ddistr.sum()

            # Did we sample the right interval?
            assert ek.all((x > z) | (ek.eq(x, 0) & (x >= z)))


def test07_discr_leading_trailing_zeros(variant_packet_rgb):
    # Check that sampling still works when there are zero-valued buckets
    from mitsuba.core import DiscreteDistribution
    x = DiscreteDistribution([0, 0, 1, 0, 1, 0, 0, 0])
    index, pmf = x.sample_pmf([-100, 0, 0.5, 0.5 + 1e-6, 1, 100])
    assert index == [2, 2, 2, 4, 4, 4]
    assert pmf == [.5] * 6


def test08_cont_empty(variant_packet_rgb):
    # Test that operations involving the empty distribution throw
    from mitsuba.core import ContinuousDistribution

    d = ContinuousDistribution()
    assert d.empty()
    d.range()[:] = [1, 2]

    with pytest.raises(RuntimeError) as excinfo:
        d.update()
    assert 'needs at least two entries' in str(excinfo.value)

    with pytest.raises(RuntimeError) as excinfo:
        ContinuousDistribution([1, 2], [1])
    assert 'needs at least two entries' in str(excinfo.value)


def test09_cont_empty_invalid_range(variant_packet_rgb):
    # Test that invalid range specifications throw an exception
    from mitsuba.core import ContinuousDistribution

    with pytest.raises(RuntimeError) as excinfo:
        ContinuousDistribution([1, 1], [1, 1])
    assert 'invalid range' in str(excinfo.value)

    with pytest.raises(RuntimeError) as excinfo:
        ContinuousDistribution([2, 1], [1, 1])
    assert 'invalid range' in str(excinfo.value)


def test10_cont_zero_prob(variant_packet_rgb):
    # Test that operations involving zero probability mass throw
    from mitsuba.core import ContinuousDistribution

    with pytest.raises(RuntimeError) as excinfo:
        ContinuousDistribution([1, 2], [0, 0, 0])
    assert "no probability mass found" in str(excinfo.value)


def test11_cont_neg_prob(variant_packet_rgb):
    # Test that operations involving negative probability mass throw
    from mitsuba.core import ContinuousDistribution

    with pytest.raises(RuntimeError) as excinfo:
        ContinuousDistribution([1, 2], [1, -1, 1])
    assert "entries must be non-negative" in str(excinfo.value)


def test12_cont_eval(variant_packet_rgb):
    # Test continuous 1D distribution pdf/cdf against hand-computed reference
    from mitsuba.core import ContinuousDistribution
    d = ContinuousDistribution([2, 3], [1, 2])
    eps = 1e-6

    assert ek.allclose(d.integral(), 3.0 / 2.0)
    assert ek.allclose(d.normalization(), 2.0 / 3.0)
    assert ek.allclose(
        d.eval_pdf_normalized([1, 2 - eps, 2, 2.5, 3, 3 + eps, 4]),
        [0, 0, 2.0 / 3.0, 1.0, 4.0 / 3.0, 0, 0]
    )
    assert ek.allclose(
        d.eval_cdf_normalized([1, 2, 2.5, 3, 4]),
        [0, 0, 5.0 / 12.0, 1, 1]
    )

    assert d.sample([0, 1]) == [2, 3]
    x, pdf = d.sample_pdf([0, 0.5, 1])
    dx = (ek.sqrt(10) - 2) / 2
    assert x == [2, 2 + dx, 3]
    assert ek.allclose(
        pdf,
        [2.0 / 3.0, (4 * dx + 2 * (1 - dx)) / 3.0, 4.0 / 3.0]
    )


def test13_cont_func(variant_packet_rgb):
    # Test continuous 1D distribution integral against analytic result
    from mitsuba.core import ContinuousDistribution, Float

    x = ek.linspace(Float, -2, 2, 513)
    y = ek.exp(-ek.sqr(x))

    d = ContinuousDistribution([-2, 2], y)
    assert ek.allclose(d.integral(), ek.sqrt(ek.pi) * ek.erf(2.0))
    assert ek.allclose(d.eval_pdf([1]), [ek.exp(-1)])
    assert ek.allclose(d.sample([0, 0.5, 1]), [-2, 0, 2])


def test14_irrcont_empty(variant_packet_rgb):
    # Test that operations involving the empty distribution throw
    from mitsuba.core import IrregularContinuousDistribution

    d = IrregularContinuousDistribution()
    assert d.empty()
    d.nodes()[:] = [1]
    d.pdf()[:] = [1]

    with pytest.raises(RuntimeError) as excinfo:
        d.update()
    assert 'needs at least two entries' in str(excinfo.value)

    d.nodes()[:] = [1, 2]
    d.pdf()[:] = [1]

    with pytest.raises(RuntimeError) as excinfo:
        d.update()
    assert 'size mismatch' in str(excinfo.value)


def test15_irrcont_empty_invalid_range(variant_packet_rgb):
    # Test that invalid range specifications throw an exception
    from mitsuba.core import IrregularContinuousDistribution

    with pytest.raises(RuntimeError) as excinfo:
        IrregularContinuousDistribution([2, 1], [1, 1])
    assert 'strictly increasing' in str(excinfo.value)

    with pytest.raises(RuntimeError) as excinfo:
        IrregularContinuousDistribution([1, 1], [1, 1])
    assert 'strictly increasing' in str(excinfo.value)


def test16_irrcont_zero_prob(variant_packet_rgb):
    # Test that operations involving the empty distribution throw
    from mitsuba.core import IrregularContinuousDistribution

    with pytest.raises(RuntimeError) as excinfo:
        IrregularContinuousDistribution([1, 2, 3], [0, 0, 0])
    assert "no probability mass found" in str(excinfo.value)


def test17_irrcont_neg_prob(variant_packet_rgb):
    # Test that operations involving negative probability mass throw
    from mitsuba.core import IrregularContinuousDistribution

    with pytest.raises(RuntimeError) as excinfo:
        IrregularContinuousDistribution([1, 2, 3], [1, -1, 1])
    assert "entries must be non-negative" in str(excinfo.value)


def test18_irrcont_simple_function(variant_packet_rgb):
    # Reference from Mathematica
    from mitsuba.core import IrregularContinuousDistribution, Float

    d = IrregularContinuousDistribution([1, 1.5, 1.8, 5], [1, 3, 0, 1])
    assert ek.allclose(d.integral(), 3.05)
    assert ek.allclose(
        d.eval_pdf([0, 1, 2, 3, 4, 5, 6]),
        [0, 1, 0.0625, 0.375, 0.6875, 1, 0]
    )
    assert ek.allclose(
        d.eval_cdf([0, 1, 2, 3, 4, 5, 6]),
        [0, 0, 1.45625, 1.675, 2.20625, 3.05, 3.05]
    )

    assert ek.allclose(
        d.sample(ek.linspace(Float, 0, 1, 11)),
        [1., 1.21368, 1.35622, 1.47111, 1.58552, 2.49282,
         3.35949, 3.8938, 4.31714, 4.67889, 5.]
    )

    assert ek.allclose(
        d.sample_pdf(ek.linspace(Float, 0, 1, 11)),
        ([1., 1.21368, 1.35622, 1.47111, 1.58552, 2.49282,
          3.35949, 3.8938, 4.31714, 4.67889, 5.],
         Float([1., 1.85472, 2.42487, 2.88444, 2.14476, 0.216506,
                0.48734, 0.654313, 0.786607, 0.899653, 1.])
         * d.normalization())
    )
