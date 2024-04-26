import pytest
import mitsuba as mi
import drjit as dr
from drjit.scalar import ArrayXf as Float


def test01_gauss_lobatto(variant_scalar_rgb):
    gauss_lobatto = mi.quad.gauss_lobatto

    assert dr.allclose(gauss_lobatto(2), [[-1, 1], [1.0, 1.0]])
    assert dr.allclose(gauss_lobatto(3), [[-1, 0, 1], [1.0/3.0, 4.0/3.0, 1.0/3.0]])
    assert dr.allclose(gauss_lobatto(4), [[-1, -dr.sqrt(1.0/5.0), dr.sqrt(1.0/5.0), 1], [1.0/6.0, 5.0/6.0, 5.0/6.0, 1.0/6.0]])
    assert dr.allclose(gauss_lobatto(5), [[-1, -dr.sqrt(3.0/7.0), 0, dr.sqrt(3.0/7.0), 1], [1.0/10.0, 49.0/90.0, 32.0/45.0, 49.0/90.0, 1.0/10.0]])


def test02_gauss_legendre(variant_scalar_rgb):
    gauss_legendre = mi.quad.gauss_legendre

    assert dr.allclose(gauss_legendre(1), [[0], [2]])
    assert dr.allclose(gauss_legendre(2), [[-dr.sqrt(1.0/3.0), dr.sqrt(1.0/3.0)], [1, 1]])
    assert dr.allclose(gauss_legendre(3), [[-dr.sqrt(3.0/5.0), 0, dr.sqrt(3.0/5.0)], [5.0/9.0, 8.0/9.0, 5.0/9.0]])
    assert dr.allclose(gauss_legendre(4), [[-0.861136, -0.339981, 0.339981, 0.861136, ], [0.347855, 0.652145, 0.652145, 0.347855]])


def test03_composite_simpson(variant_scalar_rgb):
    composite_simpson = mi.quad.composite_simpson

    assert dr.allclose(composite_simpson(3), [dr.linspace(Float, -1, 1, 3), [1.0/3.0, 4.0/3.0, 1.0/3.0]])
    assert dr.allclose(composite_simpson(5), [dr.linspace(Float, -1, 1, 5), [.5/3.0, 2/3.0, 1/3.0, 2/3.0, .5/3.0]])


def test04_composite_simpson_38(variant_scalar_rgb):
    composite_simpson_38 = mi.quad.composite_simpson_38

    assert dr.allclose(composite_simpson_38(4), [dr.linspace(Float, -1, 1, 4), [0.25, 0.75, 0.75, 0.25]])
    assert dr.allclose(composite_simpson_38(7), [dr.linspace(Float, -1, 1, 7), [0.125, 0.375, 0.375, 0.25 , 0.375, 0.375, 0.125]], atol=1e-6)
