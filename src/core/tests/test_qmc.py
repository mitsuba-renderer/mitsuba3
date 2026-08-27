import pytest
import drjit as dr
import mitsuba as mi


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


def test01_radical_inverse_base2(variant_scalar_rgb):
    v = mi.RadicalInverse()
    assert v.eval(0, 0) == 0
    assert v.eval(0, 1) == 0.5
    assert v.eval(0, 2) == 0.25
    assert v.eval(0, 3) == 0.75


def test02_radical_inverse(variant_scalar_rgb):
    v = mi.RadicalInverse()
    assert v.bases() == 1024

    for index, prime in enumerate(gen_primes()):
        if index >= 1024:
            break
        assert v.base(index) == prime
        for i in range(10):
            assert dr.abs(r_inv(prime, i) - v.eval(index, i)) < 1e-7


def test03_faure_permutations(variant_scalar_rgb):
    v = mi.RadicalInverse()
    assert (v.permutation(0) == [0, 1]).all()
    assert (v.permutation(1) == [0, 1, 2]).all()
    assert (v.permutation(2) == [0, 3, 2, 1, 4]).all()
    assert (v.permutation(3) == [0, 2, 5, 3, 1, 4, 6]).all()


def test04_permutation_is_bijection(variant_scalar_rgb):
    v = mi.RadicalInverse()
    for index in range(20):
        perm = v.permutation(index)
        assert sorted(perm) == list(range(v.base(index)))


def test05_scrambled_permutations(variant_scalar_rgb):
    v = mi.RadicalInverse(10, 3)
    assert v.scramble() == 3
    assert (v.permutation(0) == [1, 0]).all()
    assert (v.permutation(1) == [2, 1, 0]).all()


def test06_inverse_permutation(variant_scalar_rgb):
    v = mi.RadicalInverse(10, 3)
    for index in range(2):
        perm = list(v.permutation(index))
        assert v.inverse_permutation(index) == perm.index(0)


def test07_eval_out_of_bounds(variant_scalar_rgb):
    v = mi.RadicalInverse()
    with pytest.raises(RuntimeError):
        v.eval(v.bases(), 0)
    with pytest.raises(RuntimeError):
        v.base(v.bases())
