import numpy as np

import enoki as ek
from enoki.dynamic import Float32 as Float
import pytest
import mitsuba


def check_vectorization(func_str, wrapper = (lambda f: lambda x: f(x)) , resolution = 2):
    """
    Helper routine which compares evaluations of the vectorized and
    non-vectorized version of a warping routine
    """

    import importlib
    mitsuba.set_variant('scalar_rgb')
    func     = wrapper(getattr(importlib.import_module('mitsuba.core').warp, func_str))
    pdf_func = wrapper(getattr(importlib.import_module('mitsuba.core').warp, func_str + "_pdf"))

    try:
        mitsuba.set_variant('packet_rgb')
    except:
        pytest.skip("packet_rgb mode not enabled")

    func_vec     = wrapper(getattr(importlib.import_module('mitsuba.core').warp, func_str))
    pdf_func_vec = wrapper(getattr(importlib.import_module('mitsuba.core').warp, func_str + "_pdf"))
    from mitsuba.core import Vector2f

    # Generate resolution^2 test points on a 2D grid
    t = ek.linspace(Float, 1e-3, 1, resolution)
    x, y = ek.meshgrid(t, t)
    samples = Vector2f(x, y)

    # Run the sampling routine
    result = func_vec(samples)

    # Evaluate the PDF
    pdf = pdf_func_vec(result)

    # Check against the scalar version
    for i in range(resolution):
        assert ek.allclose(result.numpy()[i, :], func(samples.numpy()[i, :]), atol=1e-4)
        assert ek.allclose(pdf.numpy()[i], pdf_func(result.numpy()[i, :]), atol=1e-6)


def check_inverse(func, inverse):
    for x in ek.linspace(Float, 1e-6, 1-1e-6, 10):
        for y in ek.linspace(Float, 1e-6, 1-1e-6, 10):
            p1 = np.array([x, y])
            p2 = func(p1)
            p3 = inverse(p2)
            assert(ek.allclose(p1, p3, atol=1e-5))


def test_square_to_uniform_disk(variant_scalar_rgb):
    from mitsuba.core import warp

    assert(ek.allclose(warp.square_to_uniform_disk([0.5, 0]), [0, 0]))
    assert(ek.allclose(warp.square_to_uniform_disk([0, 1]),   [1, 0]))
    assert(ek.allclose(warp.square_to_uniform_disk([0.5, 1]), [-1, 0], atol=1e-7))
    assert(ek.allclose(warp.square_to_uniform_disk([1, 1]),   [1, 0], atol=1e-6))

    check_inverse(warp.square_to_uniform_disk, warp.uniform_disk_to_square)

    check_vectorization("square_to_uniform_disk")


def test_square_to_uniform_disk_concentric(variant_scalar_rgb):
    from mitsuba.core import warp
    from math import sqrt

    assert(ek.allclose(warp.square_to_uniform_disk_concentric([0, 0]), ([-1 / sqrt(2),  -1 / sqrt(2)])))
    assert(ek.allclose(warp.square_to_uniform_disk_concentric([0.5, .5]), [0, 0]))

    check_inverse(warp.square_to_uniform_disk_concentric, warp.uniform_disk_to_square_concentric)

    check_vectorization("square_to_uniform_disk_concentric")


def test_square_to_uniform_triangle(variant_scalar_rgb):
    from mitsuba.core import warp

    assert(ek.allclose(warp.square_to_uniform_triangle([0, 0]),   [0, 0]))
    assert(ek.allclose(warp.square_to_uniform_triangle([0, 0.1]), [0, 0.1]))
    assert(ek.allclose(warp.square_to_uniform_triangle([0, 1]),   [0, 1]))
    assert(ek.allclose(warp.square_to_uniform_triangle([1, 0]),   [1, 0]))
    assert(ek.allclose(warp.square_to_uniform_triangle([1, 0.5]), [1, 0]))
    assert(ek.allclose(warp.square_to_uniform_triangle([1, 1]),   [1, 0]))

    check_inverse(warp.square_to_uniform_triangle, warp.uniform_triangle_to_square)
    check_vectorization("square_to_uniform_triangle")


def test_interval_to_tent(variant_scalar_rgb):
    from mitsuba.core import warp

    assert(ek.allclose(warp.interval_to_tent(0.5), 0))
    assert(ek.allclose(warp.interval_to_tent(0),   -1))
    assert(ek.allclose(warp.interval_to_tent(1),   1))


def test_interval_to_nonuniform_tent(variant_scalar_rgb):
    from mitsuba.core import warp

    assert(ek.allclose(warp.interval_to_nonuniform_tent(0, 0.5, 1, 0.499), 0.499, atol=1e-3))
    assert(ek.allclose(warp.interval_to_nonuniform_tent(0, 0.5, 1, 0), 0))
    assert(ek.allclose(warp.interval_to_nonuniform_tent(0, 0.5, 1, 0.5), 1))


def test_square_to_tent(variant_scalar_rgb):
    from mitsuba.core import warp

    assert(ek.allclose(warp.square_to_tent([0.5, 0.5]), [0, 0]))
    assert(ek.allclose(warp.square_to_tent([0, 0.5]), [-1, 0]))
    assert(ek.allclose(warp.square_to_tent([1, 0]), [1, -1]))

    check_inverse(warp.square_to_tent, warp.tent_to_square)
    check_vectorization("square_to_tent")


