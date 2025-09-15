import pytest
import drjit as dr
import mitsuba as mi


def test01_conditional_irregular_empty(variants_vec_backends_once_spectral):
    # Test that operations involving the empty distribution throw
    d = mi.ConditionalIrregular1D()
    assert d.empty()

    with pytest.raises(RuntimeError) as excinfo:
        d.update()
    assert "needs at least two entries" in str(excinfo.value)


def test02_conditional_irregular_zero_prob(variants_vec_backends_once_spectral):
    # Test that operations involving zero probability mass throw
    with pytest.raises(RuntimeError) as excinfo:
        mi.ConditionalIrregular1D(
            mi.Float([1, 2, 3, 4]),
            mi.Float([0, 0, 0, 0]),
            [mi.Float(0), mi.Float(1)],
        )
    assert "no probability mass found" in str(excinfo.value)


def test03_conditional_irregular_neg_prob(variants_vec_backends_once_spectral):
    # Test that operations involving negative probability mass throw
    with pytest.raises(RuntimeError) as excinfo:
        mi.ConditionalIrregular1D(
            mi.Float([1, 2, 3, 4]),
            mi.Float([0, -1, 0, 0]),
            [mi.Float(0), mi.Float(1)],
        )
    assert "entries must be non-negative" in str(excinfo.value)


def test04_conditional_irregular_eval(variants_vec_backends_once_spectral):
    d = mi.ConditionalIrregular1D(
        mi.Float(2, 3),
        dr.tile(mi.Float([1, 2]), 4),
        [mi.Float(1, 2), mi.Float(3, 4)],
    )
    eps = 1e-6

    assert dr.allclose(d.max(), 2.0)
    assert dr.allclose(d.m_integral, dr.tile(3.0 / 2.0, 4))

    assert dr.allclose(
        d.eval_pdf_normalized(mi.Float([2, 2.5, 3]), [mi.Float(1.1), mi.Float(3.98)]),
        mi.Float([2.0 / 3.0, 1.0, 4.0 / 3.0]),
    )

    x, pdf = d.sample_pdf(mi.Float([0, 0.5, 1]), [mi.Float(1.1), mi.Float(3.98)])
    dx = (dr.sqrt(10) - 2) / 2
    assert dr.allclose(x, mi.UnpolarizedSpectrum(mi.Float([2, 2 + dx, 3])))
    assert dr.allclose(
        pdf,
        mi.UnpolarizedSpectrum(mi.Float([2.0 / 3.0, (4 * dx + 2 * (1 - dx)) / 3.0, 4.0 / 3.0])),
    )


def test05_conditional_irregular_func(variants_vec_backends_once_spectral):
    import numpy as np

    # Test continuous 1D distribution integral against analytic result
    x = dr.linspace(mi.Float, -2, 2, 513)
    y = dr.exp(-dr.square(x))

    d = mi.ConditionalIrregular1D(x, dr.tile(y, 4), [mi.Float(1, 2), mi.Float(3, 4)])
    assert dr.allclose(d.max(), 1.0)

    cdf_y = np.cumsum(
        0.5
        * (np.array(x)[1:] - np.array(x)[:-1])
        * (np.array(y)[1:] + np.array(y)[:-1])
    )
    assert dr.allclose(dr.tile(mi.Float(cdf_y), 4), d.m_cdf)
    assert dr.allclose(d.m_integral, dr.tile(dr.sqrt(dr.pi) * dr.erf(2.0), 4))
    assert dr.allclose(
        d.eval_pdf(
            mi.Float([-1.34, 1, 1.78]), [mi.Float([1, 1.5, 2]), mi.Float([3, 3.25, 4])]
        ),
        mi.Float([dr.exp(-dr.square(-1.34)), dr.exp(-1), dr.exp(-dr.square(1.78))]),
    )
    assert dr.allclose(
        d.sample_pdf(
            mi.Float([0, 0.5, 1]), [mi.Float([1, 1.25, 2]), mi.Float([3, 3.5, 4])]
        )[0],
        mi.Float([-2, 0, 2]),
    )


