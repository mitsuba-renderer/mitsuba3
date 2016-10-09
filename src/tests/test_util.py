from mitsuba.core.util import timeString, memString


def test01_timeString():
    assert timeString(1, precise=True) == '1ms'
    assert timeString(2010, precise=True) == '2.01s'
    assert timeString(2 * 1000 * 60, precise=True) == '2m'
    assert timeString(2 * 1000 * 60 * 60, precise=True) == '2h'
    assert timeString(2 * 1000 * 60 * 60 * 24, precise=True) == '2d'
    assert timeString(2 * 1000 * 60 * 60 * 24 * 7, precise=True) == '2w'
    assert timeString(2 * 1000 * 60 * 60 * 24 * 7 * 52.1429,
                      precise=True) == '2y'


def test01_memString():
    assert memString(2, precise=True) == '2 B'
    assert memString(2 * 1024, precise=True) == '2 KiB'
    assert memString(2 * 1024 ** 2, precise=True) == '2 MiB'
    assert memString(2 * 1024 ** 3, precise=True) == '2 GiB'
    assert memString(2 * 1024 ** 4, precise=True) == '2 TiB'
    assert memString(2 * 1024 ** 5, precise=True) == '2 PiB'
    assert memString(2 * 1024 ** 6, precise=True) == '2 EiB'
