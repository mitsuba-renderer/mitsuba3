import numpy as np

def check_vectorization(func, pdf_func, resolution = 10):
    """
    Helper routine which compares evaluations of the vectorized and
    non-vectorized version of a warping routine
    """
    # Generate resolution^2 test points on a 2D grid
    t = np.linspace(0, 1, resolution)
    x, y = np.meshgrid(t, t)
    samples = np.float32(np.column_stack((x.ravel(), y.ravel())))

    # Run the sampling routine
    result = func(samples)

    # Evaluate the PDF
    pdf = pdf_func(result)

    # Check against the scalar version
    for i in range(samples.shape[1]):
        assert np.allclose(result[i, :][2], func(samples[i, :])[2], atol=1e-7)
        pdf_func(result[i, :])
        assert np.allclose(pdf[i], pdf_func(result[i, :]), atol=1e-7)


def test_square_to_uniform_sphere():
    from mitsuba.core.warp import square_to_uniform_sphere

    assert(np.allclose(square_to_uniform_sphere([0, 0]), [0, 0,  1]))
    assert(np.allclose(square_to_uniform_sphere([0, 1]), [0, 0, -1]))
    assert(np.allclose(square_to_uniform_sphere([0.5, 0.5]), [-1, 0, 0], atol=1e-7))

def test_square_to_uniform_sphere_vec():
    from mitsuba.core.warp import square_to_uniform_sphere
    from mitsuba.core.warp import square_to_uniform_sphere_pdf
    check_vectorization(square_to_uniform_sphere, square_to_uniform_sphere_pdf)


def test_square_to_uniform_hemisphere():
    from mitsuba.core.warp import square_to_uniform_hemisphere
    assert(np.allclose(square_to_uniform_hemisphere([0.5, 0.5]), [0, 0, 1]))
    assert(np.allclose(square_to_uniform_hemisphere([0, 0.5]), [-1, 0, 0]))


def test_square_to_uniform_hemisphere_vec():
    from mitsuba.core.warp import square_to_uniform_hemisphere
    from mitsuba.core.warp import square_to_uniform_hemisphere_pdf
    check_vectorization(square_to_uniform_hemisphere, square_to_uniform_hemisphere_pdf)


def test_square_to_uniform_disk_concentric():
    from mitsuba.core.warp import square_to_uniform_disk_concentric
    from math import sqrt
    assert(np.allclose(square_to_uniform_disk_concentric([0, 0]), ([-1 / sqrt(2),  -1 / sqrt(2)])))
    assert(np.allclose(square_to_uniform_disk_concentric([0.5, .5]), [0, 0]))


def test_square_to_cosine_hemisphere():
    from mitsuba.core.warp import square_to_cosine_hemisphere
    assert(np.allclose(square_to_cosine_hemisphere([0.5, 0.5]), [0,  0,  1]))
    assert(np.allclose(square_to_cosine_hemisphere([0.5,   0]), [0, -1, 0], atol=1e-7))


def test_square_to_uniform_cone():
    from mitsuba.core.warp import square_to_uniform_cone
    assert(np.allclose(square_to_uniform_cone([0.5, 0.5], 1), [0, 0, 1]))
    assert(np.allclose(square_to_uniform_cone([0.5, 0],   1), [0, 0, 1], atol=1e-7))
    assert(np.allclose(square_to_uniform_cone([0.5, 0],   0), [0, 0, 1], atol=1e-7))


def test_square_to_uniform_disk():
    from mitsuba.core.warp import square_to_uniform_disk
    assert(np.allclose(square_to_uniform_disk([0.5, 0]), [0, 0]))
    assert(np.allclose(square_to_uniform_disk([0, 1]),   [1, 0]))
    assert(np.allclose(square_to_uniform_disk([0.5, 1]), [-1, 0], atol=1e-7))
    assert(np.allclose(square_to_uniform_disk([1, 1]),   [1, 0], atol=1e-6))


def test_uniform_disk_to_square_concentric():
    from mitsuba.core.warp import square_to_uniform_disk_concentric
    from mitsuba.core.warp import uniform_disk_to_square_concentric
    assert(np.allclose(square_to_uniform_disk_concentric(uniform_disk_to_square_concentric([0, 0])),      [0, 0]))
    assert(np.allclose(square_to_uniform_disk_concentric(uniform_disk_to_square_concentric([0.5, 0.5])),  [0.5, 0.5]))
    assert(np.allclose(square_to_uniform_disk_concentric(uniform_disk_to_square_concentric([0.25, 0.5])), [0.25, 0.5]))
    assert(np.allclose(square_to_uniform_disk_concentric(uniform_disk_to_square_concentric([0.5, 0.25])), [0.5, 0.25]))

    for x in np.linspace(0, 1, 10):
        for y in np.linspace(0, 1, 10):
            p1 = np.array([x, y])
            p2 = square_to_uniform_disk_concentric(p1)
            p3 = uniform_disk_to_square_concentric(p2)
            assert(np.allclose(p1, p3, atol=1e-6))


def test_square_to_uniform_triangle():
    from mitsuba.core.warp import square_to_uniform_triangle
    assert(np.allclose(square_to_uniform_triangle([0, 0]),   [0, 0]))
    assert(np.allclose(square_to_uniform_triangle([0, 0.1]), [0, 0.1]))
    assert(np.allclose(square_to_uniform_triangle([0, 1]),   [0, 1]))
    assert(np.allclose(square_to_uniform_triangle([1, 0]),   [1, 0]))
    assert(np.allclose(square_to_uniform_triangle([1, 0.5]), [1, 0]))
    assert(np.allclose(square_to_uniform_triangle([1, 1]),   [1, 0]))


def test_square_to_std_normal_pdf():
    from mitsuba.core.warp import square_to_std_normal_pdf
    assert(np.allclose(square_to_std_normal_pdf([0, 0]),   0.16, atol=1e-2))
    assert(np.allclose(square_to_std_normal_pdf([0, 0.8]), 0.12, atol=1e-2))
    assert(np.allclose(square_to_std_normal_pdf([0.8, 0]), 0.12, atol=1e-2))


def test_interval_to_tent():
    from mitsuba.core.warp import interval_to_tent
    assert(np.allclose(interval_to_tent(0.5), 0))
    assert(np.allclose(interval_to_tent(0),   1))
    assert(np.allclose(interval_to_tent(1),   -1))


def test_interval_to_nonuniform_tent():
    from mitsuba.core.warp import interval_to_nonuniform_tent
    assert(np.allclose(interval_to_nonuniform_tent(0, 0.5, 1, 0.499), 0.499, atol=1e-3))
    assert(np.allclose(interval_to_nonuniform_tent(0, 0.5, 1, 0), 0))
    assert(np.allclose(interval_to_nonuniform_tent(0, 0.5, 1, 0.5), 1))


def test_square_to_tent():
    from mitsuba.core.warp import square_to_tent
    assert(np.allclose(square_to_tent([0.5, 0.5]), [0, 0]))
    assert(np.allclose(square_to_tent([0, 0.5]), [1, 0]))
    assert(np.allclose(square_to_tent([1, 0]), [-1, 1]))


def test_square_to_std_normal():
    from mitsuba.core.warp import square_to_std_normal
    assert(np.allclose(square_to_std_normal([0, 0]), [0, 0]))
    assert(np.allclose(square_to_std_normal([0, 1]), [0, 0]))
    assert(np.allclose(square_to_std_normal([0.39346, 0]), [1, 0], atol=1e-3))

# TODO Test
#       square_to_beckmann
#
#       pdf functions
