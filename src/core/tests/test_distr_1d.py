import pytest
import drjit as dr
import mitsuba as mi


def test01_discr_empty(variants_all_backends_once):
    # Test that operations involving the empty distribution throw
    d = mi.DiscreteDistribution()
    assert d.empty()

    with pytest.raises(RuntimeError) as excinfo:
        d.update()
    assert 'empty distribution' in str(excinfo.value)


def test02_discr_zero_prob(variants_all_backends_once):
    # Test that operations involving zero probability mass throw
    with pytest.raises(RuntimeError) as excinfo:
        mi.DiscreteDistribution([0, 0, 0])
    assert "no probability mass found" in str(excinfo.value)


def test03_discr_neg_prob(variants_all_backends_once):
    # Test that operations involving negative probability mass throw
    with pytest.raises(RuntimeError) as excinfo:
        mi.DiscreteDistribution([1, -1, 1])
    assert "entries must be non-negative" in str(excinfo.value)


def test04_discr_basic(variants_vec_backends_once):
    # Validate discrete distribution cdf/pmf against hand-computed reference
    x = mi.DiscreteDistribution([1, 3, 2])
    assert len(x) == 3

    assert x.sum() == 6
    assert dr.allclose(x.normalization(), 1.0 / 6.0)
    assert dr.all(x.pmf == [1, 3, 2])
    assert dr.all(x.cdf == [1, 4, 6])
    assert dr.all(x.eval_pmf([1, 2, 0]) == [3, 2, 1])

    assert dr.allclose(
        x.eval_pmf_normalized([1, 2, 0]),
        mi.Float([3, 2, 1]) / 6.0
    )
    assert dr.allclose(
        x.eval_cdf_normalized([1, 2, 0]),
        mi.Float([4, 6, 1]) / 6.0
    )

    assert repr(x) == 'DiscreteDistribution[\n  size = 3,' \
        '\n  sum = [6],\n  pmf = [1, 3, 2]\n]'

    x.pmf = [1, 1, 1]
    x.update()
    assert dr.all(x.cdf == [1, 2, 3])
    assert dr.all(x.cdf == [1, 2, 3])
    assert x.sum() == 3
    assert dr.allclose(x.normalization(), 1.0 / 3.0)


def test05_discr_sample(variants_vec_backends_once):
    # Validate discrete distribution sampling against hand-computed reference
    eps = 1e-7

    x = mi.DiscreteDistribution([1, 3, 2])
    assert dr.all(x.sample([-1, 0, 1, 2]) == [0, 0, 2, 2])
    assert dr.all(x.sample([1 / 6.0 - eps, 1 / 6.0 + eps]) == [0, 1])
    assert dr.all(x.sample([4 / 6.0 - eps, 4 / 6.0 + eps]) == [1, 2])

    assert dr.allclose(
        x.sample_pmf([-1, 0, 1, 2]),
        ([0, 0, 2, 2], mi.Float([1, 1, 2, 2]) / 6)
    )

    assert dr.allclose(
        x.sample_pmf([1 / 6.0 - eps, 1 / 6.0 + eps]),
        ([0, 1], mi.Float([1, 3]) / 6)
    )

    assert dr.allclose(
        x.sample_pmf([4 / 6.0 - eps, 4 / 6.0 + eps]),
        ([1, 2], mi.Float([3, 2]) / 6)
    )

    assert dr.allclose(
        x.sample_reuse([0, 1 / 12.0, 1 / 6.0 - eps, 1 / 6.0 + eps]),
        ([0, 0, 0, 1], mi.Float([0, .5, 1, 0])),
        atol=3 * eps
    )

    assert dr.allclose(
        x.sample_reuse_pmf([0, 1 / 12.0, 1 / 6.0 - eps, 1 / 6.0 + eps]),
        ([0, 0, 0, 1], mi.Float([0, .5, 1, 0]), mi.Float([1, 1, 1, 3]) / 6),
        atol=3 * eps
    )


