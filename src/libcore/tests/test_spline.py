import numpy as np # TODO remove this
import enoki as ek
from enoki.dynamic import Float32 as Float
import pytest
import mitsuba

@pytest.fixture
def variant():
    mitsuba.set_variant('scalar_rgb')

def test_eval_spline(variant):
    from mitsuba.core.spline import eval_spline
    assert(np.allclose(eval_spline(0, 0, 0, 0, 0), 0))
    assert(np.allclose(eval_spline(0, 0, 0, 0, 0.5), 0))
    assert(np.allclose(eval_spline(0, 0, 0, 0, 1), 0))
    assert(np.allclose(eval_spline(0, 1, 1, 1, 1), 1))
    assert(np.allclose(eval_spline(0, 1, 1, 1, 0), 0))
    assert(np.allclose(eval_spline(0, 1, 1, 1, 0.5), 0.5))

def test_eval_spline_d(variant):
    from mitsuba.core import Vector2f
    from mitsuba.core.spline import eval_spline_d
    assert(np.allclose(eval_spline_d(0, 0, 0, 0, 0),   (0.0, 0.0)))
    assert(np.allclose(eval_spline_d(0, 0, 0, 0, 0.5), (0.0, 0.0)))
    assert(np.allclose(eval_spline_d(0, 0, 0, 0, 1),   (0.0, 0.0)))
    assert(np.allclose(eval_spline_d(0, 1, 1, 1, 1),   (1.0, 1.0)))
    assert(np.allclose(eval_spline_d(0, 1, 1, 1, 0),   (0.0, 1.0)))
    assert(np.allclose(eval_spline_d(0, 1, 1, 1, 0.5), (0.5, 1.0)))
    assert(np.allclose(eval_spline_d(0, 0, 1, -1, 0.5)[1], 0.0))

def test_eval_spline_i(variant):
    from mitsuba.core.spline import eval_spline_i
    assert(np.allclose(eval_spline_i(0, 0, 0, 0, 0),   (0.0, 0.0)))
    assert(np.allclose(eval_spline_i(0, 0, 0, 0, 0.5), (0.0, 0.0)))
    assert(np.allclose(eval_spline_i(0, 0, 0, 0, 1),   (0.0, 0.0)))
    assert(np.allclose(eval_spline_i(0, 1, 1, 1, 0),   (0.0, 0.0)))
    assert(np.allclose(eval_spline_i(0, 1, 1, 1, 0.5), (0.125, 0.5)))
    assert(np.allclose(eval_spline_i(0, 1, 1, 1, 1),   (0.5, 1.0)))

values1 = Float([0.0, 0.0,  0.5, 1.0,  1.0])
values2 = Float([0.0, 0.25, 0.5, 0.75, 1.0])
values3 = Float([1.0, 2.0,  3.0, 2.0,  1.0])

def test_eval_1d_uniform(variant):
    from mitsuba.core.spline import eval_1d
    assert(np.allclose(eval_1d(0, 1, values1, 0), 0))
    assert(np.allclose(eval_1d(0, 1, values1, 0.5), 0.5))
    assert(np.allclose(eval_1d(0, 1, values1, 1), 1))
    assert(np.allclose(eval_1d(0, 1, values2, 0.2), 0.2))
    assert(np.allclose(eval_1d(0, 1, values2, 0.8), 0.8))

input1 = Float([0, 0.5, 1,   0.5, 0, 0.5, 1,   0.5])
input2 = Float([0, 0.5, 0.8, 0.5, 0, 0.5, 0.8, 0.5])
def test_eval_1d_uniform_vec(variant):
    try:
        from mitsuba.packet_rgb.core.spline import eval_1d as eval_1d_p
    except ImportError:
        pytest.skip("packet_rgb mode not enabled")

    assert(np.allclose(eval_1d_p(0, 1, values1, input1), input1))
    assert(np.allclose(eval_1d_p(0, 1, values2, input2), input2))

nodes1 = Float([0.0, 0.25, 0.5, 0.75, 1])
nodes2 = Float([0.0, 0.5,  1.0, 1.5,  2])
def test_eval_1d_non_uniform(variant):
    from mitsuba.core.spline import eval_1d
    assert(np.allclose(eval_1d(nodes1, values2, 0), 0))
    assert(np.allclose(eval_1d(nodes1, values2, 0.5), 0.5))
    assert(np.allclose(eval_1d(nodes1, values2, 1), 1))

    assert(np.allclose(eval_1d(nodes2, values2, 0), 0))
    assert(np.allclose(eval_1d(nodes2, values2, 1), 0.5))
    assert(np.allclose(eval_1d(nodes2, values2, 2), 1))

