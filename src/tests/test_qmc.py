from mitsuba.core.qmc import radicalInverse
from mitsuba.core import PacketSize
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
