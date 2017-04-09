from mitsuba.core import AnnotatedStream, FileStream, \
    MemoryStream, Thread, EError
import pytest
import numpy as np
from .utils import tmpfile


# TODO: more types
test_data = {
    'a': 0,
    'b': 42,
    'sub1': {
        'a': 13.37,
        'b': 'hello',
        'c': 'world',
        'sub2': {
            'a': 'another one',
            'longer key with spaces?': True
        }
    },
    'other': {
        'a': 'potential naming conflicts'
    },
    'empty': {
        # Stays empty, shouldn't appear as a key
    }
}


# Writes example contents (with nested names, various types, etc)
def write_contents(astream, data=test_data):
    for (key, val) in sorted(data.items()):
        if type(val) is dict:
            astream.push(key)
            write_contents(astream, val)
            astream.pop()
        else:
            astream.set(key, val)


# Writes example contents (with nested names, various types, etc)
def check_contents(astream, data=test_data):
    for (key, val) in sorted(data.items()):
        if type(val) is dict:
            astream.push(key)
            check_contents(astream, val)
            astream.pop()
        else:
            if type(val) is float:
                assert np.abs(astream.get(key) - val) < 1e-6
            else:
                assert astream.get(key) == val


parameters = [
    'class_,args',
    [
        (MemoryStream, (64,)),
        (FileStream, (tmpfile, FileStream.ETruncReadWrite))
    ]
]


@pytest.mark.parametrize(*parameters)
def test01_basics(class_, args, request, tmpdir_factory):
    stream = class_(*[(arg if arg is not tmpfile
                      else arg(request, tmpdir_factory))
                      for arg in args])

    for write_mode in [True, False]:
        astream = AnnotatedStream(stream, write_mode)
        if write_mode:
            assert astream.size() == 0

        # Should have read-only and write-only modes
        assert astream.can_write() == write_mode
        assert astream.can_read() != write_mode

        # Cannot read or write to a closed astream
        astream.close()
        with pytest.raises(RuntimeError):
            astream.get('some_field')
        with pytest.raises(RuntimeError):
            astream.set('some_field', 42)

    stream.close()


def test02_toc():
    # Build a tree of prefixes
    stream = MemoryStream()
    astream = AnnotatedStream(stream, True)

    assert astream.keys() == []
    astream.set('a', 0)
    astream.set('b', 1)
    assert set(astream.keys()) == {'a', 'b'}
    astream.push('level_2')
    assert astream.keys() == []
    astream.set('a', 2)
    astream.set('b', 3)
    assert set(astream.keys()) == {'a', 'b'}
    astream.pop()
    assert set(astream.keys()) == {'a', 'b', 'level_2.a', 'level_2.b'}

    astream.push('level_2')
    assert set(astream.keys()) == {'a', 'b'}
    astream.push('level_3')
    astream.set('c', 4)
    astream.set('d', 5)
    assert set(astream.keys()) == {'c', 'd'}
    astream.pop()

    assert set(astream.keys()) == {'a', 'b', 'level_3.c', 'level_3.d'}
    astream.push('other')
    assert astream.keys() == []


@pytest.mark.parametrize(*parameters)
def test03_readback(class_, args, request, tmpdir_factory):
    stream = class_(*[(arg if arg is not tmpfile
                      else arg(request, tmpdir_factory))
                      for arg in args])

    with pytest.raises(RuntimeError) as e:
        # Need a valid header
        mstream = MemoryStream()
        mstream.write_long(123)
        AnnotatedStream(mstream, False)
    e.match("Error trying to read the table of contents")

    for write_mode in [True, False]:
        astream = AnnotatedStream(stream, write_mode)
        with pytest.raises(RuntimeError) as e:
            astream.get("test")
        e.match('Key "test" does not exist in AnnotatedStream')
        if write_mode:
            write_contents(astream)
        else:
            check_contents(astream)
        astream.close()
        del astream
    stream.close()


@pytest.mark.skip(reason="Not yet implemented")
def test05_gold_standard():
    # TODO: test read & write against reference serialized file ("gold
    #       standard") that is known to work. In this file, include:
    #       - Many types
    #       - Prefix hierarchy
    # TODO: make sure that a file written using the Python bindings can be
    #       read with C++, and the other way around.
    # TODO: also do read/write with endianness swap enabled
    pass
