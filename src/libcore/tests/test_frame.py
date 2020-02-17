import enoki as ek
import pytest
import mitsuba


def test01_construction(variant_scalar_rgb):
    from mitsuba.core import Frame3f

    # Uninitialized frame
    _ = Frame3f()

    # Frame3f from the 3 vectors: no normalization should be performed
    f1 = Frame3f([0.005, 50, -6], [0.01, -13.37, 1], [0.5, 0, -6.2])
    assert ek.allclose(f1.s, [0.005, 50, -6])
    assert ek.allclose(f1.t, [0.01, -13.37, 1])
    assert ek.allclose(f1.n, [0.5, 0, -6.2])

    # Frame3f from the Normal component only
    f2 = Frame3f([0, 0, 1])
    assert ek.allclose(f2.s, [1, 0, 0])
    assert ek.allclose(f2.t, [0, 1, 0])
    assert ek.allclose(f2.n, [0, 0, 1])

    # Copy constructor
    f3 = Frame3f(f2)
    assert f2 == f3


def test02_unit_frame(variant_scalar_rgb):
    from mitsuba.core import Frame3f, Vector2f, Vector3f

    for theta in [30 * mitsuba.core.math.Pi / 180, 95 * mitsuba.core.math.Pi / 180]:
        phi = 73 * mitsuba.core.math.Pi / 180
        sin_theta, cos_theta = ek.sin(theta), ek.cos(theta)
        sin_phi, cos_phi = ek.sin(phi), ek.cos(phi)

        v = Vector3f(
            cos_phi * sin_theta,
            sin_phi * sin_theta,
            cos_theta
        )
        f = Frame3f(Vector3f(1.0, 2.0, 3.0) / ek.sqrt(14))

        v2 = f.to_local(v)
        v3 = f.to_world(v2)

        assert ek.allclose(v3, v)

        assert ek.allclose(Frame3f.cos_theta(v), cos_theta)
        assert ek.allclose(Frame3f.sin_theta(v), sin_theta)
        assert ek.allclose(Frame3f.cos_phi(v), cos_phi)
        assert ek.allclose(Frame3f.sin_phi(v), sin_phi)
        assert ek.allclose(Frame3f.cos_theta_2(v), cos_theta * cos_theta)
        assert ek.allclose(Frame3f.sin_theta_2(v), sin_theta * sin_theta)
        assert ek.allclose(Frame3f.cos_phi_2(v), cos_phi * cos_phi)
        assert ek.allclose(Frame3f.sin_phi_2(v), sin_phi * sin_phi)
        assert ek.allclose(Vector2f(Frame3f.sincos_phi(v)), [sin_phi, cos_phi])
        assert ek.allclose(Vector2f(Frame3f.sincos_phi_2(v)), [sin_phi * sin_phi, cos_phi * cos_phi])


def test03_frame_equality(variant_scalar_rgb):
    from mitsuba.core import Frame3f

    f1 = Frame3f([1, 0, 0], [0, 1, 0], [0, 0, 1])
    f2 = Frame3f([0, 0, 1])
    f3 = Frame3f([0, 0, 1], [0, 1, 0], [1, 0, 0])

    assert f1 == f2
    assert f2 == f1
    assert not f1 == f3
    assert not f2 == f3