def test06_conditional_irregular_multiFunc(variants_vec_backends_once_spectral):
    import numpy as np

    # This test generates random numbers to test the distribution doing interpolation
    rng = np.random.default_rng(seed=0xBADCAFE)

    SIZE = 100
    n_conditional = [5]
    total_conditionals = np.prod(n_conditional)
    x = np.linspace(0, 10, SIZE, endpoint=True)
    y = rng.random(size=SIZE * total_conditionals) * 1000.0

    d = mi.ConditionalIrregular1D(x, y, [mi.Float(0, 1, 2, 3, 4)])

    # Launch an evaluation and compare
    u = rng.random(size=10)
    u = mi.Float(u)

    query_dims = rng.random(size=10) * 4
    for i in range(query_dims.size):
        query_x = rng.random(size=100) * 10
        query_x = mi.Float(query_x)

        floor_dim = np.floor(query_dims[i]).astype(int)
        w1 = (query_dims[i] - floor_dim) / (floor_dim + 1 - floor_dim)
        w0 = 1.0 - w1

        data_d0 = y[floor_dim * SIZE : (floor_dim + 1) * SIZE]
        data_d1 = y[(floor_dim + 1) * SIZE : (floor_dim + 2) * SIZE]

        d_ref = mi.IrregularContinuousDistribution(
            x,
            w0 * data_d0 + w1 * data_d1,
        )

        assert dr.allclose(
            d.eval_pdf(query_x, [mi.Float(query_dims[i])]), d_ref.eval_pdf(query_x)
        )

        assert dr.allclose(
            d.eval_pdf_normalized(query_x, [mi.Float(query_dims[i])]),
            d_ref.eval_pdf_normalized(query_x),
        )

        x_d, pdf_d = d.sample_pdf(u, [mi.Float(query_dims[i])])
        x_ref, pdf_ref = d_ref.sample_pdf(u)
        assert dr.allclose(x_d, x_ref)
        assert dr.allclose(pdf_d, pdf_ref)


def test07_conditional_regular_empty(variants_vec_backends_once_spectral):
    # Test that operations involving the empty distribution throw
    d = mi.ConditionalRegular1D()
    assert d.empty()

    with pytest.raises(RuntimeError) as excinfo:
        d.update()
    assert "needs at least two entries" in str(excinfo.value)


def test08_conditional_regular_zero_prob(variants_vec_backends_once_spectral):
    # Test that operations involving zero probability mass throw
    with pytest.raises(RuntimeError) as excinfo:
        mi.ConditionalRegular1D(
            mi.Float([0, 0, 0, 0]),
            mi.ScalarVector2f(3, 8),
            [mi.ScalarVector2f(3, 4)],
            [2],
        )
    assert "no probability mass found" in str(excinfo.value)


def test09_conditional_regular_neg_prob(variants_vec_backends_once_spectral):
    # Test that operations involving negative probability mass throw
    with pytest.raises(RuntimeError) as excinfo:
        mi.ConditionalRegular1D(
            mi.Float([2, -3, 1, 8]),
            mi.ScalarVector2f(3, 8),
            [mi.ScalarVector2f(3, 4)],
            [2],
        )
    assert "entries must be non-negative" in str(excinfo.value)


def test10_conditional_regular_eval(variants_vec_backends_once_spectral):
    d = mi.ConditionalRegular1D(
        dr.tile(mi.Float([1, 2]), 2),
        mi.ScalarVector2f(2, 3),
        [mi.ScalarVector2f(3, 4)],
        [2],
    )
    eps = 1e-6

    # assert dr.allclose(d.max(), 2.0)
    assert dr.allclose(d.m_integral, dr.tile(3.0 / 2.0, 2))

    assert dr.allclose(
        d.eval_pdf_normalized(mi.Float([2, 2.5, 3]), [mi.Float(3.98)]),
        mi.Float([2.0 / 3.0, 1.0, 4.0 / 3.0]),
    )

    x, pdf = d.sample_pdf(mi.Float([0, 0.5, 1]), [mi.Float(3.98)])
    dx = (dr.sqrt(10) - 2) / 2
    assert dr.allclose(x, mi.Float([2, 2 + dx, 3]))
    assert dr.allclose(
        pdf, mi.Float([2.0 / 3.0, (4 * dx + 2 * (1 - dx)) / 3.0, 4.0 / 3.0])
    )


