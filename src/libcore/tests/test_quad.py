from mitsuba.scalar_rgb.core.quad import *
import numpy as np

def test01_gauss_lobatto():
    assert np.allclose(gauss_lobatto(2), [[-1, 1], [1.0, 1.0]])
    assert np.allclose(gauss_lobatto(3), [[-1, 0, 1], [1.0/3.0, 4.0/3.0, 1.0/3.0]])
    assert np.allclose(gauss_lobatto(4), [[-1, -np.sqrt(1.0/5.0), np.sqrt(1.0/5.0), 1], [1.0/6.0, 5.0/6.0, 5.0/6.0, 1.0/6.0]])
    assert np.allclose(gauss_lobatto(5), [[-1, -np.sqrt(3.0/7.0), 0, np.sqrt(3.0/7.0), 1], [1.0/10.0, 49.0/90.0, 32.0/45.0, 49.0/90.0, 1.0/10.0]])

def test02_gauss_legendre():
    assert np.allclose(gauss_legendre(1), [[0], [2]])
    assert np.allclose(gauss_legendre(2), [[-np.sqrt(1.0/3.0), np.sqrt(1.0/3.0)], [1, 1]])
    assert np.allclose(gauss_legendre(3), [[-np.sqrt(3.0/5.0), 0, np.sqrt(3.0/5.0)], [5.0/9.0, 8.0/9.0, 5.0/9.0]])
    assert np.allclose(gauss_legendre(4), [[-0.861136, -0.339981, 0.339981, 0.861136, ], [0.347855, 0.652145, 0.652145, 0.347855]])

def test03_composite_simpson():
    assert np.allclose(composite_simpson(3), [np.linspace(-1, 1, 3), [1.0/3.0, 4.0/3.0, 1.0/3.0]])
    assert np.allclose(composite_simpson(5), [np.linspace(-1, 1, 5), np.array([.5, 2, 1, 2, .5])/3])

def test04_composite_simpson_38():
    assert np.allclose(composite_simpson_38(4), [np.linspace(-1, 1, 4), [0.25, 0.75, 0.75, 0.25]])
    assert np.allclose(composite_simpson_38(7), [np.linspace(-1, 1, 7), [0.125, 0.375, 0.375, 0.25 , 0.375, 0.375, 0.125]], atol=1e-6)
