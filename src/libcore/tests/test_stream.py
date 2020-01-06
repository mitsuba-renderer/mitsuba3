import os
import enoki as ek
import pytest
import mitsuba

mitsuba.set_variant('scalar_rgb')

from mitsuba.core import Stream, DummyStream, FileStream, MemoryStream, ZStream
from mitsuba.python.test.util import tmpfile, make_tmpfile

parameters = [
    'class_,args',
    [
        (DummyStream, ()),
        (MemoryStream, (64,)),
        (FileStream, (make_tmpfile, FileStream.ERead)),
        (FileStream, (make_tmpfile, FileStream.ETruncReadWrite))
    ]
]

# TODO: more contents, exercise lots of types
contents = [82.548, 999, 'some sentence', 424,
            'hi', 13.3701, True, 'hey',
            42, 'c', False, '', 99.998]


def write_contents(stream):
    for v in contents:
        if type(v) is str:
            stream.write_string(v)
        elif type(v) is int:
            stream.write_int64(v)
        elif type(v) is float:
            stream.write_single(v)
        elif type(v) is bool:
            stream.write_bool(v)
    stream.flush()


def check_contents(stream):
    if type(stream) is not ZStream:
        stream.seek(0)
    for v in contents:
        if type(v) is str:
            assert v == stream.read_string()
        elif type(v) is int:
            assert v == stream.read_int64()
        elif type(v) is float:
            assert ek.abs(stream.read_single() - v) / v < 1e-5
        elif type(v) is bool:
            assert v == stream.read_bool()


@pytest.mark.parametrize(*parameters)
def test01_size_and_pos(class_, args, request, tmpdir_factory):
    stream = class_(*[(arg if arg is not make_tmpfile
                      else arg(request, tmpdir_factory))
                      for arg in args])

    assert stream.size() == 0
    assert stream.tell() == 0

    if not stream.can_write():
        stream.close()
        return

    stream.write(u'hello'.encode('latin1'))
    stream.flush()
    assert stream.size() == 5
    assert stream.tell() == 5
    stream.write_int64(42)  # Long (8)
    stream.flush()
    assert stream.size() == 5 + 8
    assert stream.tell() == 5 + 8
    stream.close()


@pytest.mark.parametrize(*parameters)
def test02_truncate(class_, args, request, tmpdir_factory):
    stream = class_(*[(arg if arg is not make_tmpfile
                      else arg(request, tmpdir_factory))
                      for arg in args])

    if not stream.can_write():
        return
    assert stream.size() == 0
    assert stream.tell() == 0
    stream.truncate(100)
    assert stream.size() == 100
    assert stream.tell() == 0
    stream.seek(99)
    assert stream.tell() == 99
    stream.truncate(50)
    assert stream.tell() == 50
    assert stream.size() == 50
    stream.write_string('hello')
    stream.flush()
    assert stream.tell() == 50 + 9
    assert stream.size() == 50 + 9


@pytest.mark.parametrize(*parameters)
def test03_seek(class_, args, request, tmpdir_factory):
    stream = class_(*[(arg if arg is not make_tmpfile
                      else arg(request, tmpdir_factory))
                      for arg in args])

    size = 0

    assert stream.size() == size
    assert stream.tell() == 0

    if stream.can_write():
        size = 5
        stream.truncate(size)
        assert stream.size() == size
        assert stream.tell() == 0

    stream.seek(5)
    assert stream.size() == size
    assert stream.tell() == 5

    # Seeking beyond the end of the file is okay
    # but won't make it larger
    stream.seek(20)
    assert stream.size() == size
    assert stream.tell() == 20

    if stream.can_write():
        # A subsequent write should start at the correct position
        # and update the size.
        stream.write_single(13.37)
        stream.flush()
        assert stream.size() == 20 + 4
        assert stream.tell() == 20 + 4


