import sys
import pytest


def test01_time_string(variant_scalar_rgb):
    from mitsuba.misc import time_string

    assert time_string(1, precise=True) == '1ms'
    assert time_string(2010, precise=True) == '2.01s'
    assert time_string(2 * 1000 * 60, precise=True) == '2m'
    assert time_string(2 * 1000 * 60 * 60, precise=True) == '2h'
    assert time_string(2 * 1000 * 60 * 60 * 24, precise=True) == '2d'
    assert time_string(2 * 1000 * 60 * 60 * 24 * 7, precise=True) == '2w'
    assert time_string(2 * 1000 * 60 * 60 * 24 * 7 * 52.1429,
                      precise=True) == '2y'


def test01_mem_string(variant_scalar_rgb):
    from mitsuba.misc import mem_string

    assert mem_string(2, precise=True) == '2 B'
    assert mem_string(2 * 1024, precise=True) == '2 KiB'
    assert mem_string(2 * 1024 ** 2, precise=True) == '2 MiB'
    assert mem_string(2 * 1024 ** 3, precise=True) == '2 GiB'
    if sys.maxsize > 4*1024**3:
        assert mem_string(2 * 1024 ** 4, precise=True) == '2 TiB'
        assert mem_string(2 * 1024 ** 5, precise=True) == '2 PiB'
        assert mem_string(2 * 1024 ** 6, precise=True) == '2 EiB'
