from mitsuba.core import Frame
import numpy as np


def assertAlmostEqual(a, b):
    if type(a) is float or type(a) is int:
        a = [a]
    if type(b) is float or type(b) is int:
        b = [b]
    assert len(a) == len(b)
    for i in range(len(a)):
        assert np.abs(a[i] - b[i]) / (b[i] + 1e-6) < 1e-6


def test01_construction():
    # Uninitialized frame
    _ = Frame()

    # Frame from the 3 vectors: no normalization should be performed
    f1 = Frame([0.005, 50, -6], [0.01, -13.37, 1], [0.5, 0, -6.2])
    assertAlmostEqual(f1.s, [0.005, 50, -6])
    assertAlmostEqual(f1.t, [0.01, -13.37, 1])
    assertAlmostEqual(f1.n, [0.5, 0, -6.2])

    # Frame from the Normal component only
    f2 = Frame([0, 0, 1])
    assertAlmostEqual(f2.s, [1, 0, 0])
    assertAlmostEqual(f2.t, [0, 1, 0])
    assertAlmostEqual(f2.n, [0, 0, 1])

    # Copy constructor
    f3 = Frame(f2)
    assert f2 == f3


def test02_unit_frame():
    f = Frame([1, 0, 0], [0, 1, 0], [0, 0, 1])
    v = [0.5, 2.7, -3.12]
    lv = f.toLocal(v)

    assertAlmostEqual(lv, v)
    assertAlmostEqual(f.toWorld(v), v)
    assertAlmostEqual(f.cosTheta2(lv), v[2] * v[2])
    assertAlmostEqual(f.cosTheta(lv), v[2])
    assertAlmostEqual(f.sinTheta2(lv), 1 - f.cosTheta2(v))
    assertAlmostEqual(f.uv(f.toLocal(v)), v[0:2])



def test03_frame_equality():
    f1 = Frame([1, 0, 0], [0, 1, 0], [0, 0, 1])
    f2 = Frame([0, 0, 1])
    f3 = Frame([0, 0, 1], [0, 1, 0], [1, 0, 0])

    assert f1 == f2
    assert f2 == f1
    assert not f1 == f3
    assert not f2 == f3