@pytest.mark.parametrize(*parameters)
def test03_read_back(class_, args, request, tmpdir_factory):
    stream = class_(*[(arg if arg is not make_tmpfile
                      else arg(request, tmpdir_factory))
                      for arg in args])

    if stream.can_write():
        write_contents(stream)
        if stream.can_read():
            check_contents(stream)


@pytest.mark.parametrize(*parameters)
def test04_read_back(class_, args, request, tmpdir_factory):
    stream = class_(*[(arg if arg is not make_tmpfile
                      else arg(request, tmpdir_factory))
                      for arg in args])

    otherEndianness = Stream.EBigEndian
    if Stream.host_byte_order() == otherEndianness:
        otherEndianness = Stream.ELittleEndian

    if stream.can_write():
        write_contents(stream)
        if stream.can_read():
            check_contents(stream)

    stream.close()

    assert not stream.can_read()
    assert not stream.can_write()


@pytest.mark.parametrize(*parameters)
def test05_read_back_through_zstream(class_, args, request, tmpdir_factory):
    stream = class_(*[(arg if arg is not make_tmpfile
                      else arg(request, tmpdir_factory))
                      for arg in args])

    zstream = ZStream(stream)

    assert stream.can_read() == zstream.child_stream().can_read()
    assert stream.can_write() == zstream.child_stream().can_write()

    if stream.can_write():
        write_contents(zstream)
        del zstream
        stream.seek(0)

        if stream.can_read():
            zstream = ZStream(stream)
            check_contents(zstream)
            del zstream


def test06_dummy_stream():
    s = DummyStream()
    assert s.can_write()
    assert not s.can_read()
    # string length as a uint32_t (4) + string (11)
    s.write_string('hello world')
    s.seek(0)
    with pytest.raises(RuntimeError):
        s.read_int64()
    s.set_byte_order(Stream.EBigEndian)
    assert str(s) == """DummyStream[
  host_byte_order = little-endian,
  byte_order = big-endian,
  can_read = 0,
  can_write = 1,
  pos = 0,
  size = 15
]"""


def test07_memory_stream():
    s = MemoryStream(64)

    assert s.can_write()
    assert s.can_read()

    s.write_string('hello world')
    s.set_byte_order(Stream.EBigEndian)

    assert str(s) == """MemoryStream[
  host_byte_order = little-endian,
  byte_order = big-endian,
  can_read = 1,
  can_write = 1,
  owns_buffer = 1,
  capacity = 64,
  pos = 15,
  size = 15
]"""


@pytest.mark.parametrize('rw', [True, False])
def test08_fstream(rw, tmpfile):
    s = FileStream(tmpfile, FileStream.ETruncReadWrite
                   if rw else FileStream.ERead)

    assert s.can_read()
    assert s.can_write() == rw

    if s.can_write():
        s.write_int64(42)
        s.flush()
        s.seek(0)
        assert s.read_int64() == 42
        s.seek(0)

        # Truncating shouldn't change the position if not necessary
        assert s.tell() == 0
        s.truncate(5)
        assert s.tell() == 0
        s.write_string('hello')
        s.flush()
        assert s.tell() == 0 + 9
        s.truncate(5)
        assert s.tell() == 5

    assert str(s) == """FileStream[
  path = "%s",
  host_byte_order = little-endian,
  byte_order = little-endian,
  can_read = 1,
  can_write = %i,
  pos = %i,
  size = %i
]""" % (s.path(), 1 if rw else 0, 5 if rw else 0, 5 if rw else 0)

    if not s.can_write():
        with pytest.raises(RuntimeError):
            s.write_string('hello')
        with pytest.raises(RuntimeError):
            s.truncate(5)

    s.close()

    # File should only be created when opening in write mode
    new_name = tmpfile + "_2"
    if rw:
        s = FileStream(new_name, FileStream.ETruncReadWrite)
        s.close()
        assert os.path.exists(new_name)
    else:
        with pytest.raises(RuntimeError):
            FileStream(new_name)
