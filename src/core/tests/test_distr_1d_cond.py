import pytest
import drjit as dr
import mitsuba as mi

@pytest.fixture(params=['Float', 'UnpolarizedSpectrum'])
def type_str(request):
    '''
    Provides a fixture that generates tests for each type for testing
    '''
    yield request.param

def get_types(type_str, type_distr):
    '''
    Helper function to get the Mitsuba types based on the string provided by the fixture
    '''
    base = mi.Float if type_str == 'Float' else mi.UnpolarizedSpectrum
    distr_name = f'Conditional{type_distr}1D' + ('Spectrum' if type_str == 'UnpolarizedSpectrum' else '')
    if not hasattr(mi, distr_name):
        pytest.skip(f"{distr_name} not supported in this Mitsuba build")
    distr = getattr(mi, distr_name)
    return base, distr

def test01_conditional_irregular_empty(variants_vec_backends_once_spectral, type_str):
    _, distr_irregular = get_types(type_str, 'Irregular')
    # Test that operations involving the empty distribution throw
    d = distr_irregular()
    assert d.empty()


def test02_conditional_irregular_zero_prob(variants_vec_backends_once_spectral, type_str):
    _, distr_irregular = get_types(type_str, 'Irregular')

    if not 'scalar' in mi.variant():
        pytest.skip("Skip test for non-scalar variants as they do not throw")

    # Test that operations involving zero probability mass throw
    with pytest.raises(RuntimeError) as excinfo:
        distr_irregular(
            mi.Float([1, 2, 3, 4]),
            mi.Float([0, 0, 0, 0]),
            [mi.Float(0), mi.Float(1)],
        ).update()
    assert "no probability mass found" in str(excinfo.value)


def test03_conditional_irregular_neg_prob(variants_vec_backends_once_spectral, type_str):
    _, distr_irregular = get_types(type_str, 'Irregular')

    if not 'scalar' in mi.variant():
        pytest.skip("Skip test for non-scalar variants as they do not throw")

    # Test that operations involving negative probability mass throw
    with pytest.raises(RuntimeError) as excinfo:
        distr_irregular(
            mi.Float([1, 2, 3, 4]),
            mi.Float([0, -1, 0, 0]),
            [mi.Float(0), mi.Float(1)],
        ).update()
    assert "entries must be non-negative" in str(excinfo.value)


def test04_conditional_irregular_eval(variants_vec_backends_once_spectral, type_str):
    Type, distr_irregular = get_types(type_str, 'Irregular')
    d = distr_irregular(
        mi.Float(2, 3),
        dr.tile(mi.Float([1, 2]), 4),
        [mi.Float(1, 2), mi.Float(3, 4)],
    )

    d_tensor = distr_irregular(
        mi.Float(2, 3),
        mi.TensorXf(dr.tile(mi.Float([1, 2]), 4), (2,2,2)),
        [mi.Float(1, 2), mi.Float(3, 4)],
    )

    def execute_test(d):
        assert dr.allclose(d.nodes, mi.Float(2,3))
        assert dr.allclose(d.pdf.array, dr.tile(mi.Float([1,2]),4))
        assert dr.allclose(d.pdf.shape, (2,2,2))
        assert dr.allclose(d.nodes_cond[0], mi.Float(1,2))
        assert dr.allclose(d.nodes_cond[1], mi.Float(3,4))

        assert dr.allclose(d.max(), 2.0)
        assert dr.allclose(d.integral_array, dr.tile(3.0 / 2.0, 4))

        assert dr.allclose(
            d.eval_pdf_normalized(Type(mi.Float(2, 2.5, 3)), [Type(1.1), Type(3.98)]),
            Type(mi.Float(2.0 / 3.0, 1.0, 4.0 / 3.0)),
        )

        x, pdf = d.sample_pdf(Type(mi.Float(0, 0.5, 1)), [Type(1.1), Type(3.98)])
        dx = (dr.sqrt(10) - 2) / 2
        assert dr.allclose(x, Type(mi.Float(2, 2 + dx, 3)))
        assert dr.allclose(
            pdf,
            Type(mi.Float(2.0 / 3.0, (4 * dx + 2 * (1 - dx)) / 3.0, 4.0 / 3.0)),
        )

    execute_test(d)
    execute_test(d_tensor)


