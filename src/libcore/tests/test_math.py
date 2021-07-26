import enoki as ek
import pytest
import mitsuba


def test01_is_power_of_two(variant_scalar_rgb):
    from mitsuba.core import math
    assert not math.is_power_of_two(0)
    assert math.is_power_of_two(1)
    assert math.is_power_of_two(2)
    assert not math.is_power_of_two(3)
    assert math.is_power_of_two(4)
    assert not math.is_power_of_two(5)
    assert not math.is_power_of_two(6)
    assert not math.is_power_of_two(7)
    assert math.is_power_of_two(8)


def test02_round_to_power_of_two(variant_scalar_rgb):
    from mitsuba.core import math
    assert math.round_to_power_of_two(0) == 1
    assert math.round_to_power_of_two(1) == 1
    assert math.round_to_power_of_two(2) == 2
    assert math.round_to_power_of_two(3) == 4
    assert math.round_to_power_of_two(4) == 4
    assert math.round_to_power_of_two(5) == 8
    assert math.round_to_power_of_two(6) == 8
    assert math.round_to_power_of_two(7) == 8
    assert math.round_to_power_of_two(8) == 8


def test03_solve_quadratic(variant_scalar_rgb):
    from mitsuba.core import math
    assert ek.allclose(math.solve_quadratic(1, 4, -5), (1, -5, 1))
    assert ek.allclose(math.solve_quadratic(0, 5, -10), (1, 2, 2))
    assert ek.allclose(math.solve_quadratic(0, -5, 10), (1, 2, 2))


def test04_solve_quadratic_vec(variant_scalar_rgb):
    from mitsuba.python.test.util import check_vectorization

    def kernel(a : float, b : float, c : float):
        from mitsuba.core import math

        found, x, y = math.solve_quadratic(a, b, c)
        x = ek.select(found, x, 0.0)
        y = ek.select(found, y, 0.0)

        return x, y

    check_vectorization(kernel)


def test05_legendre_p(variant_scalar_rgb):
    from mitsuba.core import math
    assert ek.allclose(math.legendre_p(0, 0), 1)
    assert ek.allclose(math.legendre_p(1, 0), 0)
    assert ek.allclose(math.legendre_p(1, 1), 1)
    assert ek.allclose(math.legendre_p(2, 0), -0.5)
    assert ek.allclose(math.legendre_p(2, 1), 1)
    assert ek.allclose(math.legendre_p(3, 0), 0)


def test06_legendre_pd(variant_scalar_rgb):
    from mitsuba.core import math
    assert ek.allclose(math.legendre_pd(0, 0),   [1, 0])
    assert ek.allclose(math.legendre_pd(1, 0),   [0, 1])
    assert ek.allclose(math.legendre_pd(1, 0.5), [0.5, 1])
    assert ek.allclose(math.legendre_pd(2, 0),   [-0.5, 0])


def test07_legendre_pd_diff(variant_scalar_rgb):
    from mitsuba.core import math, Vector2f
    assert ek.allclose(math.legendre_pd_diff(1, 1),   Vector2f(math.legendre_pd(1+1, 1))   - Vector2f(math.legendre_pd(1-1, 1)))
    assert ek.allclose(math.legendre_pd_diff(2, 1),   Vector2f(math.legendre_pd(2+1, 1))   - Vector2f(math.legendre_pd(2-1, 1)))
    assert ek.allclose(math.legendre_pd_diff(3, 0),   Vector2f(math.legendre_pd(3+1, 0))   - Vector2f(math.legendre_pd(3-1, 0)))
    assert ek.allclose(math.legendre_pd_diff(4, 0.1), Vector2f(math.legendre_pd(4+1, 0.1)) - Vector2f(math.legendre_pd(4-1, 0.1)))


def test08_legendre_vec(variant_scalar_rgb):
    from mitsuba.python.test.util import check_vectorization

    def kernel(x : float):
        from mitsuba.core import Vector2f
        from mitsuba.core.math import legendre_p, legendre_pd, legendre_pd_diff
        return legendre_p(1, x), Vector2f(legendre_pd(3, x)), Vector2f(legendre_pd_diff(4, x))

    check_vectorization(kernel)


def test08_morton2(variant_scalar_rgb):
    from mitsuba.core import math
    v0 = [123, 456]
    v1 = math.morton_encode2(v0)
    v2 = math.morton_decode2(v1)
    assert ek.all(v0 == v2)


def test09_morton3(variant_scalar_rgb):
    from mitsuba.core import math
    v0 = [123, 456, 789]
    v1 = math.morton_encode3(v0)
    v2 = math.morton_decode3(v1)
    assert ek.all(v0 == v2)
