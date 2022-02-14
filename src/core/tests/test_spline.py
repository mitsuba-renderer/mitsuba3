import pytest
import drjit as dr
from drjit.scalar import ArrayXf as Float
import mitsuba as mi


def test_eval_spline(variant_scalar_rgb):
    from mitsuba import spline

    assert dr.allclose(spline.eval_spline(0, 0, 0, 0, 0), 0)
    assert dr.allclose(spline.eval_spline(0, 0, 0, 0, 0.5), 0)
    assert dr.allclose(spline.eval_spline(0, 0, 0, 0, 1), 0)
    assert dr.allclose(spline.eval_spline(0, 1, 1, 1, 1), 1)
    assert dr.allclose(spline.eval_spline(0, 1, 1, 1, 0), 0)
    assert dr.allclose(spline.eval_spline(0, 1, 1, 1, 0.5), 0.5)


def test_eval_spline_d(variant_scalar_rgb):
    from mitsuba import spline

    assert dr.allclose(spline.eval_spline_d(0, 0, 0, 0, 0),   (0.0, 0.0))
    assert dr.allclose(spline.eval_spline_d(0, 0, 0, 0, 0.5), (0.0, 0.0))
    assert dr.allclose(spline.eval_spline_d(0, 0, 0, 0, 1),   (0.0, 0.0))
    assert dr.allclose(spline.eval_spline_d(0, 1, 1, 1, 1),   (1.0, 1.0))
    assert dr.allclose(spline.eval_spline_d(0, 1, 1, 1, 0),   (0.0, 1.0))
    assert dr.allclose(spline.eval_spline_d(0, 1, 1, 1, 0.5), (0.5, 1.0))
    assert dr.allclose(spline.eval_spline_d(0, 0, 1, -1, 0.5)[1], 0.0)


def test_eval_spline_i(variant_scalar_rgb):
    from mitsuba import spline

    assert dr.allclose(spline.eval_spline_i(0, 0, 0, 0, 0),   (0.0, 0.0))
    assert dr.allclose(spline.eval_spline_i(0, 0, 0, 0, 0.5), (0.0, 0.0))
    assert dr.allclose(spline.eval_spline_i(0, 0, 0, 0, 1),   (0.0, 0.0))
    assert dr.allclose(spline.eval_spline_i(0, 1, 1, 1, 0),   (0.0, 0.0))
    assert dr.allclose(spline.eval_spline_i(0, 1, 1, 1, 0.5), (0.125, 0.5))
    assert dr.allclose(spline.eval_spline_i(0, 1, 1, 1, 1),   (0.5, 1.0))

values1 = Float([0.0, 0.0,  0.5, 1.0,  1.0])
values2 = Float([0.0, 0.25, 0.5, 0.75, 1.0])
values3 = Float([1.0, 2.0,  3.0, 2.0,  1.0])


def test_eval_1d_uniform(variant_scalar_rgb):
    from mitsuba import spline

    assert dr.allclose(spline.eval_1d(0, 1, values1, 0), 0)
    assert dr.allclose(spline.eval_1d(0, 1, values1, 0.5), 0.5)
    assert dr.allclose(spline.eval_1d(0, 1, values1, 1), 1)
    assert dr.allclose(spline.eval_1d(0, 1, values2, 0.2), 0.2)
    assert dr.allclose(spline.eval_1d(0, 1, values2, 0.8), 0.8)

input1 = Float([0, 0.5, 1,   0.5, 0, 0.5, 1,   0.5])
input2 = Float([0, 0.5, 0.8, 0.5, 0, 0.5, 0.8, 0.5])

nodes1 = Float([0.0, 0.25, 0.5, 0.75, 1])
nodes2 = Float([0.0, 0.5,  1.0, 1.5,  2])


def test_eval_1d_non_uniform(variant_scalar_rgb):
    from mitsuba import spline

    assert dr.allclose(spline.eval_1d(nodes1, values2, 0), 0)
    assert dr.allclose(spline.eval_1d(nodes1, values2, 0.5), 0.5)
    assert dr.allclose(spline.eval_1d(nodes1, values2, 1), 1)

    assert dr.allclose(spline.eval_1d(nodes2, values2, 0), 0)
    assert dr.allclose(spline.eval_1d(nodes2, values2, 1), 0.5)
    assert dr.allclose(spline.eval_1d(nodes2, values2, 2), 1)


