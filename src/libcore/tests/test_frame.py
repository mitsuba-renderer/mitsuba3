from mitsuba.scalar_rgb.core import Frame3f
import numpy as np


def test01_construction():
    # Uninitialized frame
    _ = Frame3f()

    # Frame3f from the 3 vectors: no normalization should be performed
    f1 = Frame3f([0.005, 50, -6], [0.01, -13.37, 1], [0.5, 0, -6.2])
    assert np.allclose(f1.s, [0.005, 50, -6])
    assert np.allclose(f1.t, [0.01, -13.37, 1])
    assert np.allclose(f1.n, [0.5, 0, -6.2])

    # Frame3f from the Normal component only
    f2 = Frame3f([0, 0, 1])
    assert np.allclose(f2.s, [1, 0, 0])
    assert np.allclose(f2.t, [0, 1, 0])
    assert np.allclose(f2.n, [0, 0, 1])

    # Copy constructor
    f3 = Frame3f(f2)
    assert f2 == f3


def test02_unit_frame():
    for theta in [30 * np.pi / 180, 95 * np.pi / 180]:
        phi = 73 * np.pi / 180
        sin_theta, cos_theta = np.sin(theta), np.cos(theta)
        sin_phi, cos_phi = np.sin(phi), np.cos(phi)

        v = np.array([
            cos_phi * sin_theta,
            sin_phi * sin_theta,
            cos_theta
        ])
        f = Frame3f(np.array([1.0, 2.0, 3.0]) / np.sqrt(14))

        v2 = f.to_local(v)
        v3 = f.to_world(v2)

        assert np.allclose(v3, v)

        assert np.allclose(Frame3f.cos_theta(v), cos_theta)
        assert np.allclose(Frame3f.sin_theta(v), sin_theta)
        assert np.allclose(Frame3f.cos_phi(v), cos_phi)
        assert np.allclose(Frame3f.sin_phi(v), sin_phi)
        assert np.allclose(Frame3f.cos_theta_2(v), cos_theta * cos_theta)
        assert np.allclose(Frame3f.sin_theta_2(v), sin_theta * sin_theta)
        assert np.allclose(Frame3f.cos_phi_2(v), cos_phi * cos_phi)
        assert np.allclose(Frame3f.sin_phi_2(v), sin_phi * sin_phi)
        assert np.allclose(Frame3f.sincos_phi(v), [sin_phi, cos_phi])
        assert np.allclose(Frame3f.sincos_phi_2(v), [sin_phi * sin_phi,
                                                   cos_phi * cos_phi])


def test03_frame_equality():
    f1 = Frame3f([1, 0, 0], [0, 1, 0], [0, 0, 1])
    f2 = Frame3f([0, 0, 1])
    f3 = Frame3f([0, 0, 1], [0, 1, 0], [1, 0, 0])

    assert f1 == f2
    assert f2 == f1
    assert not f1 == f3
    assert not f2 == f3
