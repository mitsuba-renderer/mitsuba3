import mitsuba
import numpy as np
import pytest

from mitsuba import DEBUG as is_debug
from mitsuba.core import DiscreteDistribution

eps = 1e-6
def approx(v):
    return pytest.approx(v, abs=eps)

def _get_example_distribution(normalized, pdf_values = [0.5, 0.3, 0.0, 0.1]):
    d = DiscreteDistribution(len(pdf_values))
    for v in pdf_values:
        d.append(v)
    if normalized:
        d.normalize()
    return d

def test01_construction():
    # Construction with default size value (0)
    _ = DiscreteDistribution()
    # Construction with size > 0
    pdf_values = [1.5, 0.5, 0.3, 0.0, 0.1, 6]
    d = _get_example_distribution(False, pdf_values)
    for (i, v) in enumerate(pdf_values):
        assert(d[i] == approx(v))
    assert(d.size() == len(pdf_values))

    # Clearing
    d.clear()
    assert(d.size() == 0)
    assert(not d.normalized())
    # Reserving should not change the size
    d.reserve(3)
    assert(d.size() == 0)

def test02_normalization():
    pdf_values = [1.5, 0.5, 0.3, 0.1, 6]
    n = sum(pdf_values)
    d = _get_example_distribution(True, pdf_values)
    for (i, v) in enumerate(pdf_values):
        assert(d[i] == approx(v / n))
    assert(d.normalized())
    assert(d.sum() == approx(n))
    assert(d.normalization() == approx(1.0 / n))

def test03_empty():
    # Sampling from an empty distribution has undefined behavior, so we don't
    # test for it.
    d = DiscreteDistribution()
    assert(d.size() == 0)
    if is_debug:
        with pytest.raises(RuntimeError):
            d.normalize()

    # Sampling from a distribution which has null probability
    d.append(0.0)
    d.append(0.0)
    d.append(0.0)
    assert(d.size() == 3)
    assert(d.normalize() == 0.0)
    assert(d.sum() == 0.0)
    assert(d.normalization() == np.inf)
    assert(d.sample(0.00) == 0)
    assert(d.sample(0.50) == 0)
    assert(d.sample(0.99) == 0)
    assert(d.sample(1.00) == 0)

def test04_sample_pdf():
    # Should work with and without calling `normalize`
    pdf_values = [0.0, 0.0, 0.25, 0.25, 0.0, 0.0, 0.0, 0.5, 0.0, 0.0]
    distributions = [
        _get_example_distribution(False, pdf_values),
        _get_example_distribution(True, pdf_values)]
    for d in distributions:
        assert(d.size() == len(pdf_values))
        assert(d.sample_pdf(0.00) == (2, 0.25))
        assert(d.sample_pdf(0.10) == (2, 0.25))
        assert(d.sample_pdf(0.25) == (3, 0.25))
        assert(d.sample_pdf(0.26) == (3, 0.25))
        assert(d.sample_pdf(0.49) == (3, 0.25))
        assert(d.sample_pdf(0.50) == (7, 0.50))
        assert(d.sample_pdf(0.51) == (7, 0.50))
        assert(d.sample_pdf(0.99) == (7, 0.50))
        assert(d.sample_pdf(0.9999999) == (7, 0.50))
        assert(d.sample_pdf(1.0) == (7, 0.50))

    # No problematic entries (represents common use-case).
    pdf_values = [0.25, 0.30, 0.45]
    distributions = [
        _get_example_distribution(False, pdf_values),
        _get_example_distribution(True, pdf_values)]
    for d in distributions:
        assert(d.size() == len(pdf_values))
        assert(d.sample_pdf(0.00) == (0, 0.25))
        assert(d.sample_pdf(0.10) == (0, 0.25))
        assert(d.sample_pdf(0.26) == (1, approx(0.30)))
        assert(d.sample_pdf(0.30) == (1, approx(0.30)))
        assert(d.sample_pdf(0.31) == (1, approx(0.30)))
        assert(d.sample_pdf(0.49) == (1, approx(0.30)))
        assert(d.sample_pdf(0.51) == (1, approx(0.30)))
        assert(d.sample_pdf(0.55) == (2, approx(0.45)))
        assert(d.sample_pdf(0.99) == (2, approx(0.45)))
        assert(d.sample_pdf(0.9999999) == (2, approx(0.45)))
        assert(d.sample_pdf(1.0) == (2, approx(0.45)))


