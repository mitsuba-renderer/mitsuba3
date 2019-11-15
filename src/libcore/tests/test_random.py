import numpy as np


def test_pcg32():
    from mitsuba.scalar_rgb.core import PCG32

    values_32bit = [0xa15c02b7, 0x7b47f409, 0xba1d3330, 0x83d2f293,
                    0xbfa4784b, 0xcbed606e]
    values_coins = 'HHTTTHTHHHTHTTTHHHHHTTTHHHTHTHTHTTHTTTHHHHHHTT' \
                   'TTHHTTTTTHTTTTTTTHT'
    values_rolls = [3, 4, 1, 1, 2, 2, 3, 2, 4, 3, 2, 4, 3, 3, 5, 2, 3, 1, 3,
                    1, 5, 1, 4, 1, 5, 6, 4, 6, 6, 2, 6, 3, 3]
    values_cards = 'Qd Ks 6d 3s 3d 4c 3h Td Kc 5c Jh Kd Jd As 4s 4h Ad Th ' \
                   'Ac Jc 7s Qs 2s 7h Kh 2d 6c Ah 4d Qh 9h 6s 5s 2c 9c Ts ' \
                   '8d 9s 3c 8c Js 5d 2h 6h 7d 8s 9d 5h 8h Qc 7c Tc'

    p = PCG32()
    p.seed(42, 54)
    base = PCG32(p)

    for v in values_32bit:
        assert p.next_uint32() == v

    p.advance(-4)
    assert p - base == 2

    for v in values_32bit[2:]:
        assert p.next_uint32() == v

    for v in values_coins:
        assert 'H' if p.next_uint32_bounded(2) == 1 else 'T' == v

    for v in values_rolls:
        assert p.next_uint32_bounded(6) + 1 == v

    cards = list(range(52))
    p.shuffle(cards)

    number = ['A', '2', '3', '4', '5', '6', '7', '8', '9', 'T', 'J',
              'Q', 'K']
    suit = ['h', 'c', 'd', 's']
    ctr = 0

    for v in values_cards.split():
        assert v == number[cards[ctr] // len(suit)] + \
            suit[cards[ctr] % len(suit)]
        ctr += 1


def test_tea_float32():
    from mitsuba.scalar_rgb.core import sample_tea_float32

    assert sample_tea_float32(1, 1, 4) == 0.5424730777740479
    assert sample_tea_float32(1, 2, 4) == 0.5079904794692993
    assert sample_tea_float32(1, 3, 4) == 0.4171961545944214
    assert sample_tea_float32(1, 4, 4) == 0.008385419845581055
    assert sample_tea_float32(1, 5, 4) == 0.8085528612136841
    assert sample_tea_float32(2, 1, 4) == 0.6939879655838013
    assert sample_tea_float32(3, 1, 4) == 0.6978365182876587
    assert sample_tea_float32(4, 1, 4) == 0.4897364377975464


def test_tea_float64():
    from mitsuba.scalar_rgb.core import sample_tea_float64

    assert sample_tea_float64(1, 1, 4) == 0.5424730799533735
    assert sample_tea_float64(1, 2, 4) == 0.5079905082233922
    assert sample_tea_float64(1, 3, 4) == 0.4171962610608142
    assert sample_tea_float64(1, 4, 4) == 0.008385529523330604
    assert sample_tea_float64(1, 5, 4) == 0.80855288317879
    assert sample_tea_float64(2, 1, 4) == 0.6939880404156831
    assert sample_tea_float64(3, 1, 4) == 0.6978365636630994
    assert sample_tea_float64(4, 1, 4) == 0.48973647949223253


def test_tea_vectorized():
    try:
        from mitsuba.packet_rgb.core import sample_tea_float32 as sample_tea_float32_p
        from mitsuba.packet_rgb.core import sample_tea_float64 as sample_tea_float64_p
    except ImportError:
        pytest.mark.skip("packet_rgb mode not enabled")

    count = 100

    result = sample_tea_float32_p(
        np.full(count, 1, dtype=np.uint32),
        np.arange(count, dtype=np.uint32), 4)
    for i in range(count):
        assert result[i] == sample_tea_float32_p(1, i, 4)

    result = sample_tea_float64_p(
        np.full(count, 1, dtype=np.uint32),
        np.arange(count, dtype=np.uint32), 4)
    for i in range(count):
        assert result[i] == sample_tea_float64_p(1, i, 4)
