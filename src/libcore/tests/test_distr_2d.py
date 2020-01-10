import mitsuba
import pytest
import enoki as ek
import numpy as np

@pytest.fixture()
def variant_scalar():
    mitsuba.set_variant('scalar_rgb')

@pytest.fixture()
def variant_packet():
    try:
        mitsuba.set_variant('packet_rgb')
    except ImportError:
        pytest.skip("packet_rgb mode not enabled")

@pytest.fixture(params = ['packet_rgb', 'gpu_rgb'])
def variants(request):
    try:
        mitsuba.set_variant(request.param)
    except:
        pytest.skip("%s mode not enabled" % request.param)
    return request.param

@pytest.fixture(params = ['Hierarchical2D', 'Marginal2DDiscrete', 'Marginal2DContinuous'])
# @pytest.fixture(params = ['Hierarchical2D'])
def warps(request):
    return [ getattr(mitsuba.core, request.param + str(i)) for i in range(4) ]


# Create array of 2D samples to test implementations of the sample, eval and invert methods.
sample_res = 4
x, y = np.meshgrid(np.linspace(0.0, 1.0, sample_res), np.linspace(0.0, 1.0, sample_res))
samples = np.array([x.ravel(), y.ravel()]).T
samples_cout = sample_res**2


def test01_constructors(variants, warps):
    assert warps[0](np.ones((16, 16)),          [])
    assert warps[1](np.ones((4, 16, 16)),       [np.arange(4)])
    assert warps[2](np.ones((4, 4, 16, 16)),    [np.arange(4), np.arange(4)])
    assert warps[3](np.ones((4, 4, 4, 16, 16)), [np.arange(4), np.arange(4), np.arange(4)])


def test02_interpolation_eval(variants, warps):
    data_res = 8
    # Build a data grid of shape [2 x res x res]. The first level is set to a ramp with values going
    # from 0.0 to 1.0. The second level is set to a ramp with values going from 1.0 to 0.0. This way,
    # the linear interpolation of both level at [0.0, :, :] is a constant 2D grid.
    ramp, _ = np.meshgrid(np.linspace(0.0, 1.0, data_res), np.ones(data_res))
    distr = warps[1](np.array([ramp, 1.0 - ramp]), [[0.0, 1.0]])
    assert ek.allclose(distr.eval(samples, [0.5]), np.ones((samples_cout)))

    # Build a data grid of shape [2 x res x res] with random data. Compare the lerping of the 1D
    # Warp versus evaluating a 0D Warp with data that as manually been lerped.
    for i in range(10):
        np.random.seed(1234 + i)
        data_rand = np.array([np.random.random((data_res, data_res)),
                              np.random.random((data_res, data_res))])

        distr = warps[1](data_rand, [[0.0, 1.0]], False, False)

        for p in np.linspace(0.0, 1.0, 6):
            interp_data = data_rand[0] * (1.0 - p) + data_rand[1] * p
            interp_distr = warps[0](interp_data, [], False, False)
            assert ek.allclose(distr.eval(samples, [p]), interp_distr.eval(samples))


def test03_sample_match_eval_invert(variants, warps):
    for i in range(5):
        np.random.seed(123456 + i)
        distr0 = warps[0](np.random.random((16, 16)), [])
        distr1 = warps[1](np.random.random((4, 16, 16)), [np.linspace(0.0, 1.0, 4)])

        pos, s_den = distr0.sample(samples)
        sam, i_den = distr0.invert(pos)

        assert ek.allclose(s_den, distr0.eval(pos), atol=1e-4)
        assert ek.allclose(s_den, i_den, atol=1e-4)
        assert ek.allclose(samples, sam, atol=1e-4)

        for p in np.linspace(0.0, 1.0, 12):
            pos, s_den = distr1.sample(samples, [p])
            sam, i_den = distr1.invert(pos, [p])
            assert ek.allclose(s_den, distr1.eval(pos, [p]), atol=1e-4)
            assert ek.allclose(s_den, i_den, atol=1e-4)
            assert ek.allclose(samples, sam, atol=1e-4)


# Sample a continuous bilinear patch defined by 4 points
def invert_marginal_cdf(points, sample):
    r0 = points[0] + points[1]
    r1 = points[2] + points[3]
    # Invert marginal CDF in the 'y' parameter
    if (ek.abs(r0 - r1) > 1e-4 * (r0 + r1)):
        y = (r0 - ek.sqrt(r0**2 + sample[1] * (r1**2 - r0**2))) / (r0 - r1)
    else:
        y = sample[1]
    # Invert conditional CDF in the 'x' parameter
    c0 = (1.0 - y) * points[0] + y * points[2]
    c1 = (1.0 - y) * points[1] + y * points[3]
    if (ek.abs(c0 - c1) > 1e-4 * (c0 + c1)):
        x = (c0 - ek.sqrt(c0**2 + sample[0] * (c1**2 - c0**2))) / (c0 - c1)
    else:
        x = sample[0]
    return [x, y], (1.0 - x) * c0 + x * c1


