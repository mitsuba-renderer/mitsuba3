import numpy as np
import pytest
import drjit as dr
from drjit.scalar import ArrayXf as Float
import mitsuba as mi


def check_warp_vectorization(func_str, wrapper = (lambda f: lambda x: f(x)), atol=1e-6):
    """
    Helper routine which compares evaluations of the vectorized and
    non-vectorized version of a warping routine.
    """

    def kernel(u : float, v : float):
        func_vec     = wrapper(getattr(mi.warp, func_str))
        pdf_func_vec = wrapper(getattr(mi.warp, func_str + "_pdf"))

        result = func_vec([u, v])
        pdf = pdf_func_vec(result)

        return result

    from mitsuba.test.util import check_vectorization
    check_vectorization(kernel, atol=atol)


def check_inverse(func, inverse, atol=1e-5):
    for x in dr.linspace(Float, 1e-6, 1-1e-6, 10):
        for y in dr.linspace(Float, 1e-6, 1-1e-6, 10):
            p1 = dr.scalar.Array2f(x, y)
            p2 = func(p1)
            p3 = inverse(p2)
            assert(dr.allclose(p1, p3, atol=atol))


def test_square_to_uniform_disk(variant_scalar_rgb):
    assert(dr.allclose(mi.warp.square_to_uniform_disk([0.5, 0]), [0, 0]))
    assert(dr.allclose(mi.warp.square_to_uniform_disk([0, 1]),   [1, 0]))
    assert(dr.allclose(mi.warp.square_to_uniform_disk([0.5, 1]), [-1, 0], atol=1e-7))
    assert(dr.allclose(mi.warp.square_to_uniform_disk([1, 1]),   [1, 0], atol=1e-6))

    check_inverse(mi.warp.square_to_uniform_disk, mi.warp.uniform_disk_to_square)

    check_warp_vectorization("square_to_uniform_disk")


def test_square_to_uniform_disk_concentric(variant_scalar_rgb):
    from math import sqrt

    assert(dr.allclose(mi.warp.square_to_uniform_disk_concentric([0, 0]), ([-1 / sqrt(2),  -1 / sqrt(2)])))
    assert(dr.allclose(mi.warp.square_to_uniform_disk_concentric([0.5, .5]), [0, 0]))

    check_inverse(mi.warp.square_to_uniform_disk_concentric, mi.warp.uniform_disk_to_square_concentric)

    check_warp_vectorization("square_to_uniform_disk_concentric")


def test_square_to_uniform_triangle(variant_scalar_rgb):
    assert(dr.allclose(mi.warp.square_to_uniform_triangle([0, 0]),   [0, 0]))
    assert(dr.allclose(mi.warp.square_to_uniform_triangle([0, 0.1]), [0, 0.1]))
    assert(dr.allclose(mi.warp.square_to_uniform_triangle([0, 1]),   [0, 1]))
    assert(dr.allclose(mi.warp.square_to_uniform_triangle([1, 0]),   [1, 0]))
    assert(dr.allclose(mi.warp.square_to_uniform_triangle([1, 0.5]), [1, 0]))
    assert(dr.allclose(mi.warp.square_to_uniform_triangle([1, 1]),   [1, 0]))

    check_inverse(mi.warp.square_to_uniform_triangle, mi.warp.uniform_triangle_to_square)
    check_warp_vectorization("square_to_uniform_triangle")


def test_interval_to_tent(variant_scalar_rgb):
    assert(dr.allclose(mi.warp.interval_to_tent(0.5), 0))
    assert(dr.allclose(mi.warp.interval_to_tent(0),   -1))
    assert(dr.allclose(mi.warp.interval_to_tent(1),   1))


def test_interval_to_nonuniform_tent(variant_scalar_rgb):
    assert(dr.allclose(mi.warp.interval_to_nonuniform_tent(0, 0.5, 1, 0.499), 0.499, atol=1e-3))
    assert(dr.allclose(mi.warp.interval_to_nonuniform_tent(0, 0.5, 1, 0), 0))
    assert(dr.allclose(mi.warp.interval_to_nonuniform_tent(0, 0.5, 1, 0.5), 1))


def test_square_to_tent(variant_scalar_rgb):
    assert(dr.allclose(mi.warp.square_to_tent([0.5, 0.5]), [0, 0]))
    assert(dr.allclose(mi.warp.square_to_tent([0, 0.5]), [-1, 0]))
    assert(dr.allclose(mi.warp.square_to_tent([1, 0]), [1, -1]))

    check_inverse(mi.warp.square_to_tent, mi.warp.tent_to_square)
    check_warp_vectorization("square_to_tent")


def test_square_to_uniform_sphere_vec(variant_scalar_rgb):
    assert(dr.allclose(mi.warp.square_to_uniform_sphere([0, 0]), [0, 0,  1]))
    assert(dr.allclose(mi.warp.square_to_uniform_sphere([0, 1]), [0, 0, -1]))
    assert(dr.allclose(mi.warp.square_to_uniform_sphere([0.5, 0.5]), [-1, 0, 0], atol=1e-7))

    check_inverse(mi.warp.square_to_uniform_sphere, mi.warp.uniform_sphere_to_square)
    check_warp_vectorization("square_to_uniform_sphere")


def test_square_to_uniform_hemisphere(variant_scalar_rgb):
    assert(dr.allclose(mi.warp.square_to_uniform_hemisphere([0.5, 0.5]), [0, 0, 1]))
    assert(dr.allclose(mi.warp.square_to_uniform_hemisphere([0, 0.5]), [-1, 0, 0]))

    check_inverse(mi.warp.square_to_uniform_hemisphere, mi.warp.uniform_hemisphere_to_square)
    check_warp_vectorization("square_to_uniform_hemisphere")


