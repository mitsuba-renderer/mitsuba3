import enoki as ek
import pytest
import mitsuba


def test01_tea_float32(variant_scalar_rgb):
    from mitsuba.core import sample_tea_float32

    assert sample_tea_float32(1, 1, 4) == 0.5424730777740479
    assert sample_tea_float32(1, 2, 4) == 0.5079904794692993
    assert sample_tea_float32(1, 3, 4) == 0.4171961545944214
    assert sample_tea_float32(1, 4, 4) == 0.008385419845581055
    assert sample_tea_float32(1, 5, 4) == 0.8085528612136841
    assert sample_tea_float32(2, 1, 4) == 0.6939879655838013
    assert sample_tea_float32(3, 1, 4) == 0.6978365182876587
    assert sample_tea_float32(4, 1, 4) == 0.4897364377975464


def test02_tea_float64(variant_scalar_rgb):
    from mitsuba.core import sample_tea_float64

    assert sample_tea_float64(1, 1, 4) == 0.5424730799533735
    assert sample_tea_float64(1, 2, 4) == 0.5079905082233922
    assert sample_tea_float64(1, 3, 4) == 0.4171962610608142
    assert sample_tea_float64(1, 4, 4) == 0.008385529523330604
    assert sample_tea_float64(1, 5, 4) == 0.80855288317879
    assert sample_tea_float64(2, 1, 4) == 0.6939880404156831
    assert sample_tea_float64(3, 1, 4) == 0.6978365636630994
    assert sample_tea_float64(4, 1, 4) == 0.48973647949223253


def test03_tea_vectorized(variant_packet_rgb):
    from mitsuba.core import sample_tea_float32, sample_tea_float64, UInt32

    count = 100

    result = sample_tea_float32(
        UInt32.full(1, count),
        UInt32.arange(count), 4)
    for i in range(count):
        assert result[i] == sample_tea_float32(1, i, 4)

    result = sample_tea_float64(
        UInt32.full(1, count),
        UInt32.arange(count), 4)
    for i in range(count):
        assert result[i] == sample_tea_float64(1, i, 4)


def test04_permute_bijection(variant_scalar_rgb):
    """ Check that the resulting permutation vectors are bijections """
    from mitsuba.core import permute

    for p in range(4):
        sample_count = 2**(p+1)
        for seed in range(500):
            perm = [permute(i, sample_count, seed) for i in range(sample_count)]
            assert len(set(perm)) == len(perm)


def test05_permute_uniform(variant_scalar_rgb):
    """ Chi2 tests to check that the permutation vectors are uniformly distributed """
    import numpy as np
    from mitsuba.core import permute

    for p in range(4):
        sample_count = 2**(p+1)
        histogram = np.zeros((sample_count, sample_count))

        N = 1000
        for seed in range(N):
            for i in range(sample_count):
                j = permute(i, sample_count, seed)
                histogram[i, j] += 1

        histogram /= N

        mean = np.mean(histogram)
        assert ek.allclose(1.0 / sample_count, mean)


def test06_permute_kensler_bijection(variant_scalar_rgb, variant_gpu_rgb):
    """ Check that the resulting permutation vectors are bijections """
    from mitsuba.core import permute_kensler

    for p in range(4):
        sample_count = 2**(p+1) + p
        for seed in range(500):
            perm = [permute_kensler(i, sample_count, seed) for i in range(sample_count)]
            assert len(set(perm)) == len(perm)


def test07_permute_kensler_uniform(variant_scalar_rgb):
    """ Chi2 tests to check that the permutation vectors are uniformly distributed """
    import numpy as np
    from mitsuba.core import permute_kensler

    for p in range(4):
        sample_count = 2**(p+1) + p
        histogram = np.zeros((sample_count, sample_count))

        N = 1000
        for seed in range(N):
            for i in range(sample_count):
                j = permute_kensler(i, sample_count, seed)
                histogram[i, j] += 1

        histogram /= N

        mean = np.mean(histogram)
        assert ek.allclose(1.0 / sample_count, mean)