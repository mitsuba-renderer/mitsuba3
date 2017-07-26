from mitsuba.core import BoundingSphere3f as BSphere
from mitsuba.core import float_dtype
import numpy as np


def test01_basics():
    bsphere1 = BSphere()
    bsphere2 = BSphere([0, 1, 2], 1)

    assert str(bsphere1) == "BoundingSphere3%s[empty]" % float_dtype.char
    assert str(bsphere2) == "BoundingSphere3%s[center = [0, 1, 2]," \
        " radius = 1]" % float_dtype.char
    assert bsphere1.radius == 0
    assert (bsphere1.center == [0, 0, 0]).all()
    assert bsphere2.radius == 1
    assert (bsphere2.center == [0, 1, 2]).all()
    assert bsphere1 != bsphere2
    assert bsphere2 == bsphere2
    assert bsphere1.empty()
    assert not bsphere2.empty()
    bsphere1.expand([0, 1, 0])
    assert not bsphere1.empty()
    assert bsphere1.contains([0, 0, 1])
    assert not bsphere1.contains([0, 0, 1], strict=True)