def test_square_to_cosine_hemisphere(variant_scalar_rgb):
    assert(dr.allclose(mi.warp.square_to_cosine_hemisphere([0.5, 0.5]), [0,  0,  1]))
    assert(dr.allclose(mi.warp.square_to_cosine_hemisphere([0.5,   0]), [0, -1, 0], atol=1e-7))

    check_inverse(mi.warp.square_to_cosine_hemisphere, mi.warp.cosine_hemisphere_to_square)
    check_warp_vectorization("square_to_cosine_hemisphere", atol=1e-5)


def test_square_to_uniform_cone(variant_scalar_rgb):
    assert(dr.allclose(mi.warp.square_to_uniform_cone([0.5, 0.5], 1), [0, 0, 1]))
    assert(dr.allclose(mi.warp.square_to_uniform_cone([0.5, 0],   1), [0, 0, 1], atol=1e-7))
    assert(dr.allclose(mi.warp.square_to_uniform_cone([0.5, 0],   0), [0, -1, 0], atol=1e-7))

    fwd = lambda v: mi.warp.square_to_uniform_cone(v, 0.3)
    inv = lambda v: mi.warp.uniform_cone_to_square(v, 0.3)

    check_inverse(fwd, inv)

    wrapper = lambda f: lambda x: f(x, 0.3)

    check_warp_vectorization("square_to_uniform_cone", wrapper)


def test_square_to_beckmann(variant_scalar_rgb):
    fwd = lambda v: mi.warp.square_to_beckmann(v, 0.3)
    pdf = lambda v: mi.warp.square_to_beckmann_pdf(v, 0.3)
    inv = lambda v: mi.warp.beckmann_to_square(v, 0.3)

    check_inverse(fwd, inv)

    wrapper = lambda f: lambda x: f(x, 0.3)

    check_warp_vectorization("square_to_beckmann", wrapper, atol=1e-5)


def test_square_to_von_mises_fisher(variant_scalar_rgb):
    fwd = lambda v: mi.warp.square_to_von_mises_fisher(v, 10)
    pdf = lambda v: mi.warp.square_to_von_mises_fisher_pdf(v, 10)
    inv = lambda v: mi.warp.von_mises_fisher_to_square(v, 10)

    check_inverse(fwd, inv)

    wrapper = lambda f: lambda x: f(x, 10)

    check_warp_vectorization("square_to_von_mises_fisher", wrapper)


def test_square_to_std_normal_pdf(variant_scalar_rgb):
    assert(dr.allclose(mi.warp.square_to_std_normal_pdf([0, 0]),   0.16, atol=1e-2))
    assert(dr.allclose(mi.warp.square_to_std_normal_pdf([0, 0.8]), 0.12, atol=1e-2))
    assert(dr.allclose(mi.warp.square_to_std_normal_pdf([0.8, 0]), 0.12, atol=1e-2))


def test_square_to_std_normal(variant_scalar_rgb):
    assert(dr.allclose(mi.warp.square_to_std_normal([0, 0]), [0, 0]))
    assert(dr.allclose(mi.warp.square_to_std_normal([0, 1]), [0, 0]))
    assert(dr.allclose(mi.warp.square_to_std_normal([0.39346, 0]), [1, 0], atol=1e-3))


def test_interval_to_linear(variant_scalar_rgb):
    x = mi.warp.interval_to_linear(3, 5., .3)
    assert dr.allclose(mi.warp.linear_to_interval(3, 5, x), .3)


def test_square_to_bilinear(variant_scalar_rgb):
    # Reference values generated by Mathematica
    values = [1, 3, 5, 7]
    p, pdf = mi.warp.square_to_bilinear(*values, [0.3, 0.2])
    assert dr.allclose(p.y, 0.306226)
    assert dr.allclose(p.x, 0.372479)
    assert dr.allclose(pdf, 2.96986)
    p2, pdf2 = mi.warp.bilinear_to_square(*values, p)
    assert dr.allclose(p2.y, 0.2)
    assert dr.allclose(p2.x, 0.3)
    assert dr.allclose(pdf2, pdf)
    pdf3 = mi.warp.square_to_bilinear_pdf(*values, p),
    assert dr.allclose(pdf3, pdf)


def test_interval_to_tangent_direction(variant_scalar_rgb):
    from mitsuba.test.util import check_vectorization

    for x in dr.linspace(Float, 1e-6, 1-1e-6, 5):
        for y in dr.linspace(Float, 1e-6, 1-1e-6, 5):
            n = mi.warp.square_to_uniform_sphere([x, y])

            for sample in dr.linspace(Float, 1e-6, 1-1e-6, 10):
                d = mi.warp.interval_to_tangent_direction(n, sample)
                assert dr.allclose(dr.dot(d, n), 0, atol=1e-6)
                assert dr.allclose(mi.warp.tangent_direction_to_interval(n, d), sample, atol=1e-6)

            def kernel(n, sample):
                n = dr.normalize(n)
                d = mi.warp.interval_to_tangent_direction(n, sample)
                out_sample = mi.warp.tangent_direction_to_interval(n, d)
                return d, out_sample

            check_vectorization(kernel, arg_dims=[3, 1], atol=1e-6)
            mi.set_variant('scalar_rgb')


def test_square_to_uniform_lune(variants_vec_rgb):
    n1 = dr.normalize(mi.Vector3f(-0.1, 0.8, 0.2))
    n2 = dr.normalize(mi.Vector3f(0.2, 0.7, 0.2))
    fwd = lambda v: mi.warp.square_to_uniform_spherical_lune(v, n1, n2)
    inv = lambda v: mi.warp.uniform_spherical_lune_to_square(v, n1, n2)

    check_inverse(fwd, inv, atol=1e-4)