def test05_conditional_irregular_func(variants_vec_backends_once_spectral, type_str):
    import numpy as np
    Type, distr_irregular = get_types(type_str, 'Irregular')

    # Test continuous 1D distribution integral against analytic result
    x = dr.linspace(mi.Float, -2, 2, 513)
    y = dr.exp(-dr.square(x))

    d = distr_irregular(x, dr.tile(y, 4), [mi.Float(1, 2), mi.Float(3, 4)])
    d_tensor = distr_irregular(
        x, mi.TensorXf(dr.tile(y, 4), (2, 2, 513)), [mi.Float(1, 2), mi.Float(3, 4)]
    )

    def execute_test(d):
        assert dr.allclose(d.max(), 1.0)

        cdf_y = np.cumsum(
            0.5
            * (np.array(x)[1:] - np.array(x)[:-1])
            * (np.array(y)[1:] + np.array(y)[:-1])
        )
        assert dr.allclose(dr.tile(mi.Float(cdf_y), 4), d.cdf_array)
        assert dr.allclose(d.integral_array, dr.tile(dr.sqrt(dr.pi) * dr.erf(2.0), 4))
        assert dr.allclose(
            d.eval_pdf(
                Type(mi.Float(-1.34, 1, 1.78)), [Type(mi.Float(1, 1.5, 2)), Type(mi.Float(3, 3.25, 4))]
            ),
            Type(mi.Float(dr.exp(-dr.square(-1.34)), dr.exp(-1), dr.exp(-dr.square(1.78)))),
        )
        assert dr.allclose(
            d.sample_pdf(
                Type(mi.Float(0, 0.5, 1)), [Type(mi.Float(1, 1.25, 2)), Type(mi.Float(3, 3.5, 4))]
            )[0],
            Type(mi.Float([-2, 0, 2])),
        )

    execute_test(d)
    execute_test(d_tensor)

def test06_conditional_irregular_multiFunc(variants_vec_backends_once_spectral, type_str):
    import numpy as np
    Type, distr_irregular = get_types(type_str, 'Irregular')

    # This test generates random numbers to test the distribution doing interpolation
    rng = np.random.default_rng(seed=0xBADCAFE)

    SIZE = 100
    n_conditional = [5]
    total_conditionals = np.prod(n_conditional)
    x = np.linspace(0, 10, SIZE, endpoint=True)
    y = rng.random(size=SIZE * total_conditionals) * 1000.0

    d = distr_irregular(x, y, [mi.Float(0, 1, 2, 3, 4)])
    d_tensor = distr_irregular(
        x, mi.TensorXf(mi.Float(y), (5, SIZE)), [mi.Float(0, 1, 2, 3, 4)]
    )

    def execute_test(d):
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
                d.eval_pdf(Type(query_x), [Type(mi.Float(query_dims[i]))]), d_ref.eval_pdf(query_x)
            )

            assert dr.allclose(
                d.eval_pdf_normalized(Type(query_x), [Type(mi.Float(query_dims[i]))]),
                d_ref.eval_pdf_normalized(query_x),
            )

            x_d, pdf_d = d.sample_pdf(Type(u), [Type(mi.Float(query_dims[i]))])
            x_ref, pdf_ref = d_ref.sample_pdf(u)
            assert dr.allclose(x_d, x_ref)
            assert dr.allclose(pdf_d, pdf_ref)

    execute_test(d)
    execute_test(d_tensor)


def test07_conditional_regular_empty(variants_vec_backends_once_spectral, type_str):
    _ , distr_regular = get_types(type_str, 'Regular')
    # Test that operations involving the empty distribution throw
    d = distr_regular()
    assert d.empty()


def test08_conditional_regular_zero_prob(variants_vec_backends_once_spectral, type_str):
    _ , distr_regular = get_types(type_str, 'Regular')

    if not 'scalar' in mi.variant():
        pytest.skip("Skip test for non-scalar variants as they do not throw")

    # Test that operations involving zero probability mass throw
    with pytest.raises(RuntimeError) as excinfo:
        distr_regular(
            mi.Float([0, 0, 0, 0]),
            mi.ScalarVector2f(3, 8),
            [mi.ScalarVector2f(3, 4)],
            [2],
        ).update()
    assert "no probability mass found" in str(excinfo.value)


