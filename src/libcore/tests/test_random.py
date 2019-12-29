import enoki as ek
from enoki.dynamic import UInt32
import pytest
import mitsuba

@pytest.fixture
def variant():
    mitsuba.set_variant('scalar_rgb')

def test_tea_float32(variant):
    from mitsuba.core import sample_tea_float32

    assert sample_tea_float32(1, 1, 4) == 0.5424730777740479
    assert sample_tea_float32(1, 2, 4) == 0.5079904794692993
    assert sample_tea_float32(1, 3, 4) == 0.4171961545944214
    assert sample_tea_float32(1, 4, 4) == 0.008385419845581055
    assert sample_tea_float32(1, 5, 4) == 0.8085528612136841
    assert sample_tea_float32(2, 1, 4) == 0.6939879655838013
    assert sample_tea_float32(3, 1, 4) == 0.6978365182876587
    assert sample_tea_float32(4, 1, 4) == 0.4897364377975464


def test_tea_float64(variant):
    from mitsuba.core import sample_tea_float64

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
        mitsuba.set_variant("packet_rgb")
        from mitsuba.core import sample_tea_float32, sample_tea_float64
    except ImportError:
        pytest.skip("packet_rgb mode not enabled")

    count = 100

    result = sample_tea_float32(
        UInt32.full(count, 1),
        UInt32.arange(count), 4)
    for i in range(count):
        assert result[i] == sample_tea_float32(1, i, 4)

    result = sample_tea_float64(
        UInt32.full(count, 1),
        UInt32.arange(count), 4)
    for i in range(count):
        assert result[i] == sample_tea_float64(1, i, 4)
