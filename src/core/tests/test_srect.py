import pytest
import drjit as dr
import mitsuba as mi


def test01_solid_angle(variant_scalar_rgb):
    from math import atan, sqrt

    # Closed form for the solid angle of a rectangle seen from a point above its center
    a, b, d = 1.6, 2.6, 2.0
    rect = mi.SphericalRectangle3f([0, 0, 0], [0, 0, d], [a / 2, 0, 0], [0, b / 2, 0])
    omega = 4 * atan(a * b / (2 * d * sqrt(4 * d * d + a * a + b * b)))
    assert dr.allclose(rect.solid_angle, omega, rtol=1e-5)
    assert not rect.degenerate

    v = rect.sample([0.3, 0.9])
    assert rect.contains(v)
    assert dr.allclose(rect.pdf(v), 1 / omega, rtol=1e-5)
    assert rect.pdf(mi.Vector3f(0, 0, -1)) == 0

    # A reference point in the plane of the rectangle is degenerate
    flat = mi.SphericalRectangle3f([3, 0, 0], [0, 0, 0], [1, 0, 0], [0, 1, 0])
    assert flat.degenerate
    assert flat.pdf(flat.sample([0.5, 0.5])) == 0


@pytest.mark.parametrize("offset", [[0.2, -0.1, 0.3], [2.5, 1.5, 0.1]])
def test02_chi2(variants_vec_backends_once_rgb, offset):
    dx = 0.8 * dr.normalize(mi.Vector3f(1, 0.5, 0))
    dy = 1.3 * dr.normalize(mi.Vector3f(-0.5, 1, 0))
    rect = mi.SphericalRectangle3f(mi.Point3f(offset), mi.Point3f(0.5, 0.4, 2.0), dx, dy)
    assert not dr.any(rect.degenerate)

    chi2 = mi.chi2.ChiSquareTest(
        domain=mi.chi2.SphericalDomain(),
        sample_func=rect.sample,
        pdf_func=rect.pdf,
        sample_dim=2,
        ires=32
    )

    assert chi2.run()
