from mitsuba.scalar_rgb.core import BoundingSphere3f as BSphere
import numpy as np


def test01_basics():
    bsphere1 = BSphere()
    bsphere2 = BSphere([0, 1, 2], 1)

    assert str(bsphere1) == "BoundingSphere3f[empty]"
    assert str(bsphere2) == "BoundingSphere3f[\n  center = [0, 1, 2],\n  radius = 1\n]"
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