def test09_conditional_regular_neg_prob(variants_vec_backends_once_spectral, type_str):
    _ , distr_regular = get_types(type_str, 'Regular')

    if not 'scalar' in mi.variant():
        pytest.skip("Skip test for non-scalar variants as they do not throw")

    # Test that operations involving negative probability mass throw
    with pytest.raises(RuntimeError) as excinfo:
        distr_regular(
            mi.Float([2, -3, 1, 8]),
            mi.ScalarVector2f(3, 8),
            [mi.ScalarVector2f(3, 4)],
            [2],
        ).update()
    assert "entries must be non-negative" in str(excinfo.value)


def test10_conditional_regular_eval(variants_vec_backends_once_spectral, type_str):
    Type, distr_regular = get_types(type_str, 'Regular')
    d = distr_regular(
        dr.tile(mi.Float([1, 2]), 2),
        mi.ScalarVector2f(2, 3),
        [mi.ScalarVector2f(3, 4)],
        [2],
    )
    d_tensor = distr_regular(
        mi.TensorXf(dr.tile(mi.Float([1, 2]), 2), (2,2)),
        mi.ScalarVector2f(2, 3),
        [mi.ScalarVector2f(3, 4)]
    )

    def execute_test(d):
        assert dr.allclose(d.range, mi.ScalarVector2f(2,3))
        assert dr.allclose(d.pdf.array, dr.tile(mi.Float([1,2]),2))
        assert dr.allclose(d.pdf.shape, (2,2))
        assert dr.allclose(d.range_cond[0], mi.ScalarVector2f(3,4))

        assert dr.allclose(d.max(), 2.0)
        assert dr.allclose(d.integral_array, dr.tile(3.0 / 2.0, 2))

        assert dr.allclose(
            d.eval_pdf_normalized(Type(mi.Float([2, 2.5, 3])), [Type(mi.Float(3.98))]),
            Type(mi.Float([2.0 / 3.0, 1.0, 4.0 / 3.0])),
        )

        x, pdf = d.sample_pdf(Type(mi.Float([0, 0.5, 1])), [Type(mi.Float(3.98))])
        dx = (dr.sqrt(10) - 2) / 2
        assert dr.allclose(x, Type(mi.Float([2, 2 + dx, 3])))
        assert dr.allclose(
            pdf, Type(mi.Float([2.0 / 3.0, (4 * dx + 2 * (1 - dx)) / 3.0, 4.0 / 3.0]))
        )

    execute_test(d)
    execute_test(d_tensor)


def test11_conditional_regular_func(variants_vec_backends_once_spectral, type_str):
    import numpy as np
    Type , distr_regular = get_types(type_str, 'Regular')

    SIZE_X = 513
    x = dr.linspace(mi.Float, -2, 2, SIZE_X)
    y = dr.exp(-dr.square(x))

    d = distr_regular(
        dr.tile(y, 2),
        mi.ScalarVector2f(-2, 2),
        [mi.ScalarVector2f(1, 5)],
        [2],
    )
    d_tensor = distr_regular(
        mi.TensorXf(dr.tile(y, 2), (2, SIZE_X)),
        mi.ScalarVector2f(-2, 2),
        [mi.ScalarVector2f(1, 5)],
    )

    def execute_test(d):
        assert dr.allclose(d.max(), 1.0)

        cdf_y = np.cumsum(
            0.5
            * (np.array(x)[1:] - np.array(x)[:-1])
            * (np.array(y)[1:] + np.array(y)[:-1])
        )

        ref_cdf = dr.tile(mi.Float(cdf_y), 2)
        assert dr.allclose(ref_cdf, d.cdf_array)

        assert dr.allclose(d.integral_array, dr.tile(dr.sqrt(dr.pi) * dr.erf(2.0), 2))

        assert dr.allclose(
            d.eval_pdf(Type(mi.Float([-1.34, 1, 1.78])), [Type(mi.Float([1, 1.5, 2]))]),
            Type(mi.Float([dr.exp(-dr.square(-1.34)), dr.exp(-1), dr.exp(-dr.square(1.78))])),
        )
        assert dr.allclose(
            d.sample_pdf(Type(mi.Float([0, 0.5, 1])), [Type(mi.Float([1, 1.25, 2]))])[0],
            Type(mi.Float([-2, 0, 2])),
        )

    execute_test(d)
    execute_test(d_tensor)