def test06_discr_bruteforce(variants_vec_backends_once):
    # Brute force validation of discrete distribution sampling, PCG32, UInt64
    rng = mi.PCG32(initseq=dr.arange(mi.UInt64, 50))

    for size in range(2, 20, 5):
        for i in range(2, 50, 5):
            density = dr.gather(mi.Float, mi.Float(rng.next_uint32_bounded(i)), 
                dr.arange(mi.UInt32, size))
            if dr.sum(density)[0] == 0:
                continue
            ddistr = mi.DiscreteDistribution(density)

            x = dr.linspace(mi.Float, 0, 1, 20)
            y = ddistr.sample(x)
            z = dr.gather(mi.Float, ddistr.cdf, y - 1, y > 0)
            x *= ddistr.sum()

            # Did we sample the right interval?
            assert dr.all((x > z) | ((x == 0) & (x >= z)))


def test07_discr_leading_trailing_zeros(variants_vec_backends_once):
    # Check that sampling still works when there are zero-valued buckets
    x = mi.DiscreteDistribution([0, 0, 1, 0, 1, 0, 0, 0])
    index, pmf = x.sample_pmf([-100, 0, 0.5, 0.5 + 1e-6, 1, 100])
    assert dr.all(index == [2, 2, 2, 4, 4, 4])
    assert dr.all(pmf == [.5] * 6)


def test08_cont_empty(variants_all_backends_once):
    # Test that operations involving the empty distribution throw
    d = mi.ContinuousDistribution()
    assert d.empty()
    d.range = [1, 2]

    with pytest.raises(RuntimeError) as excinfo:
        d.update()
    assert 'needs at least two entries' in str(excinfo.value)

    with pytest.raises(RuntimeError) as excinfo:
        mi.ContinuousDistribution([1, 2], [1])
    assert 'needs at least two entries' in str(excinfo.value)


def test09_cont_empty_invalid_range(variants_all_backends_once):
    # Test that invalid range specifications throw an exception
    with pytest.raises(RuntimeError) as excinfo:
        mi.ContinuousDistribution([1, 1], [1, 1])
    assert 'invalid range' in str(excinfo.value)

    with pytest.raises(RuntimeError) as excinfo:
        mi.ContinuousDistribution([2, 1], [1, 1])
    assert 'invalid range' in str(excinfo.value)


def test10_cont_zero_prob(variants_all_backends_once):
    # Test that operations involving zero probability mass throw
    with pytest.raises(RuntimeError) as excinfo:
        mi.ContinuousDistribution([1, 2], [0, 0, 0])
    assert "no probability mass found" in str(excinfo.value)


def test11_cont_neg_prob(variants_all_backends_once):
    # Test that operations involving negative probability mass throw
    with pytest.raises(RuntimeError) as excinfo:
        mi.ContinuousDistribution([1, 2], [1, -1, 1])
    assert "entries must be non-negative" in str(excinfo.value)


def test12_cont_eval(variants_vec_backends_once):
    # Test continuous 1D distribution pdf/cdf against hand-computed reference
    d = mi.ContinuousDistribution([2, 3], [1, 2])
    eps = 1e-6

    assert dr.allclose(d.max(), 2.0)
    assert dr.allclose(d.integral(), 3.0 / 2.0)
    assert dr.allclose(d.normalization(), 2.0 / 3.0)
    assert dr.allclose(
        d.eval_pdf_normalized([1, 2 - eps, 2, 2.5, 3, 3 + eps, 4]),
        [0, 0, 2.0 / 3.0, 1.0, 4.0 / 3.0, 0, 0]
    )
    assert dr.allclose(
        d.eval_cdf_normalized([1, 2, 2.5, 3, 4]),
        [0, 0, 5.0 / 12.0, 1, 1]
    )

    assert dr.all(d.sample([0, 1]) == [2, 3])
    x, pdf = d.sample_pdf([0, 0.5, 1])
    dx = (dr.sqrt(10) - 2) / 2
    assert dr.all(x == [2, 2 + dx, 3])
    assert dr.allclose(
        pdf,
        [2.0 / 3.0, (4 * dx + 2 * (1 - dx)) / 3.0, 4.0 / 3.0]
    )