def test11_conditional_regular_func(variants_vec_backends_once_spectral):
    import numpy as np

    SIZE_X = 513
    x = dr.linspace(mi.Float, -2, 2, SIZE_X)
    y = dr.exp(-dr.square(x))

    d = mi.ConditionalRegular1D(
        dr.tile(y, 2),
        mi.ScalarVector2f(-2, 2),
        [mi.ScalarVector2f(1, 5)],
        [2],
    )

    assert dr.allclose(d.max(), 1.0)

    cdf_y = np.cumsum(
        0.5
        * (np.array(x)[1:] - np.array(x)[:-1])
        * (np.array(y)[1:] + np.array(y)[:-1])
    )

    ref_cdf = dr.tile(mi.Float(cdf_y), 2)
    assert dr.allclose(ref_cdf, d.m_cdf)

    assert dr.allclose(d.m_integral, dr.tile(dr.sqrt(dr.pi) * dr.erf(2.0), 2))

    assert dr.allclose(
        d.eval_pdf(mi.Float([-1.34, 1, 1.78]), [mi.Float([1, 1.5, 2])]),
        mi.Float([dr.exp(-dr.square(-1.34)), dr.exp(-1), dr.exp(-dr.square(1.78))]),
    )
    assert dr.allclose(
        d.sample_pdf(mi.Float([0, 0.5, 1]), [mi.Float([1, 1.25, 2])])[0],
        mi.Float([-2, 0, 2]),
    )


def test12_conditional_regular_multiFunc(variants_vec_backends_once_spectral):
    import numpy as np

    # This test generates random numbers to test the distribution doing interpolation
    rng = np.random.default_rng(seed=0xBADCAFE)

    SIZE = 100
    n_conditional = [5]
    total_conditionals = np.prod(n_conditional)
    x = np.linspace(0, 10, SIZE, endpoint=True)
    y = rng.random(size=SIZE * total_conditionals) * 1000.0

    d = mi.ConditionalRegular1D(
        mi.Float(y),
        mi.ScalarVector2f(0, 10),
        [mi.ScalarVector2f(0, 4)],
        n_conditional,
    )

    # Launch an evaluation and compare
    u = rng.random(size=10)
    u = mi.Float(u)
    query_dims = rng.random(size=10) * 4
    for i in range(query_dims.size):
        query_x = mi.Float(rng.random(size=100) * 10)

        floor_dim = np.floor(query_dims[i]).astype(int)
        w1 = (query_dims[i] - floor_dim) / (floor_dim + 1 - floor_dim)
        w0 = 1.0 - w1

        data_d0 = y[floor_dim * SIZE : (floor_dim + 1) * SIZE]
        data_d1 = y[(floor_dim + 1) * SIZE : (floor_dim + 2) * SIZE]

        d_ref = mi.IrregularContinuousDistribution(
            x,
            w0 * data_d0 + w1 * data_d1,
        )

        assert dr.allclose(
            d.eval_pdf(query_x, [mi.Float(query_dims[i])]), d_ref.eval_pdf(query_x)
        )

        assert dr.allclose(
            d.eval_pdf_normalized(query_x, [mi.Float(query_dims[i])]),
            d_ref.eval_pdf_normalized(query_x),
        )

        x_d, pdf_d = d.sample_pdf(u, mi.Float(query_dims[i]))
        x_ref, pdf_ref = d_ref.sample_pdf(u)
        assert dr.allclose(x_d, x_ref)
        assert dr.allclose(pdf_d, pdf_ref)