def test04_hierarchical2D_sample(variants):
    from mitsuba.core import Hierarchical2D0, Vector2f

    data = np.array([[0.0,  0.0,  0.6],
                     [0.0,  0.0,  0.2],
                     [0.2,  0.2,  0.0]])
    # data_level_1 = np.array([[0.0, 0.2],
    #                          [0.1, 0.1]])
    distr = Hierarchical2D0(data, [])
    assert ek.allclose(distr.sample([0.5, 0.25])[0],
                       (0.5 * (Vector2f(invert_marginal_cdf([0.0, 0.6, 0.0, 0.2], [0.5, 0.5])[0]) + [1, 0])))
    assert ek.allclose(distr.sample([0.25, 0.75])[0],
                       (0.5 * (Vector2f(invert_marginal_cdf([0.0, 0.0, 0.2, 0.2], [0.5, 0.5])[0]) + [0, 1])))

    # with an odd number of bilinear patches per axis
    data = np.array([[0.0,  0.1,  0.1,  0.2],
                     [0.1,  0.0,  0.0,  0.1],
                     [0.1,  0.0,  0.0,  0.1],
                     [0.1,  0.2,  0.2,  0.5]])
    # data_level_1 = np.array([[0.2, 0.2, 0.4],
    #                          [0.2, 0.0, 0.2]
    #                          [0.4, 0.4, 0.8]])
    # data_level_2 = np.array([[0.6,  0.6],
    #                          [0.8,  0.8]])
    distr = Hierarchical2D0(data, [])
    assert ek.allclose(distr.sample([0.125, 1.0/14*3])[0],
                       (Vector2f(invert_marginal_cdf([0.0, 0.1, 0.1, 0.0], [0.5, 3.0/4.0])[0]) + [0, 0]) / 3.0)


def test04_marginal2Ddiscrete_sample(variants):
    from mitsuba.core import Marginal2DDiscrete0, Vector2f

    data = np.array([[0.1,  0.1,  0.5],
                     [0.3,  0.1,  0.3],
                     [0.4,  0.4,  0.4]])
    # bilinear_data = np.array([[0.1, 0.3],
    #                           [0.2, 0.2]
    #                           [0.4, 0.4]])
    # cdfs = np.array([0.1, 0.4],
    #                 [0.2, 0.4]
    #                 [0.4, 0.8])
    # marginal = [0.4, 0.6]

    distr = Marginal2DDiscrete0(data, [])
    assert ek.allclose(distr.sample([0.5, 0.2])[0],
                       0.5 * (Vector2f(invert_marginal_cdf([0.1, 0.5, 0.1, 0.3], [1.0/3, 0.5])[0]) + [1, 0]))

    assert ek.allclose(distr.sample([0.25, 0.5])[0],
                       0.5 * (Vector2f(invert_marginal_cdf([0.3, 0.1, 0.4, 0.4], [0.5, 1.0/6])[0]) + [0, 1]))

# TODO
# def test05_marginal2Dcontinuous_sample(variant_scalar):
#     from mitsuba.core import Marginal2DContinuous0, Vector2f

#     data = np.array([[0.1,  0.1,  0.5],
#                      [0.3,  0.1,  0.3],
#                      [0.4,  0.4,  0.4]])
#     # bilinear_data = np.array([[0.1, 0.3],
#     #                           [0.2, 0.2]
#     #                           [0.4, 0.4]])
#     # cdfs = np.array([0.1, 0.4],
#     #                 [0.2, 0.4]
#     #                 [0.4, 0.8])
#     # marginal = [0.4, 0.6]

#     distr = Marginal2DContinuous0(data, [])

#     print("my:", invert_marginal_cdf([0.2, 0.2, 0.4, 0.4], [0.0, 0.5 - 0.4])[0])

#     print(distr.sample([0.5, 0.5])[0]) # -> 1/3 0.5

#     print(0.5 * (Vector2f(invert_marginal_cdf([0.1, 0.5, 0.1, 0.3], [1.0/3, 0.5])[0]) + [1, 0]))


#     # assert ek.allclose(distr.sample([0.5, 0.2])[0],
#     #                    0.5 * (Vector2f(invert_marginal_cdf([0.1, 0.5, 0.1, 0.3], [1.0/3, 0.5])[0]) + [1, 0]))

#     # assert ek.allclose(distr.sample([0.25, 0.5])[0],
#     #                    0.5 * (Vector2f(invert_marginal_cdf([0.3, 0.1, 0.4, 0.4], [0.5, 1.0/6])[0]) + [0, 1]))

#     assert False