def test13_cont_func(variants_vec_backends_once):
    # Test continuous 1D distribution integral against analytic result
    x = dr.linspace(mi.Float, -2, 2, 513)
    y = dr.exp(-dr.square(x))

    d = mi.ContinuousDistribution([-2, 2], y)
    assert dr.allclose(d.max(), 1.0)
    assert dr.allclose(d.integral(), dr.sqrt(dr.pi) * dr.erf(2.0))
    assert dr.allclose(d.eval_pdf([1]), [dr.exp(-1)])
    assert dr.allclose(d.sample([0, 0.5, 1]), [-2, 0, 2])


def test14_irrcont_empty(variants_all_backends_once):
    # Test that operations involving the empty distribution throw
    d = mi.IrregularContinuousDistribution()
    assert d.empty()

    with pytest.raises(RuntimeError) as excinfo:
        mi.IrregularContinuousDistribution([1], [1])
    assert 'needs at least two entries' in str(excinfo.value)

    with pytest.raises(RuntimeError) as excinfo:
        mi.IrregularContinuousDistribution([1, 2], [1])
    assert 'size mismatch' in str(excinfo.value)


def test15_irrcont_empty_invalid_range(variants_all_backends_once):
    # Test that invalid range specifications throw an exception
    with pytest.raises(RuntimeError) as excinfo:
        mi.IrregularContinuousDistribution([2, 1], [1, 1])
    assert 'strictly increasing' in str(excinfo.value)

    with pytest.raises(RuntimeError) as excinfo:
        mi.IrregularContinuousDistribution([1, 1], [1, 1])
    assert 'strictly increasing' in str(excinfo.value)


def test16_irrcont_zero_prob(variants_all_backends_once):
    # Test that operations involving the empty distribution throw
    with pytest.raises(RuntimeError) as excinfo:
        mi.IrregularContinuousDistribution([1, 2, 3], [0, 0, 0])
    assert "no probability mass found" in str(excinfo.value)


def test17_irrcont_neg_prob(variants_all_backends_once):
    # Test that operations involving negative probability mass throw
    with pytest.raises(RuntimeError) as excinfo:
        mi.IrregularContinuousDistribution([1, 2, 3], [1, -1, 1])
    assert "entries must be non-negative" in str(excinfo.value)


def test18_irrcont_simple_function(variants_vec_backends_once):
    # Reference from Mathematica, mi.Float
    d = mi.IrregularContinuousDistribution([1, 1.5, 1.8, 5], [1, 3, 0, 1])
    assert dr.allclose(d.max(), 3.0)
    assert dr.allclose(d.integral(), 3.05)
    assert dr.allclose(
        d.eval_pdf([0, 1, 2, 3, 4, 5, 6]),
        [0, 1, 0.0625, 0.375, 0.6875, 1, 0]
    )
    assert dr.allclose(
        d.eval_cdf([0, 1, 2, 3, 4, 5, 6]),
        [0, 0, 1.45625, 1.675, 2.20625, 3.05, 3.05]
    )

    assert dr.allclose(
        d.sample(dr.linspace(mi.Float, 0, 1, 11)),
        [1., 1.21368, 1.35622, 1.47111, 1.58552, 2.49282,
         3.35949, 3.8938, 4.31714, 4.67889, 5.]
    )

    assert dr.allclose(
        d.sample_pdf(dr.linspace(mi.Float, 0, 1, 11)),
        ([1., 1.21368, 1.35622, 1.47111, 1.58552, 2.49282,
          3.35949, 3.8938, 4.31714, 4.67889, 5.],
         mi.Float([1., 1.85472, 2.42487, 2.88444, 2.14476, 0.216506,
                0.48734, 0.654313, 0.786607, 0.899653, 1.])
         * d.normalization())
    )
