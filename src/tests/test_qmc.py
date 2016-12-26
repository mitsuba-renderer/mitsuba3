from mitsuba.core.qmc import (radicalInverse, scrambledRadicalInverse,
                              PermutationStorage)
from mitsuba.core import PacketSize
from mitsuba.core.math import OneMinusEpsilon
import numpy as np


def r_inv(divisor, index):
    factor = 1
    value = 0
    recip = 1 / divisor

    while index != 0:
        next_val = index // divisor
        factor *= recip
        value = value * divisor + index - next_val * divisor
        index = next_val

    return value * factor


def gen_primes():
    # http://code.activestate.com/recipes/117119/
    D = {}
    q = 2
    while True:
        if q not in D:
            yield q
            D[q * q] = [q]
        else:
            for p in D[q]:
                D.setdefault(p + q, []).append(p)
            del D[q]
        q += 1


def test01_radicalInverse():
    assert(radicalInverse(0, 0) == 0)
    assert(radicalInverse(0, 1) == 0.5)
    assert(radicalInverse(0, 2) == 0.25)
    assert(radicalInverse(0, 3) == 0.75)

    for index, prime in enumerate(gen_primes()):
        if index >= 1024:
            break
        for i in range(10):
            assert np.abs(r_inv(prime, i) - radicalInverse(index, i)) < 1e-7


def test02_radicalInverseVectorized():
    for index, prime in enumerate(gen_primes()):
        if index >= 1024:
            break
        result = radicalInverse(index, np.arange(PacketSize, dtype=np.uint64))
        for i in range(PacketSize):
            assert np.abs(r_inv(prime, i) - result[i]) < 1e-7


def test03_faurePermutations():
    ps = PermutationStorage()
    assert np.all(ps.permutation(0) == [0, 1])
    assert np.all(ps.permutation(1) == [0, 1, 2])
    assert np.all(ps.permutation(2) == [0, 3, 2, 1, 4])
    assert np.all(ps.permutation(3) == [0, 2, 5, 3, 1, 4, 6])


def test04_scrambledRadicalInverse():
    p = PermutationStorage().permutation(0)

    values = [
        0.0, 0.5, 0.25, 0.75, 0.125, 0.625, 0.375, 0.875, 0.0625, 0.5625,
        0.3125, 0.8125, 0.1875, 0.6875, 0.4375
    ]

    for i in range(len(values)):
        assert(scrambledRadicalInverse(0, i, [0, 1]) == values[i])

    values_scrambled = [
        OneMinusEpsilon,
        0.5, 0.75, 0.25, 0.875, 0.375, 0.625, 0.125, 0.9375, 0.4375,
        0.6875, 0.1875, 0.8125, 0.3125, 0.5625
    ]

    for i in range(len(values_scrambled)):
        assert(scrambledRadicalInverse(0, i, [1, 0]) == values_scrambled[i])


def test03_scrambledRadicalInverseVectorized():
    padding = [0, 0, 0]

    result = scrambledRadicalInverse(0, np.arange(PacketSize, dtype=np.uint64), [0, 1] + padding)
    for i in range(PacketSize):
        assert np.abs(result[i] - scrambledRadicalInverse(0, i, [0, 1])) < 1e-7

    result = scrambledRadicalInverse(0, np.arange(PacketSize, dtype=np.uint64), [1, 0] + padding)
    for i in range(PacketSize):
        assert np.abs(result[i] - scrambledRadicalInverse(0, i, [1, 0])) < 1e-7