def test12_conditional_regular_multiFunc(variants_vec_backends_once_spectral, type_str):
    import numpy as np
    Type , distr_regular = get_types(type_str, 'Regular')

    # This test generates random numbers to test the distribution doing interpolation
    rng = np.random.default_rng(seed=0xBADCAFE)

    SIZE = 100
    n_conditional = [5]
    total_conditionals = np.prod(n_conditional)
    x = np.linspace(0, 10, SIZE, endpoint=True)
    y = rng.random(size=SIZE * total_conditionals) * 1000.0

    d = distr_regular(
        mi.Float(y),
        mi.ScalarVector2f(0, 10),
        [mi.ScalarVector2f(0, 4)],
        n_conditional,
    )
    d_tensor = distr_regular(
        mi.TensorXf(mi.Float(y), (5, SIZE)),
        mi.ScalarVector2f(0, 10),
        [mi.ScalarVector2f(0, 4)],
    )

    def execute_test(d):
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
                d.eval_pdf(Type(query_x), [Type(mi.Float(query_dims[i]))]), d_ref.eval_pdf(query_x)
            )

            assert dr.allclose(
                d.eval_pdf_normalized(Type(query_x), [Type(mi.Float(query_dims[i]))]),
                d_ref.eval_pdf_normalized(query_x),
            )

            x_d, pdf_d = d.sample_pdf(Type(u), [Type(mi.Float(query_dims[i]))])
            x_ref, pdf_ref = d_ref.sample_pdf(u)
            assert dr.allclose(x_d, x_ref)
            assert dr.allclose(pdf_d, pdf_ref)

    execute_test(d)
    execute_test(d_tensor)

def test13_conditional_regular_irregular_same(variants_vec_backends_once_spectral, type_str):
    import numpy as np
    Type , distr_regular = get_types(type_str, 'Regular')
    _ , distr_irregular = get_types(type_str, 'Irregular')

    # Test that the regular and irregular versions of the distribution yield the same results
    rng = np.random.default_rng(seed=0xBADCAFE)

    SIZE = 100
    n_conditional = [5]
    total_conditionals = np.prod(n_conditional)
    x = np.linspace(0, 10, SIZE, endpoint=True)
    y = rng.random(size=SIZE * total_conditionals) * 1000.0

    d_regular = distr_regular(
        y, mi.ScalarVector2f(0, 10), [mi.ScalarVector2f(0, 4)], n_conditional
    )
    d_irregular = distr_irregular(x, y, [mi.Float(0, 1, 2, 3, 4)])

    conds = [Type(dr.linspace(mi.Float, 0, 4, 10))]
    x = Type(dr.linspace(mi.Float, 1, 4, 10))

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
    assert dr.allclose(d_irregular.integral_array, d_regular.integral_array)

    # Check that the CDFs are the same
    assert dr.allclose(d_irregular.cdf_array, d_regular.cdf_array)

    # Check that the maximum values are the same
    assert dr.allclose(d_irregular.max(), d_regular.max())


def test14_conditional_chi2_sampling(variants_vec_backends_once_spectral, type_str):
    from mitsuba.chi2 import ChiSquareTest, LineDomain
    import numpy as np
    Type , distr_regular = get_types(type_str, 'Regular')
    _ , distr_irregular = get_types(type_str, 'Irregular')

    def gaussian_pdf(x, mu=0.0, sigma=1.0):
        return (1.0 / (sigma * np.sqrt(2 * np.pi))) * np.exp(
            -0.5 * ((x - mu) / sigma) ** 2
        )

    def DistrAdapter(distr):
        def sample_functor(sample, *args):
            input = Type(mi.Float(sample[0]))
            cond = [Type(mi.Float(1.5)), Type(mi.Float(3.7))]
            res = distr.sample_pdf(input, cond, active=True)[0]
            if dr.depth_v(res) > 1:
                res = res[0]
            return mi.Vector1f(res)

        def pdf_functor(values, *args):
            input = Type(mi.Float(values))
            cond = [Type(mi.Float(1.5)), Type(mi.Float(3.7))]
            res = distr.eval_pdf_normalized(input, cond, active=True)
            if dr.depth_v(res) > 1:
                res = res[0]
            return res

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

    def execute_test(d):
        sample_functor, pdf_functor = DistrAdapter(d)
        test = ChiSquareTest(
            domain=LineDomain(bounds=BOUNDS),
            sample_func=sample_functor,
            pdf_func=pdf_functor,
            sample_dim=1,
        )
        assert test.run()

    d_regular = distr_regular(
        y,
        mi.ScalarVector2f(0, 10),
        [mi.ScalarVector2f(0, 4) for _ in range(len(n_conditional))],
        n_conditional,
    )
    execute_test(d_regular)

    d_regular_tensor = distr_regular(
        mi.TensorXf(mi.Float(y), (5, 5, SIZE)),
        mi.ScalarVector2f(0, 10),
        [mi.ScalarVector2f(0, 4) for _ in range(len(n_conditional))],
    )
    execute_test(d_regular_tensor)

    d_irregular = distr_irregular(
        x,
        y,
        [mi.Float(0, 1, 2, 3, 4) for _ in range(len(n_conditional))],
    )
    execute_test(d_irregular)
    d_irregular_tensor = distr_irregular(
        x,
        mi.TensorXf(mi.Float(y), (5, 5, SIZE)),
        [mi.Float(0, 1, 2, 3, 4) for _ in range(len(n_conditional))],
    )
    execute_test(d_irregular_tensor)

