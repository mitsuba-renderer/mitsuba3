from mitsuba.core import ArgParser
import pytest


def test01_short_args():
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
    assert ap.executableName() == "mitsuba"


def test02_parameterValue():
    ap = ArgParser()
    a = ap.add("-a", True)
    ap.parse(['mitsuba', '-a', 'abc', '-axyz'])
    assert bool(a)
    assert a.count() == 2
    assert a.asString() == 'abc'
    with pytest.raises(RuntimeError):
        a.asInt()
    assert a.next().asString() == 'xyz'


def test03_parameterMissing():
    ap = ArgParser()
    ap.add("-a", True)
    with pytest.raises(RuntimeError):
        ap.parse(['mitsuba', '-a'])


def test04_parameterFloatAndExtraArgs():
    ap = ArgParser()
    f = ap.add("-f", True)
    other = ap.add("", True)
    ap.parse(['mitsuba', 'other', '-f0.25', 'arguments'])
    assert bool(f)
    assert f.asFloat() == 0.25
    assert bool(other)
    assert other.count() == 2


def test05_longParametersFailure():
    ap = ArgParser()
    i = ap.add("--int", True)
    with pytest.raises(RuntimeError):
        ap.parse(['mitsuba', '--int1'])
    with pytest.raises(RuntimeError):
        ap.parse(['mitsuba', '--int'])
    ap.parse(['mitsuba', '--int', '34'])
    assert i.asInt() == 34
