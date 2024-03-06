import pytest
import drjit as dr
import mitsuba as mi


def test01_is_power_of_two(variant_scalar_rgb):
    assert not mi.math.is_power_of_two(0)
    assert mi.math.is_power_of_two(1)
    assert mi.math.is_power_of_two(2)
    assert not mi.math.is_power_of_two(3)
    assert mi.math.is_power_of_two(4)
    assert not mi.math.is_power_of_two(5)
    assert not mi.math.is_power_of_two(6)
    assert not mi.math.is_power_of_two(7)
    assert mi.math.is_power_of_two(8)


def test02_round_to_power_of_two(variant_scalar_rgb):
    assert mi.math.round_to_power_of_two(0) == 1
    assert mi.math.round_to_power_of_two(1) == 1
    assert mi.math.round_to_power_of_two(2) == 2
    assert mi.math.round_to_power_of_two(3) == 4
    assert mi.math.round_to_power_of_two(4) == 4
    assert mi.math.round_to_power_of_two(5) == 8
    assert mi.math.round_to_power_of_two(6) == 8
    assert mi.math.round_to_power_of_two(7) == 8
    assert mi.math.round_to_power_of_two(8) == 8


def test03_solve_quadratic(variant_scalar_rgb):
    assert dr.allclose(mi.math.solve_quadratic(1, 4, -5), (1, -5, 1))
    assert dr.allclose(mi.math.solve_quadratic(0, 5, -10), (1, 2, 2))
    assert dr.allclose(mi.math.solve_quadratic(0, -5, 10), (1, 2, 2))


def test04_solve_quadratic_vec(variant_scalar_rgb):
    from mitsuba.test.util import check_vectorization

    def kernel(a : float, b : float, c : float):
        found, x, y = mi.math.solve_quadratic(a, b, c)
        x = dr.select(found, x, 0.0)
        y = dr.select(found, y, 0.0)

        return x, y

    check_vectorization(kernel)


def test05_legendre_p(variant_scalar_rgb):
    assert dr.allclose(mi.math.legendre_p(0, 0), 1)
    assert dr.allclose(mi.math.legendre_p(1, 0), 0)
    assert dr.allclose(mi.math.legendre_p(1, 1), 1)
    assert dr.allclose(mi.math.legendre_p(2, 0), -0.5)
    assert dr.allclose(mi.math.legendre_p(2, 1), 1)
    assert dr.allclose(mi.math.legendre_p(3, 0), 0)


def test06_legendre_pd(variant_scalar_rgb):
    assert dr.allclose(mi.math.legendre_pd(0, 0),   [1, 0])
    assert dr.allclose(mi.math.legendre_pd(1, 0),   [0, 1])
    assert dr.allclose(mi.math.legendre_pd(1, 0.5), [0.5, 1])
    assert dr.allclose(mi.math.legendre_pd(2, 0),   [-0.5, 0])


def test07_legendre_pd_diff(variant_scalar_rgb):
    assert dr.allclose(mi.math.legendre_pd_diff(1, 1),   mi.Vector2f(mi.math.legendre_pd(1+1, 1))   - mi.Vector2f(mi.math.legendre_pd(1-1, 1)))
    assert dr.allclose(mi.math.legendre_pd_diff(2, 1),   mi.Vector2f(mi.math.legendre_pd(2+1, 1))   - mi.Vector2f(mi.math.legendre_pd(2-1, 1)))
    assert dr.allclose(mi.math.legendre_pd_diff(3, 0),   mi.Vector2f(mi.math.legendre_pd(3+1, 0))   - mi.Vector2f(mi.math.legendre_pd(3-1, 0)))
    assert dr.allclose(mi.math.legendre_pd_diff(4, 0.1), mi.Vector2f(mi.math.legendre_pd(4+1, 0.1)) - mi.Vector2f(mi.math.legendre_pd(4-1, 0.1)))


def test08_legendre_vec(variant_scalar_rgb):
    from mitsuba.test.util import check_vectorization

    def kernel(x : float):
        return (
            mi.math.legendre_p(1, x),
            mi.Vector2f(mi.math.legendre_pd(3, x)),
            mi.Vector2f(mi.math.legendre_pd_diff(4, x))
        )

    check_vectorization(kernel)


def test08_morton2(variant_scalar_rgb):
    v0 = [123, 456]
    v1 = mi.math.morton_encode2(v0)
    v2 = mi.math.morton_decode2(v1)
    assert dr.all(v0 == v2)


def test09_morton3(variant_scalar_rgb):
    v0 = [123, 456, 789]
    v1 = mi.math.morton_encode3(v0)
    v2 = mi.math.morton_decode3(v1)
    assert dr.all(v0 == v2)
