import pytest
import mitsuba


def test01_short_args(variant_scalar_rgb):
    from mitsuba.core import ArgParser
    ap = ArgParser()
    a = ap.add("-a")
    b = ap.add("-b")
    c = ap.add("-c")
    d = ap.add("-d")
    ap.parse(['mitsuba', '-aba', '-d'])

    assert bool(a)
    assert a.count() == 2
    assert bool(b)
    assert b.count() == 1
    assert not bool(c)
    assert c.count() == 0
    assert bool(d)
    assert d.count() == 1
    assert ap.executable_name() == "mitsuba"


def test02_parameter_value(variant_scalar_rgb):
    from mitsuba.core import ArgParser
    ap = ArgParser()
    a = ap.add("-a", True)
    ap.parse(['mitsuba', '-a', 'abc', '-axyz'])
    assert bool(a)
    assert a.count() == 2
    assert a.as_string() == 'abc'
    with pytest.raises(RuntimeError):
        a.as_int()
    assert a.next().as_string() == 'xyz'


def test03_parameter_missing(variant_scalar_rgb):
    from mitsuba.core import ArgParser
    ap = ArgParser()
    ap.add("-a", True)
    with pytest.raises(RuntimeError):
        ap.parse(['mitsuba', '-a'])


def test04_parameter_float_and_extra_args(variant_scalar_rgb):
    from mitsuba.core import ArgParser
    ap = ArgParser()
    f = ap.add("-f", True)
    other = ap.add("", True)
    ap.parse(['mitsuba', 'other', '-f0.25', 'arguments'])
    assert bool(f)
    assert f.as_float() == 0.25
    assert bool(other)
    assert other.count() == 2


def test05_long_parameters_failure(variant_scalar_rgb):
    from mitsuba.core import ArgParser
    ap = ArgParser()
    i = ap.add("--int", True)
    with pytest.raises(RuntimeError):
        ap.parse(['mitsuba', '--int1'])
    with pytest.raises(RuntimeError):
        ap.parse(['mitsuba', '--int'])
    ap.parse(['mitsuba', '--int', '34'])
    assert i.as_int() == 34