def test_eval_1d_non_uniform_vec(variant):
    try:
        from mitsuba.packet_rgb.core.spline import eval_1d as eval_1d_p
    except ImportError:
        pytest.skip("packet_rgb mode not enabled")

    assert(np.allclose(eval_1d_p(nodes1, values2, input1), input1))
    assert(np.allclose(eval_1d_p(nodes1, values2, input2), input2))
    assert(np.allclose(eval_1d_p(nodes2, values2, 2*input1), input1))
    assert(np.allclose(eval_1d_p(nodes2, values2, 2*input2), input2))

def test_integrate_1d_uniform(variant):
    from mitsuba.core.spline import integrate_1d
    values = Float([0.0, 0.5, 1])
    res = Float([0.0, 0.125, 0.5])
    assert(np.allclose(integrate_1d(0, 1, values), res))
    assert(np.allclose(integrate_1d(0, 2, values), 2*res))

def test_integrate_1d_non_uniform(variant):
    from mitsuba.core.spline import integrate_1d
    values = Float([0.0, 0.5, 1])
    nodes = Float([0.0, 0.5, 1])
    res = Float([0.0, 0.125, 0.5])
    assert(np.allclose(integrate_1d(nodes, values), res))

def test_invert_1d(variant):
    from mitsuba.core.spline import eval_1d
    from mitsuba.core.spline import invert_1d
    values = Float([0.0, 0.25, 0.5, 0.75, 1.0])
    assert(np.allclose(invert_1d(0, 1, values, eval_1d(0, 1, values, 0.6)), 0.6))
    values = Float([0.1, 0.2,  0.3, 0.35,  1])
    assert(np.allclose(invert_1d(0, 1, values, eval_1d(0, 1, values, 0.26)), 0.26))
    assert(np.allclose(invert_1d(0, 1, values, eval_1d(0, 1, values, 0.46)), 0.46))
    assert(np.allclose(invert_1d(0, 1, values, eval_1d(0, 1, values, 0.86)), 0.86))


def test_sample_1d_uniform(variant):
    from mitsuba.core.spline import sample_1d
    from mitsuba.core.spline import integrate_1d
    values = Float([0.0, 0.25, 0.5, 0.75, 1.0])

    for i in range(10):
        res = sample_1d(0, 2, values, integrate_1d(0, 2, values), i / 10)
        assert(np.allclose(res[1], res[2]))


    values = Float([1.0, 1.0, 1.0, 1.0, 1.0])
    res = sample_1d(0, 1, values, integrate_1d(0, 1, values), 0.5)
    assert(np.allclose(res[0], 0.5))
    assert(np.allclose(res[1], 1))
    assert(np.allclose(res[2], 1))


def test_sample_1d_non_uniform(variant):
    from mitsuba.core.spline import sample_1d
    from mitsuba.core.spline import integrate_1d
    nodes = Float([0.0, 0.25, 0.5, 0.75, 1.0])
    values = Float([0.0, 0.25, 0.5, 0.75, 1.0])

    for i in range(10):
        res = sample_1d(2*nodes, values, integrate_1d(2*nodes, values), i / 10)
        assert(np.allclose(res[1], res[2]))


    values = Float([1.0, 1.0, 1.0, 1.0, 1.0])
    res = sample_1d(nodes, values, integrate_1d(nodes, values), 0.5)
    assert(np.allclose(res[0], 0.5))
    assert(np.allclose(res[1], 1))
    assert(np.allclose(res[2], 1))


def test_eval_2d(variant):
    from mitsuba.core.spline import eval_2d

    nodes_x = Float([0.0, 0.25, 0.5, 0.75, 1.0])
    nodes_y = Float([0.0, 0.25, 0.5, 0.75, 1.0])
    values = Float([0.0, 0.25, 0.5, 0.75, 1.0,
                       0.0, 0.25, 0.5, 0.75, 1.0,
                       0.0, 0.25, 0.5, 0.75, 1.0,
                       0.0, 0.25, 0.5, 0.75, 1.0,
                       0.0, 0.25, 0.5, 0.75, 1.0])

    assert(np.allclose(eval_2d(nodes_x, nodes_y, values, 0, 0),     0))
    assert(np.allclose(eval_2d(nodes_x, nodes_y, values, 1, 0),     1))
    assert(np.allclose(eval_2d(nodes_x, nodes_y, values, 0, 1),     0))
    assert(np.allclose(eval_2d(nodes_x, nodes_y, values, 1, 1),     1))
    assert(np.allclose(eval_2d(nodes_x, nodes_y, values, 0.5, 0.5), 0.5))
