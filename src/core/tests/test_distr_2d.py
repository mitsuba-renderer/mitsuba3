import pytest
import drjit as dr
import mitsuba as mi
import numpy as np


@pytest.mark.parametrize("normalize", [True, False])
@pytest.mark.parametrize("warp", ['Hierarchical2D0', 'MarginalDiscrete2D0'])
def test01_sample_inverse_discrete(variants_all_backends_once, warp, normalize):
    # Spot checks of Hierarchial2D0/MarginalDiscrete2D0 vs Mathematica

    def allclose(a, b):
        return dr.allclose(a, b, atol=1e-6)

    # Mismatched X/Y resolution, odd number of columns
    ref = np.array([[1, 2, 5],
                    [9, 7, 2]])

    # Normalized discrete PDF corresponding to the two patches
    intg = np.array([19, 16]) / 35

    warp = getattr(mi, warp)
    distr = warp(ref, normalize=normalize)

    intg0 = float(intg[0])

    s = 35 / 8.0 if not normalize else 1
    # Check if we can reach the corners and transition between patches
    assert allclose(distr.sample([0, 0]), [[0, 0], s * 8.0 / 35.0])
    assert allclose(distr.sample([1, 1]), [[1, 1], s * 16.0 / 35.0])
    assert allclose(distr.sample([intg0, 0]), [[0.5, 0], s * 16.0 / 35.0])
    assert allclose(distr.invert([0, 0]), [[0, 0], s * 8.0 / 35.0])
    assert allclose(distr.invert([1, 1]), [[1, 1], s * 16.0 / 35.0])
    assert allclose(distr.invert([0.5, 0]), [[intg0, 0], s * 16.0 / 35.0])

    # Check if we can sample a specific position in each patch
    sample, pdf = mi.warp.bilinear_to_square(1, 2, 9, 7, [0.4, 0.3])
    sample.x *= intg[0]
    pdf *= 8.0 / 35.0 * s
    assert allclose(distr.sample(sample), [[0.2, 0.3], pdf])
    assert allclose(distr.invert([0.2, 0.3]), [sample, pdf])
    assert allclose(distr.eval([0.2, 0.3]), pdf)

    sample, pdf = mi.warp.bilinear_to_square(2, 5, 7, 2, [0.4, 0.3])
    sample.x = sample.x * intg[1] + intg[0]
    pdf *= 8.0 / 35.0 * s
    assert allclose(distr.sample(sample), [[0.7, 0.3], pdf])
    assert allclose(distr.invert([0.7, 0.3]), [sample, pdf])
    assert allclose(distr.eval([0.7, 0.3]), pdf)


@pytest.mark.parametrize("normalize", [True, False])
def test02_sample_inverse_continuous(variants_all_backends_once, normalize):
    # Spot checks of MarginalContinuous2D0 vs Mathematica

    # Nonuniform case
    ref = np.array([[1, 2, 5],
                    [9, 7, 2],
                    [1, 0, 5]])

    distr = mi.MarginalContinuous2D0(ref, normalize=normalize)
    s = 1 if normalize else 4.125
    assert dr.allclose(distr.eval([0, 0]), s / 4.125)
    ref = [[.162433, .330829], s * 1.44807]
    assert dr.allclose(distr.sample([0.2, 0.3]), ref)
    assert dr.allclose(distr.invert(ref[0]), [[0.2, 0.3], ref[1]])
    ref = [[.8277673, .8235800], s * 0.8326217]
    assert dr.allclose(distr.sample([0.8, 0.9]), ref, atol=1e-5)
    assert dr.allclose(distr.invert(ref[0]), [[0.8, 0.9], ref[1]])

    # Uniform case
    ref = np.array([[1, 1, 1],
                    [1, 1, 1],
                    [1, 1, 1]])

    distr = mi.MarginalContinuous2D0(ref)
    assert dr.allclose(distr.eval([0, 0]), 1.0)
    assert dr.allclose(distr.sample([0.2, 0.3]),
                       [[.2, .3], 1.0])
    assert dr.allclose(distr.invert([0.2, 0.3]),
                       [[.2, .3], 1.0])
    assert dr.allclose(distr.sample([0.8, 0.9]),
                       [[.8, .9], 1.0])
    assert dr.allclose(distr.invert([0.8, 0.9]),
                       [[.8, .9], 1.0])