def test05_sample_reuse_pdf():
    # Challenging case
    pdf_values = [0.0, 0.0, 0.25, 0.25, 0.0, 0.0, 0.5, 0.0, 0.0]
    # Should work with and without calling `normalize`
    distributions = [
        _get_example_distribution(False, pdf_values),
        _get_example_distribution(True, pdf_values)]
    for d in distributions:
        assert(d.size() == len(pdf_values))
        assert(d.sample_reuse_pdf(0.00) == (2, 0.25, 0.00))
        assert(d.sample_reuse_pdf(0.10) == (2, 0.25, approx(0.10 / 0.25)))
        assert(d.sample_reuse_pdf(0.26) == (3, 0.25, approx(0.01 / 0.25)))
        assert(d.sample_reuse_pdf(0.49) == (3, 0.25, approx(0.24 / 0.25)))
        assert(d.sample_reuse_pdf(0.50) == (6, 0.50, 0.0))
        assert(d.sample_reuse_pdf(0.51) == (6, 0.50, approx(0.01 / 0.50)))
        assert(d.sample_reuse_pdf(0.99) == (6, 0.50, approx(0.49 / 0.50)))
        assert(d.sample_reuse_pdf(0.9999999) == (6, 0.50, approx(0.4999999 / 0.50)))
        assert(d.sample_reuse_pdf(1.0) == (6, 0.50, approx(1.0)))

    # Less problematic values (represents common use-case).
    pdf_values = [0.25, 0.30, 0.45]
    distributions = [
        _get_example_distribution(False, pdf_values),
        _get_example_distribution(True, pdf_values)]
    for d in distributions:
        assert(d.size() == len(pdf_values))
        assert(d.sample_reuse_pdf(0.00) == (0, 0.25, 0.00))
        assert(d.sample_reuse_pdf(0.10) == (0, 0.25,         approx(0.10 / 0.25)))
        assert(d.sample_reuse_pdf(0.25) == (1, approx(0.30), approx(0.0)))
        assert(d.sample_reuse_pdf(0.26) == (1, approx(0.30), approx(0.01 / 0.30)))
        assert(d.sample_reuse_pdf(0.49) == (1, approx(0.30), approx(0.24 / 0.30)))
        assert(d.sample_reuse_pdf(0.50) == (1, approx(0.30), approx(0.25 / 0.30)))
        assert(d.sample_reuse_pdf(0.51) == (1, approx(0.30), approx(0.26 / 0.30)))
        assert(d.sample_reuse_pdf(0.55) == (2, approx(0.45), approx(0.0)))
        assert(d.sample_reuse_pdf(0.56) == (2, approx(0.45), approx(0.01 / 0.45)))
        assert(d.sample_reuse_pdf(0.99) == (2, approx(0.45), approx(0.44 / 0.45)))
        assert(d.sample_reuse_pdf(0.9999999) == (2, approx(0.45), approx(0.4499999 / 0.45)))
        assert(d.sample_reuse_pdf(1.0) == (2, approx(0.45), approx(1.0)))

def test06_print():
    d = DiscreteDistribution(2)
    d.append(0.5)
    d.append(1.5)
    assert(str(d) == "DiscreteDistribution[sum=2, normalized=0, cdf={0, 0.5, 2}]")
    d.normalize()
    assert(str(d) == "DiscreteDistribution[sum=2, normalized=1, cdf={0, 0.25, 1}]")

def test07_negative_append():
    if is_debug:
        d = DiscreteDistribution(2)
        with pytest.raises(RuntimeError):
            d.append(-0.5)

def test08_vectorized_getitem():
    d = _get_example_distribution(False, [0.5, 1.5, 0.0, 1.0, 5.5])

    assert(d[0] == 0.5)
    assert(d[4] == 5.5)
    assert(np.allclose(d[[0, 1]], [0.5, 1.5]))
    assert(np.allclose(d[[0, 2, 4]], [0.5, 0.0, 5.5]))
    assert(np.allclose(d[[4, 1, 2, 0]], [5.5, 1.5, 0.0, 0.5]))
    assert(np.allclose(d[[1, 2, 1, 4, 4, 0]], [1.5, 0.0, 1.5, 5.5, 5.5, 0.5]))

def test09_vectorized_sample_pdf():
    pdf_values = [0.0, 0.0, 0.25, 0.25, 0.0, 0.5, 0.0]
    d = _get_example_distribution(False, pdf_values)

    print(d)
    print(d.sample_pdf([0.0, 0.49, 1.0]))

    assert(np.allclose(d.sample_pdf([0.0, 0.49, 1.0]),
                       ([   2,    3,   5],
                        [0.25, 0.25, 0.5])))
    assert(np.allclose(d.sample_pdf([0.0, 0.49, 1.0, 0.24, 0.51]),
                       ([   2,    3,   5,    2,    5],
                        [0.25, 0.25, 0.5, 0.25, 0.50])))

def test10_vectorized_sample_reuse_pdf():
    pdf_values = [0.0, 0.0, 0.25, 0.25, 0.0, 0.5, 0.0]
    d = _get_example_distribution(False, pdf_values)

    assert(np.allclose(d.sample_reuse_pdf([0.0, 0.49, 1.0]),
                       ([   2,    3,   5],
                        [0.25, 0.25, 0.5],
                        [0.00, 0.96, 1.0])))
    assert(np.allclose(d.sample_reuse_pdf([0.0, 0.49, 1.0, 0.24, 0.51], True),
                       ([   2,    3,   5,    2,    5],
                        [0.25, 0.25, 0.5, 0.25, 0.50],
                        [0.00, 0.96, 1.0, 0.96, 0.02])))


