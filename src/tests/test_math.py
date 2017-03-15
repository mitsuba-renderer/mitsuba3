import numpy as np
from mitsuba.core.math import log2i
from mitsuba.core.math import is_power_of_two
from mitsuba.core.math import round_to_power_of_two

from mitsuba.core.math import legendre_p
from mitsuba.core.math import legendre_pd
from mitsuba.core.math import legendre_pd_diff


def test01_log2i():
    from mitsuba.core.math import log2i
    assert log2i(1) == 0
    assert log2i(2) == 1
    assert log2i(3) == 1
    assert log2i(4) == 2
    assert log2i(5) == 2
    assert log2i(6) == 2
    assert log2i(7) == 2
    assert log2i(8) == 3


def test02_is_power_of_two():
    from mitsuba.core.math import is_power_of_two
    assert not is_power_of_two(0)
    assert is_power_of_two(1)
    assert is_power_of_two(2)
    assert not is_power_of_two(3)
    assert is_power_of_two(4)
    assert not is_power_of_two(5)
    assert not is_power_of_two(6)
    assert not is_power_of_two(7)
    assert is_power_of_two(8)


def test03_round_to_power_of_two():
    from mitsuba.core.math import round_to_power_of_two
    assert round_to_power_of_two(0) == 1
    assert round_to_power_of_two(1) == 1
    assert round_to_power_of_two(2) == 2
    assert round_to_power_of_two(3) == 4
    assert round_to_power_of_two(4) == 4
    assert round_to_power_of_two(5) == 8
    assert round_to_power_of_two(6) == 8
    assert round_to_power_of_two(7) == 8
    assert round_to_power_of_two(8) == 8


def test04_solve_quadratic():
    from mitsuba.core.math import solve_quadratic
    assert np.allclose(solve_quadratic(1, 4, -5), (True, -5, 1))
    assert np.allclose(solve_quadratic([1], [4], [-5]), ([True], [-5], [1]))
    assert np.allclose(solve_quadratic(0, 5, -10), (True, 2, 2))
    assert np.allclose(solve_quadratic([0], [5], [-10]), ([True], [2], [2]))


def test_legendre_p():
        assert(np.allclose(legendre_p(0, 0), 1))
        assert(np.allclose(legendre_p(1, 0), 0))
        assert(np.allclose(legendre_p(1, 1), 1))
        assert(np.allclose(legendre_p(2, 0), -0.5))
        assert(np.allclose(legendre_p(2, 1), 1))
        assert(np.allclose(legendre_p(3, 0), 0))


def test_legendre_pd():
    assert(np.allclose(legendre_pd(0, 0),   [1, 0]))
    assert(np.allclose(legendre_pd(1, 0),   [0, 1]))
    assert(np.allclose(legendre_pd(1, 0.5), [0.5, 1]))
    assert(np.allclose(legendre_pd(2, 0),   [-0.5, 0]))


def test_legendre_pd_diff():
        assert(np.allclose(legendre_pd_diff(1, 1),   np.array(legendre_pd(1+1, 1))   - np.array(legendre_pd(1-1, 1))))
        assert(np.allclose(legendre_pd_diff(2, 1),   np.array(legendre_pd(2+1, 1))   - np.array(legendre_pd(2-1, 1))))
        assert(np.allclose(legendre_pd_diff(3, 0),   np.array(legendre_pd(3+1, 0))   - np.array(legendre_pd(3-1, 0))))
        assert(np.allclose(legendre_pd_diff(4, 0.1), np.array(legendre_pd(4+1, 0.1)) - np.array(legendre_pd(4-1, 0.1))))


def test_find_interval():
	from mitsuba.core.math import find_interval
	values = np.array([0.0, 0.25, 0.5, 0.75, 1.0])
	assert(np.allclose(find_interval(values, 0.05), 0))
	assert(np.allclose(find_interval(values, 0.45), 1))
	assert(np.allclose(find_interval(values, 0.55), 2))
	assert(np.allclose(find_interval(values, 0.85), 3))
	assert(np.allclose(find_interval(values, 0.75), 3))
	# edge cases
	assert(np.allclose(find_interval(values, -1), 3))
	assert(np.allclose(find_interval(values,  0), 0))
	assert(np.allclose(find_interval(values, 1.75),  3))
	# repeated item in the nodes
	valuesR = np.array([0.0, 0.25, 0.25, 0.5, 0.75, 1.0])	
	assert(np.allclose(find_interval(valuesR, 0.45), 2))
	assert(np.allclose(find_interval(valuesR, 0.25), 2))