def test13_conditional_regular_irregular_same(variants_vec_backends_once_spectral):
    import numpy as np

    # Test that the regular and irregular versions of the distribution yield the same results
    rng = np.random.default_rng(seed=0xBADCAFE)

    SIZE = 100
    n_conditional = [5]
    total_conditionals = np.prod(n_conditional)
    x = np.linspace(0, 10, SIZE, endpoint=True)
    y = rng.random(size=SIZE * total_conditionals) * 1000.0

    d_regular = mi.ConditionalRegular1D(
        y, mi.ScalarVector2f(0, 10), [mi.ScalarVector2f(0, 4)], n_conditional
    )
    d_irregular = mi.ConditionalIrregular1D(x, y, [mi.Float(0, 1, 2, 3, 4)])

    conds = [dr.linspace(mi.Float, 0, 4, 10)]
    x = dr.linspace(mi.Float, 1, 4, 10)

    assert dr.allclose(
        d_irregular.eval_pdf_normalized(x, conds),
        d_regular.eval_pdf_normalized(x, conds),
    )
    assert dr.allclose(
        d_irregular.sample_pdf(mi.Float([0.5]), conds)[0],
        d_regular.sample_pdf(mi.Float([0.5]), conds)[0],
    )
    assert dr.allclose(
        d_irregular.sample_pdf(mi.Float([0.5]), conds)[1],
        d_regular.sample_pdf(mi.Float([0.5]), conds)[1],
    )

    # Check that the normalization factors are the same
    assert dr.allclose(d_irregular.m_integral, d_regular.m_integral)

    # Check that the CDFs are the same
    assert dr.allclose(d_irregular.m_cdf, d_regular.m_cdf)

    # Check that the maximum values are the same
    assert dr.allclose(d_irregular.max(), d_regular.max())


def test14_conditional_chi2_sampling(variants_vec_backends_once_spectral):
    from mitsuba.chi2 import ChiSquareTest, LineDomain
    import numpy as np

    def gaussian_pdf(x, mu=0.0, sigma=1.0):
        return (1.0 / (sigma * np.sqrt(2 * np.pi))) * np.exp(
            -0.5 * ((x - mu) / sigma) ** 2
        )

    def DistrAdapter(distr):
        def sample_functor(sample, *args):
            input = mi.UnpolarizedSpectrum(sample[0])
            cond = [mi.Float(1.5), mi.Float(3.7)]
            res = distr.sample_pdf(input, cond, active=True)
            return mi.Vector1f(res[0][0])

        def pdf_functor(values, *args):
            input = mi.UnpolarizedSpectrum(values)
            cond = [mi.Float(1.5), mi.Float(3.7)]
            res = distr.eval_pdf_normalized(input, cond, active=True)
            return res[0]

        return sample_functor, pdf_functor

    n_conditional = [5, 5]
    # n_conditional = [5]
    total_conditionals = np.prod(n_conditional)
    BOUNDS = (0, 10)
    SIZE = 50
    MU = 5.0
    SIGMA = 3.0
    x = np.linspace(BOUNDS[0], BOUNDS[1], SIZE, endpoint=True)
    y = np.hstack(
        [gaussian_pdf(x, MU, SIGMA - i * 0.25) for i in range(n_conditional[0])]
    )
    y = np.tile(y, n_conditional[1])
    assert y.shape[0] == SIZE * total_conditionals

    d_regular = mi.ConditionalRegular1D(
        y,
        mi.ScalarVector2f(0, 10),
        [mi.ScalarVector2f(0, 4) for _ in range(len(n_conditional))],
        n_conditional,
        enable_sampling=True,
    )
    sample_functor, pdf_functor = DistrAdapter(d_regular)

    test = ChiSquareTest(
        domain=LineDomain(bounds=BOUNDS),
        sample_func=sample_functor,
        pdf_func=pdf_functor,
        sample_dim=1,
    )
    assert test.run()

    d_irregular = mi.ConditionalIrregular1D(
        x,
        y,
        [mi.Float(0, 1, 2, 3, 4) for _ in range(len(n_conditional))],
        enable_sampling=True,
    )
    sample_functor, pdf_functor = DistrAdapter(d_irregular)

    test = ChiSquareTest(
        domain=LineDomain(bounds=BOUNDS),
        sample_func=sample_functor,
        pdf_func=pdf_functor,
        sample_dim=1,
        res=401,
        ires=16,
    )
    assert test.run()


