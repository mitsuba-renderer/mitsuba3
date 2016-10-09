from mitsuba.core.math import log2i
from mitsuba.core.math import isPowerOfTwo
from mitsuba.core.math import roundToPowerOfTwo


def test01_log2i():
    assert log2i(1) == 0
    assert log2i(2) == 1
    assert log2i(3) == 1
    assert log2i(4) == 2
    assert log2i(5) == 2
    assert log2i(6) == 2
    assert log2i(7) == 2
    assert log2i(8) == 3


def test01_isPowerOfTwo():
    assert not isPowerOfTwo(0)
    assert isPowerOfTwo(1)
    assert isPowerOfTwo(2)
    assert not isPowerOfTwo(3)
    assert isPowerOfTwo(4)
    assert not isPowerOfTwo(5)
    assert not isPowerOfTwo(6)
    assert not isPowerOfTwo(7)
    assert isPowerOfTwo(8)


def test01_roundToPowerOfTwo():
    assert roundToPowerOfTwo(1) == 1
    assert roundToPowerOfTwo(2) == 2
    assert roundToPowerOfTwo(3) == 4
    assert roundToPowerOfTwo(4) == 4
    assert roundToPowerOfTwo(5) == 8
    assert roundToPowerOfTwo(6) == 8
    assert roundToPowerOfTwo(7) == 8
    assert roundToPowerOfTwo(8) == 8
    assert roundToPowerOfTwo(0) == 1

