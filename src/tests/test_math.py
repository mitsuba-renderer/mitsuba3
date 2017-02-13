from mitsuba.core.math import log2i
from mitsuba.core.math import is_power_of_two
from mitsuba.core.math import round_to_power_of_two


def test01_log2i():
    assert log2i(1) == 0
    assert log2i(2) == 1
    assert log2i(3) == 1
    assert log2i(4) == 2
    assert log2i(5) == 2
    assert log2i(6) == 2
    assert log2i(7) == 2
    assert log2i(8) == 3


def test01_is_power_of_two():
    assert not is_power_of_two(0)
    assert is_power_of_two(1)
    assert is_power_of_two(2)
    assert not is_power_of_two(3)
    assert is_power_of_two(4)
    assert not is_power_of_two(5)
    assert not is_power_of_two(6)
    assert not is_power_of_two(7)
    assert is_power_of_two(8)


def test01_round_to_power_of_two():
    assert round_to_power_of_two(0) == 1
    assert round_to_power_of_two(1) == 1
    assert round_to_power_of_two(2) == 2
    assert round_to_power_of_two(3) == 4
    assert round_to_power_of_two(4) == 4
    assert round_to_power_of_two(5) == 8
    assert round_to_power_of_two(6) == 8
    assert round_to_power_of_two(7) == 8
    assert round_to_power_of_two(8) == 8