def test15_neareast_irregular(variants_vec_backends_once_spectral):
    import numpy as np

    SIZE = 10
    C = 5

    x = dr.arange(mi.Float, SIZE)
    y = dr.arange(mi.Float, SIZE * C)

    d = mi.ConditionalIrregular1D(
        x, y, [mi.Float(0, 1, 2, 3, 4)], enable_sampling=True, nearest=True
    )
    value = d.eval_pdf(mi.Float(np.arange(0, 4, 0.33)), [mi.Float(0.45)])

    ref = mi.Float(0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4)
    assert dr.allclose(value, ref)


def test16_neareast_regular(variants_vec_backends_once_spectral):
    import numpy as np

    SIZE = 10
    C = 5

    y = dr.arange(mi.Float, SIZE * C)

    d = mi.ConditionalRegular1D(
        y,
        mi.ScalarVector2f(0, SIZE - 1),
        [mi.ScalarVector2f(0, C - 1)],
        [C],
        enable_sampling=True,
        nearest=True,
    )
    value = d.eval_pdf(mi.Float(np.arange(0, 4, 0.33)), [mi.Float(0.45)])

    ref = mi.Float(0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4)
    assert dr.allclose(value, ref)

def test17_integral_irregular(variants_vec_backends_once_spectral):
    import numpy as np

    SIZE = 10
    C = 5

    x = dr.arange(mi.Float, SIZE)
    y = dr.arange(mi.Float, SIZE * C)

    d = mi.ConditionalIrregular1D(
        x, y, [mi.Float(0, 1, 2, 3, 4)], enable_sampling=True
    )

    # Test integral for perfect conditional
    INDEX = 2
    value = d.integral([dr.gather(mi.Float, x, mi.UInt32(INDEX), True)])
    ref = np.trapz(y.numpy()[SIZE*INDEX:SIZE*(INDEX+1)], x.numpy())
    assert dr.allclose(value, ref)

    # Test integral for interpolated conditional
    INDEX = 2.87
    value = d.integral([mi.Float(INDEX)])
    l_i = np.floor(INDEX).astype(int)
    w_i = INDEX - l_i
    ref = (
        (1.0 - w_i) * np.trapz(y=y.numpy()[SIZE*l_i:SIZE*(l_i+1)], x=x.numpy()) +
        w_i * np.trapz(y=y.numpy()[SIZE*(l_i+1):SIZE*(l_i+2)], x=x.numpy())
    )
    assert dr.allclose(value, ref)

def test18_integral_regular(variants_vec_backends_once_spectral):
    import numpy as np

    SIZE = 10
    C = 5

    y = dr.arange(mi.Float, SIZE * C)

    d = mi.ConditionalRegular1D(
        y,
        mi.ScalarVector2f(0, SIZE - 1),
        [mi.ScalarVector2f(0, C - 1)],
        [C],
        enable_sampling=True,
    )

    # Test integral for perfect conditional
    INDEX = 2
    value = d.integral([dr.gather(mi.Float, dr.linspace(mi.Float, 0, C-1, C), mi.UInt32(INDEX), True)])
    ref = np.trapz(y.numpy()[SIZE*INDEX:SIZE*(INDEX+1)], np.linspace(0, SIZE-1, SIZE))
    assert dr.allclose(value, ref)

    # Test integral for interpolated conditional
    INDEX = 2.87
    value = d.integral([mi.Float(INDEX)])
    l_i = np.floor(INDEX).astype(int)
    w_i = INDEX - l_i
    ref = (
        (1.0 - w_i) * np.trapz(y=y.numpy()[SIZE*l_i:SIZE*(l_i+1)], x=np.linspace(0, SIZE-1, SIZE)) +
        w_i * np.trapz(y=y.numpy()[SIZE*(l_i+1):SIZE*(l_i+2)], x=np.linspace(0, SIZE-1, SIZE))
    )
    assert dr.allclose(value, ref)
