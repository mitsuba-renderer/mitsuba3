import enoki as ek
import pytest
import mitsuba


def r_inv(divisor, index):
    factor = 1
    value = 0
    recip = 1.0 / divisor

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


def test01_radical_inverse(variant_scalar_rgb):
    from mitsuba.core import RadicalInverse

    v = RadicalInverse()
    assert(v.eval(0, 0) == 0)
    assert(v.eval(0, 1) == 0.5)
    assert(v.eval(0, 2) == 0.25)
    assert(v.eval(0, 3) == 0.75)

    for index, prime in enumerate(gen_primes()):
        if index >= 1024:
            break
        for i in range(10):
            assert ek.abs(r_inv(prime, i) - v.eval(index, i)) < 1e-7

@pytest.mark.skip(reason="RadicalInverse has no vectorized bindings")
def test02_radical_inverse_vectorized(variant_scalar_rgb):
    from mitsuba.core import RadicalInverse

    v = RadicalInverse()
    for index, prime in enumerate(gen_primes()):
        if index >= 1024:
            break
        result = v.eval(index, ek.arange(10, dtype=ek.uint64))
        for i in range(len(result)):
            assert ek.abs(r_inv(prime, i) - result[i]) < 1e-7


def test03_faure_permutations(variant_scalar_rgb):
    from mitsuba.core import RadicalInverse

    p = RadicalInverse()
    assert (p.permutation(0) == [0, 1]).all()
    assert (p.permutation(1) == [0, 1, 2]).all()
    assert (p.permutation(2) == [0, 3, 2, 1, 4]).all()
    assert (p.permutation(3) == [0, 2, 5, 3, 1, 4, 6]).all()


def test04_scrambled_radical_inverse(variant_scalar_rgb):
    from mitsuba.core import RadicalInverse
    from mitsuba.core import math

    p = RadicalInverse(10, -1)
    assert (p.permutation(0) == [0, 1]).all()

    values = [
        0.0, 0.5, 0.25, 0.75, 0.125, 0.625, 0.375, 0.875, 0.0625, 0.5625,
        0.3125, 0.8125, 0.1875, 0.6875, 0.4375
    ]

    for i in range(len(values)):
        assert(p.eval_scrambled(0, i) == values[i])

    p = RadicalInverse(10, 3)
    assert (p.permutation(0) == [1, 0]).all()

    values_scrambled = [
        math.OneMinusEpsilon,
        0.5, 0.75, 0.25, 0.875, 0.375, 0.625, 0.125, 0.9375, 0.4375,
        0.6875, 0.1875, 0.8125, 0.3125, 0.5625
    ]

    for i in range(len(values_scrambled)):
        assert(p.eval_scrambled(0, i) == values_scrambled[i])

@pytest.mark.skip(reason="RadicalInverse has no vectorized bindings")
def test02_radical_inverse_vectorized(variant_scalar_rgb):
    from mitsuba.core import RadicalInverse

    try:
        from mitsuba.packet_rgb.core.qmc import RadicalInverseP
    except ImportError:
        pytest.skip("packet_rgb mode not enabled")

    v   = RadicalInverse()
    v_p = RadicalInverseP()
    for index in range(1024):
        result = v_p.eval_scrambled(index, ek.arange(10, dtype=ek.uint64))
        for i in range(len(result)):
            assert ek.abs(v.eval_scrambled(index, i) - result[i]) < 1e-7
