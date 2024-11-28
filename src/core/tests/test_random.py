import pytest
import drjit as dr
import mitsuba as mi

import numpy as np


def test01_tea_float32(variant_scalar_rgb):
    assert mi.sample_tea_float32(1, 1, 4) == 0.5424730777740479
    assert mi.sample_tea_float32(1, 2, 4) == 0.5079904794692993
    assert mi.sample_tea_float32(1, 3, 4) == 0.4171961545944214
    assert mi.sample_tea_float32(1, 4, 4) == 0.008385419845581055
    assert mi.sample_tea_float32(1, 5, 4) == 0.8085528612136841
    assert mi.sample_tea_float32(2, 1, 4) == 0.6939879655838013
    assert mi.sample_tea_float32(3, 1, 4) == 0.6978365182876587
    assert mi.sample_tea_float32(4, 1, 4) == 0.4897364377975464


def test02_tea_float64(variant_scalar_rgb):
    assert mi.sample_tea_float64(1, 1, 4) == 0.5424730799533735
    assert mi.sample_tea_float64(1, 2, 4) == 0.5079905082233922
    assert mi.sample_tea_float64(1, 3, 4) == 0.4171962610608142
    assert mi.sample_tea_float64(1, 4, 4) == 0.008385529523330604
    assert mi.sample_tea_float64(1, 5, 4) == 0.80855288317879
    assert mi.sample_tea_float64(2, 1, 4) == 0.6939880404156831
    assert mi.sample_tea_float64(3, 1, 4) == 0.6978365636630994
    assert mi.sample_tea_float64(4, 1, 4) == 0.48973647949223253


def test03_tea_vectorized(variant_scalar_rgb):
    from mitsuba.test.util import check_vectorization

    width = 125

    def kernel(x : float):
        i = mi.UInt32(width * x)
        return mi.sample_tea_float32(1, i, 4), mi.sample_tea_float64(1, i, 4)

    check_vectorization(kernel, width=width)


def test04_permute_bijection(variants_all_backends_once):
    """Check that the resulting permutation vectors are bijections"""
    for p in range(3):
        sample_count = 2**(p+1)
        for seed in range(75):
            perm = [mi.permute(i, sample_count, seed) for i in range(sample_count)]
            assert len(set(perm)) == len(perm)


def test05_permute_uniform(variant_scalar_rgb):
    """Chi2 tests to check that the permutation vectors are uniformly distributed"""
    for p in range(4):
        sample_count = 2**(p+1)
        histogram = np.zeros((sample_count, sample_count))

        N = 1000
        for seed in range(N):
            for i in range(sample_count):
                j = mi.permute(i, sample_count, seed)
                histogram[i, j] += 1

        histogram /= N

        mean = np.mean(histogram)
        assert dr.allclose(1.0 / sample_count, mean)


def test06_permute_kensler_bijection(variants_all_backends_once):
    """Check that the resulting permutation vectors are bijections"""
    for p in range(3):
        sample_count = 2**(p+1) + p
        for seed in range(75):
            perm = [mi.permute_kensler(i, sample_count, seed) for i in range(sample_count)]
            assert len(set(perm)) == len(perm)


def test07_permute_kensler_uniform(variant_scalar_rgb):
    """Chi2 tests to check that the permutation vectors are uniformly distributed"""
    for p in range(4):
        sample_count = 2**(p+1) + p
        histogram = np.zeros((sample_count, sample_count))

        N = 1000
        for seed in range(N):
            for i in range(sample_count):
                j = mi.permute_kensler(i, sample_count, seed)
                histogram[i, j] += 1

        histogram /= N

        mean = np.mean(histogram)
        assert dr.allclose(1.0 / sample_count, mean)

def test08_sample_tea_float(variants_all):
    """Check that the return type of `mi.sample_tea_float` is correct, given the chosen variant."""
    f = mi.sample_tea_float(mi.UInt(0), mi.UInt(0))
    assert type(f) == mi.Float
