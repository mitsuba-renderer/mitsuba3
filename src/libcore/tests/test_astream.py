import numpy as np
import pytest
import sys

from mitsuba.core import AnnotatedStream, FileStream, MemoryStream, Thread
from mitsuba.test.util import tmpfile, fresolver_append_path


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

@fresolver_append_path
def test04_reference_file():
    """
    Read back a specific serialized file written from C++. Checks the prefix
    hierarchy and the entries' types (closest match to Python types).
    This should help check that we maintain compatibility.

    See `serialize_ref.cpp` for the C++ code generating the serialized file.
    """
    # TODO: also do read/write with endianness swap enabled
    import numpy as np

    fr = Thread.thread().file_resolver()
    fstream = FileStream(fr.resolve("resources/data/tests/reference.serialized"))
    s = AnnotatedStream(fstream, write_mode=False)

    assert set(s.keys()) == {
        'top_char', 'top_float_nan', 'top_double_nan',
        'prefix1.prefix1_bool', 'prefix1.prefix1_double', 'prefix1.prefix1_float',
        'prefix1.prefix1_int16', 'prefix1.prefix2.prefix2_bool',
        'prefix3.prefix3_int16', 'prefix3.prefix3_int32', 'prefix3.prefix3_int64',
        'prefix3.prefix3_int8', 'prefix3.prefix3_uint16', 'prefix3.prefix3_uint32',
        'prefix3.prefix3_uint64', 'prefix3.prefix3_uint8',
    }

    v = s.get("top_char")
    assert isinstance(v, str if sys.version_info >= (3,0) else unicode) and v == 'a'
    v = s.get("top_float_nan")
    assert isinstance(v, float) and np.isnan(v)
    v = s.get("top_double_nan")
    assert isinstance(v, float) and np.isnan(v)

    # Accessing the prefixes in a different order than they were written.
    s.push("prefix3")
    v = s.get("prefix3_int8")
    assert isinstance(v, int) and v == 1
    v = s.get("prefix3_uint8")
    assert isinstance(v, int) and v == 1
    v = s.get("prefix3_int16")
    assert isinstance(v, int) and v == 1
    v = s.get("prefix3_uint16")
    assert isinstance(v, int) and v == 1
    v = s.get("prefix3_int32")
    assert isinstance(v, int) and v == 1
    v = s.get("prefix3_uint32")
    assert isinstance(v, int) and v == 1
    v = s.get("prefix3_int64")
    assert isinstance(v, int) and v == 1
    v = s.get("prefix3_uint64")
    assert isinstance(v, int) and v == 1
    s.pop()

    s.push("prefix1")
    s.push("prefix2")
    v = s.get("prefix2_bool")
    assert isinstance(v, bool) and v == True
    s.pop()
    s.pop()

    s.push("prefix1")
    v = s.get("prefix1_float")
    assert isinstance(v, float) and np.allclose(v, 1.0)
    v = s.get("prefix1_double")
    assert isinstance(v, float) and np.allclose(v, 1.0)
    v = s.get("prefix1_bool")
    assert isinstance(v, bool) and v == False
    v = s.get("prefix1_int16")
    assert isinstance(v, int) and v == 1
    s.pop()