def test15_integral_irregular(variants_vec_backends_once_spectral, type_str):
    import numpy as np
    Type , distr_irregular = get_types(type_str, 'Irregular')

    SIZE = 10
    C = 5

    x = dr.arange(mi.Float, SIZE)
    y = dr.arange(mi.Float, SIZE * C)

    d = distr_irregular(
        x, y, [mi.Float(0, 1, 2, 3, 4)]
    )
    d_tensor = distr_irregular(
        x, mi.TensorXf(y, (5, SIZE)), [mi.Float(0, 1, 2, 3, 4)]
    )

    def execute_test(d):
        # Test integral for perfect conditional
        INDEX = 2
        value = d.integral([Type(dr.gather(mi.Float, x, mi.UInt32(INDEX), True))])
        ref = np.trapz(y.numpy()[SIZE*INDEX:SIZE*(INDEX+1)], x.numpy())
        assert dr.allclose(value, ref)

        # Test integral for interpolated conditional
        INDEX = 2.87
        value = d.integral([Type(mi.Float(INDEX))])
        l_i = np.floor(INDEX).astype(int)
        w_i = INDEX - l_i
        ref = (
            (1.0 - w_i) * np.trapz(y=y.numpy()[SIZE*l_i:SIZE*(l_i+1)], x=x.numpy()) +
            w_i * np.trapz(y=y.numpy()[SIZE*(l_i+1):SIZE*(l_i+2)], x=x.numpy())
        )
        assert dr.allclose(value, ref)

    execute_test(d)
    execute_test(d_tensor)

def test16_integral_regular(variants_vec_backends_once_spectral, type_str):
    import numpy as np
    Type , distr_regular = get_types(type_str, 'Regular')

    SIZE = 10
    C = 5

    y = dr.arange(mi.Float, SIZE * C)

    d = distr_regular(
        y,
        mi.ScalarVector2f(0, SIZE - 1),
        [mi.ScalarVector2f(0, C - 1)],
        [C],
    )
    d_tensor = distr_regular(
        mi.TensorXf(y, (5, SIZE)),
        mi.ScalarVector2f(0, SIZE - 1),
        [mi.ScalarVector2f(0, C - 1)],
    )

    def execute_test(d):
        # Test integral for perfect conditional
        INDEX = 2
        value = d.integral([Type(dr.gather(mi.Float, dr.linspace(mi.Float, 0, C-1, C), mi.UInt32(INDEX), True))])
        ref = np.trapz(y.numpy()[SIZE*INDEX:SIZE*(INDEX+1)], np.linspace(0, SIZE-1, SIZE))
        assert dr.allclose(value, ref)

        # Test integral for interpolated conditional
        INDEX = 2.87
        value = d.integral([Type(mi.Float(INDEX))])
        l_i = np.floor(INDEX).astype(int)
        w_i = INDEX - l_i
        ref = (
            (1.0 - w_i) * np.trapz(y=y.numpy()[SIZE*l_i:SIZE*(l_i+1)], x=np.linspace(0, SIZE-1, SIZE)) +
            w_i * np.trapz(y=y.numpy()[SIZE*(l_i+1):SIZE*(l_i+2)], x=np.linspace(0, SIZE-1, SIZE))
        )
        assert dr.allclose(value, ref)

    execute_test(d)
    execute_test(d_tensor)