def test_integrate_1d_uniform(variant_scalar_rgb):
    from mitsuba import spline

    values = Float([0.0, 0.5, 1])
    res = Float([0.0, 0.125, 0.5])
    print(spline.integrate_1d(0, 1, values))
    print(type(spline.integrate_1d(0, 1, values)))
    assert dr.allclose(spline.integrate_1d(0, 1, values), res)
    assert dr.allclose(spline.integrate_1d(0, 2, values), 2*res)


def test_integrate_1d_non_uniform(variant_scalar_rgb):
    from mitsuba import spline

    values = Float([0.0, 0.5, 1])
    nodes = Float([0.0, 0.5, 1])
    res = Float([0.0, 0.125, 0.5])
    assert dr.allclose(spline.integrate_1d(nodes, values), res)


def test_invert_1d(variant_scalar_rgb):
    from mitsuba import spline

    values = Float([0.0, 0.25, 0.5, 0.75, 1.0])
    assert dr.allclose(spline.invert_1d(0, 1, values, spline.eval_1d(0, 1, values, 0.6)), 0.6)
    values = Float([0.1, 0.2,  0.3, 0.35,  1])
    assert dr.allclose(spline.invert_1d(0, 1, values, spline.eval_1d(0, 1, values, 0.26)), 0.26)
    assert dr.allclose(spline.invert_1d(0, 1, values, spline.eval_1d(0, 1, values, 0.46)), 0.46)
    assert dr.allclose(spline.invert_1d(0, 1, values, spline.eval_1d(0, 1, values, 0.86)), 0.86)


def test_sample_1d_uniform(variant_scalar_rgb):
    from mitsuba import spline

    values = Float([0.0, 0.25, 0.5, 0.75, 1.0])

    for i in range(10):
        res = spline.sample_1d(0, 2, values, spline.integrate_1d(0, 2, values), i / 10)
        assert dr.allclose(res[1], res[2])

    values = Float([1.0, 1.0, 1.0, 1.0, 1.0])
    res = spline.sample_1d(0, 1, values, spline.integrate_1d(0, 1, values), 0.5)
    assert dr.allclose(res[0], 0.5)
    assert dr.allclose(res[1], 1)
    assert dr.allclose(res[2], 1)


def test_sample_1d_non_uniform(variant_scalar_rgb):
    from mitsuba import spline

    nodes = Float([0.0, 0.25, 0.5, 0.75, 1.0])
    values = Float([0.0, 0.25, 0.5, 0.75, 1.0])

    for i in range(10):
        res = spline.sample_1d(2*nodes, values, spline.integrate_1d(2*nodes, values), i / 10)
        assert dr.allclose(res[1], res[2])

    values = Float([1.0, 1.0, 1.0, 1.0, 1.0])
    res = spline.sample_1d(nodes, values, spline.integrate_1d(nodes, values), 0.5)
    assert dr.allclose(res[0], 0.5)
    assert dr.allclose(res[1], 1)
    assert dr.allclose(res[2], 1)


def test_eval_2d(variant_scalar_rgb):
    from mitsuba import spline

    nodes_x = Float([0.0, 0.25, 0.5, 0.75, 1.0])
    nodes_y = Float([0.0, 0.25, 0.5, 0.75, 1.0])
    values = Float([0.0, 0.25, 0.5, 0.75, 1.0,
                       0.0, 0.25, 0.5, 0.75, 1.0,
                       0.0, 0.25, 0.5, 0.75, 1.0,
                       0.0, 0.25, 0.5, 0.75, 1.0,
                       0.0, 0.25, 0.5, 0.75, 1.0])

    assert dr.allclose(spline.eval_2d(nodes_x, nodes_y, values, 0, 0),     0)
    assert dr.allclose(spline.eval_2d(nodes_x, nodes_y, values, 1, 0),     1)
    assert dr.allclose(spline.eval_2d(nodes_x, nodes_y, values, 0, 1),     0)
    assert dr.allclose(spline.eval_2d(nodes_x, nodes_y, values, 1, 1),     1)
    assert dr.allclose(spline.eval_2d(nodes_x, nodes_y, values, 0.5, 0.5), 0.5)