all_warps = [w + str(i) for w in
             ['Hierarchical2D', 'MarginalDiscrete2D', 'MarginalContinuous2D']
             for i in range(4)]


@pytest.mark.parametrize("normalize", [True, False])
@pytest.mark.parametrize("warp", all_warps)
def test03_forward_inverse_nd(variant_scalar_rgb, warp, normalize):
    # Check that forward + inverse give the identity. Test for
    # all supported variants of N-dimensional warps (N = 0..3)

    cls = getattr(mi, warp)
    ndim = int(warp[-1]) + 2
    rng = np.random.default_rng(seed=all_warps.index(warp))

    for i in range(10):
        shape = rng.integers(2, 8, ndim)
        param_res = [
            sorted(rng.random(s)) for s in shape[:-2]
        ]
        values = rng.random(shape) * 10
        if i == 9:
            values = np.ones(shape)
        instance = cls(values, param_res, normalize=normalize)

        for j in range(10):
            p_i = mi.Vector2f(rng.random(2))
            p_o, pdf = instance.sample(p_i)
            assert dr.allclose(pdf, instance.eval(p_o), atol=1e-4)
            p_i_2, pdf2 = instance.invert(p_o)
            assert dr.allclose(pdf, pdf2, atol=1e-4)
            assert dr.allclose(p_i_2, p_i, atol=1e-4)


@pytest.mark.parametrize("attempt", list(range(2)))
@pytest.mark.parametrize("warp", all_warps)
def test04_chi2(variants_vec_backends_once, warp, attempt):
    # Chi^2 test to make sure that the warping schemes are producing
    # the right distribution. Test for all supported variants of
    # N-dimensional warps (N = 0..3)

    cls = getattr(mi, warp)
    ndim = int(warp[-1]) + 2
    rng = np.random.default_rng(seed=all_warps.index(warp) * 10 + attempt)

    shape = rng.integers(2, 8, ndim)
    param_res = [
        sorted(rng.random(s)) for s in shape[:-2]
    ]

    if attempt == 9:
        values = np.ones(shape)
    else:
        values = rng.random(shape) * 10

    instance = cls(values, param_res)

    for j in range(10 if ndim > 2 else 1):
        param = [
            dr.lerp(
                param_res[i][0],
                param_res[i][-1],
                rng.random()
            ) for i in range(0, ndim - 2)
        ]

        chi2 = mi.chi2.ChiSquareTest(
            domain=mi.chi2.PlanarDomain(mi.ScalarBoundingBox2f(0, 1)),
            sample_func=lambda p: instance.sample(p, param=param)[0],
            pdf_func=lambda p: instance.eval(p, param=param),
            sample_dim=2,
            res=31,
            sample_count=100000
        )

        assert chi2.run(
            test_count=11 * len(all_warps)
        )


def test05_discrete_distribution_2d(variants_all_backends_once):
    import numpy as np

    d = mi.DiscreteDistribution2D(np.array([[1, 2, 3], [0, 1, 3]], dtype=np.float32))

    def allclose(a, b):
        return dr.allclose(a, b, atol=1e-6)

    assert allclose(d.sample([0, 0]), ([0, 0], .1, [0, 0]))
    assert allclose(d.sample([1.0 / 6.0 - 1e-7, 0]), ([0, 0], .1, [1, 0]))
    assert allclose(d.sample([1.0 / 6.0 + 1e-7, 0]), ([1, 0], .2, [0, 0]))
    assert allclose(d.sample([1, 0]), ([2, 0], .3, [1, 0]))
    assert allclose(d.sample([0, 6 / 10 - 1e-7]), ([0, 0], .1, [0, 1]))
    assert allclose(d.sample([0, 6 / 10 + 1e-7]), ([1, 1], .1, [0, 0]))