def test_square_to_uniform_sphere_vec(variant_scalar_rgb):
    from mitsuba.core import warp

    assert(ek.allclose(warp.square_to_uniform_sphere([0, 0]), [0, 0,  1]))
    assert(ek.allclose(warp.square_to_uniform_sphere([0, 1]), [0, 0, -1]))
    assert(ek.allclose(warp.square_to_uniform_sphere([0.5, 0.5]), [-1, 0, 0], atol=1e-7))

    check_inverse(warp.square_to_uniform_sphere, warp.uniform_sphere_to_square)
    check_vectorization("square_to_uniform_sphere")


def test_square_to_uniform_hemisphere(variant_scalar_rgb):
    from mitsuba.core import warp

    assert(ek.allclose(warp.square_to_uniform_hemisphere([0.5, 0.5]), [0, 0, 1]))
    assert(ek.allclose(warp.square_to_uniform_hemisphere([0, 0.5]), [-1, 0, 0]))

    check_inverse(warp.square_to_uniform_hemisphere, warp.uniform_hemisphere_to_square)
    check_vectorization("square_to_uniform_hemisphere")


def test_square_to_cosine_hemisphere(variant_scalar_rgb):
    from mitsuba.core import warp

    assert(ek.allclose(warp.square_to_cosine_hemisphere([0.5, 0.5]), [0,  0,  1]))
    assert(ek.allclose(warp.square_to_cosine_hemisphere([0.5,   0]), [0, -1, 0], atol=1e-7))

    check_inverse(warp.square_to_cosine_hemisphere, warp.cosine_hemisphere_to_square)
    check_vectorization("square_to_cosine_hemisphere")


def test_square_to_uniform_cone(variant_scalar_rgb):
    from mitsuba.core import warp

    assert(ek.allclose(warp.square_to_uniform_cone([0.5, 0.5], 1), [0, 0, 1]))
    assert(ek.allclose(warp.square_to_uniform_cone([0.5, 0],   1), [0, 0, 1], atol=1e-7))
    assert(ek.allclose(warp.square_to_uniform_cone([0.5, 0],   0), [0, -1, 0], atol=1e-7))

    fwd = lambda v: warp.square_to_uniform_cone(v, 0.3)
    inv = lambda v: warp.uniform_cone_to_square(v, 0.3)

    check_inverse(fwd, inv)

    wrapper = lambda f: lambda x: f(x, 0.3)

    check_vectorization("square_to_uniform_cone", wrapper)


def test_square_to_beckmann(variant_scalar_rgb):
    from mitsuba.core import warp

    fwd = lambda v: warp.square_to_beckmann(v, 0.3)
    pdf = lambda v: warp.square_to_beckmann_pdf(v, 0.3)
    inv = lambda v: warp.beckmann_to_square(v, 0.3)

    check_inverse(fwd, inv)

    wrapper = lambda f: lambda x: f(x, 0.3)

    check_vectorization("square_to_beckmann", wrapper)


def test_square_to_von_mises_fisher(variant_scalar_rgb):
    from mitsuba.core import warp

    fwd = lambda v: warp.square_to_von_mises_fisher(v, 10)
    pdf = lambda v: warp.square_to_von_mises_fisher_pdf(v, 10)
    inv = lambda v: warp.von_mises_fisher_to_square(v, 10)

    check_inverse(fwd, inv)

    wrapper = lambda f: lambda x: f(x, 10)

    check_vectorization("square_to_von_mises_fisher", wrapper)


def test_square_to_std_normal_pdf(variant_scalar_rgb):
    from mitsuba.core import warp
    assert(ek.allclose(warp.square_to_std_normal_pdf([0, 0]),   0.16, atol=1e-2))
    assert(ek.allclose(warp.square_to_std_normal_pdf([0, 0.8]), 0.12, atol=1e-2))
    assert(ek.allclose(warp.square_to_std_normal_pdf([0.8, 0]), 0.12, atol=1e-2))


def test_square_to_std_normal(variant_scalar_rgb):
    from mitsuba.core import warp
    assert(ek.allclose(warp.square_to_std_normal([0, 0]), [0, 0]))
    assert(ek.allclose(warp.square_to_std_normal([0, 1]), [0, 0]))
    assert(ek.allclose(warp.square_to_std_normal([0.39346, 0]), [1, 0], atol=1e-3))


def test_interval_to_linear(variant_scalar_rgb):
    from mitsuba.core.warp import interval_to_linear, linear_to_interval
    x = interval_to_linear(3, 5., .3)
    assert ek.allclose(linear_to_interval(3, 5, x), .3)


def test_square_to_bilinear(variant_scalar_rgb):
    from mitsuba.core.warp import square_to_bilinear, \
        bilinear_to_square, square_to_bilinear_pdf

    # Refrence values generated by Mathematica
    values = [1, 3, 5, 7]
    p, pdf = square_to_bilinear(*values, [0.3, 0.2])
    assert ek.allclose(p.y, 0.306226)
    assert ek.allclose(p.x, 0.372479)
    assert ek.allclose(pdf, 2.96986)
    p2, pdf2 = bilinear_to_square(*values, p)
    assert ek.allclose(p2.y, 0.2)
    assert ek.allclose(p2.x, 0.3)
    assert ek.allclose(pdf2, pdf)
    pdf3 = square_to_bilinear_pdf(*values, p),
    assert ek.allclose(pdf3, pdf)
