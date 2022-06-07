import pytest
import drjit as dr
import mitsuba as mi


def test01_construction(variant_scalar_rgb):
    # Uninitialized frame
    _ = mi.Frame3f()

    # mi.Frame3f from the 3 vectors: no normalization should be performed
    f1 = mi.Frame3f([0.005, 50, -6], [0.01, -13.37, 1], [0.5, 0, -6.2])
    assert dr.allclose(f1.s, [0.005, 50, -6])
    assert dr.allclose(f1.t, [0.01, -13.37, 1])
    assert dr.allclose(f1.n, [0.5, 0, -6.2])

    # mi.Frame3f from the Normal component only
    f2 = mi.Frame3f([0, 0, 1])
    assert dr.allclose(f2.s, [1, 0, 0])
    assert dr.allclose(f2.t, [0, 1, 0])
    assert dr.allclose(f2.n, [0, 0, 1])

    # Copy constructor
    f3 = mi.Frame3f(f2)
    assert f2 == f3


def test02_unit_frame(variant_scalar_rgb):
    for theta in [30 * dr.pi / 180, 95 * dr.pi / 180]:
        phi = 73 * dr.pi / 180
        sin_theta, cos_theta = dr.sin(theta), dr.cos(theta)
        sin_phi, cos_phi = dr.sin(phi), dr.cos(phi)

        v = mi.Vector3f(
            cos_phi * sin_theta,
            sin_phi * sin_theta,
            cos_theta
        )
        f = mi.Frame3f(mi.Vector3f(1.0, 2.0, 3.0) / dr.sqrt(14))

        v2 = f.to_local(v)
        v3 = f.to_world(v2)

        assert dr.allclose(v3, v)

        assert dr.allclose(mi.Frame3f.cos_theta(v), cos_theta)
        assert dr.allclose(mi.Frame3f.sin_theta(v), sin_theta)
        assert dr.allclose(mi.Frame3f.cos_phi(v), cos_phi)
        assert dr.allclose(mi.Frame3f.sin_phi(v), sin_phi)
        assert dr.allclose(mi.Frame3f.cos_theta_2(v), cos_theta * cos_theta)
        assert dr.allclose(mi.Frame3f.sin_theta_2(v), sin_theta * sin_theta)
        assert dr.allclose(mi.Frame3f.cos_phi_2(v), cos_phi * cos_phi)
        assert dr.allclose(mi.Frame3f.sin_phi_2(v), sin_phi * sin_phi)
        assert dr.allclose(mi.Vector2f(mi.Frame3f.sincos_phi(v)), [sin_phi, cos_phi])
        assert dr.allclose(mi.Vector2f(mi.Frame3f.sincos_phi_2(v)), [sin_phi * sin_phi, cos_phi * cos_phi])


def test03_frame_equality(variant_scalar_rgb):
    f1 = mi.Frame3f([1, 0, 0], [0, 1, 0], [0, 0, 1])
    f2 = mi.Frame3f([0, 0, 1])
    f3 = mi.Frame3f([0, 0, 1], [0, 1, 0], [1, 0, 0])

    assert f1 == f2
    assert f2 == f1
    assert not f1 == f3
    assert not f2 == f3


def test05_coord_convertion_vec(variant_scalar_rgb):
    from mitsuba.test.util import check_vectorization

    def kernel(theta, phi, d):
        sin_theta, cos_theta = dr.sincos(theta)
        sin_phi, cos_phi = dr.sincos(phi)

        v = mi.Vector3f(
            cos_phi * sin_theta,
            sin_phi * sin_theta,
            cos_theta
        )

        f = mi.Frame3f(dr.normalize(d))

        return f.to_local(v), f.to_world(v)

    check_vectorization(kernel, arg_dims = [1, 1, 3])
