import pytest
import os
import numpy as np
from mitsuba.core import Stream, DummyStream, FileStream, MemoryStream, ZStream
from .utils import tmpfile

parameters = [
    'class_,args',
    [
        (DummyStream, ()),
        (MemoryStream, (64,)),
        (FileStream, (tmpfile, False)),
        (FileStream, (tmpfile, True))
    ]
]

# TODO: more contents, exercise lots of types
contents = [82.548, 999, 'some sentence', 424,
            'hi', 13.3701, True, 'hey',
            42, 'c', False, '', 99.998]


def write_contents(stream):
    for v in contents:
        if type(v) is str:
            stream.writeString(v)
        elif type(v) is int:
            stream.writeLong(v)
        elif type(v) is float:
            stream.writeSingle(v)
        elif type(v) is bool:
            stream.writeBool(v)
    stream.flush()


def check_contents(stream):
    if type(stream) is not ZStream:
        stream.seek(0)
    for v in contents:
        if type(v) is str:
            assert v == stream.readString()
        elif type(v) is int:
            assert v == stream.readLong()
        elif type(v) is float:
            assert np.abs(stream.readSingle() - v) / v < 1e-5
        elif type(v) is bool:
            assert v == stream.readBool()


@pytest.mark.parametrize(*parameters)
def test01_size_and_pos(class_, args, request, tmpdir_factory):
    stream = class_(*[(arg if arg is not tmpfile
                      else arg(request, tmpdir_factory))
                      for arg in args])

    assert stream.size() == 0
    assert stream.tell() == 0

    if not stream.canWrite():
        return

    stream.write(u'hello'.encode('latin1'))
    stream.flush()
    assert stream.size() == 5
    assert stream.tell() == 5
    stream.writeLong(42)  # Long (8)
    stream.flush()
    assert stream.size() == 5 + 8
    assert stream.tell() == 5 + 8


@pytest.mark.parametrize(*parameters)
def test02_truncate(class_, args, request, tmpdir_factory):
    stream = class_(*[(arg if arg is not tmpfile
                      else arg(request, tmpdir_factory))
                      for arg in args])

    if not stream.canWrite():
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
    stream.writeString('hello')
    stream.flush()
    assert stream.tell() == 50 + 9
    assert stream.size() == 50 + 9


@pytest.mark.parametrize(*parameters)
def test03_seek(class_, args, request, tmpdir_factory):
    stream = class_(*[(arg if arg is not tmpfile
                      else arg(request, tmpdir_factory))
                      for arg in args])

    size = 0

    assert stream.size() == size
    assert stream.tell() == 0

    if stream.canWrite():
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

    if stream.canWrite():
        # A subsequent write should start at the correct position
        # and update the size.
        stream.writeSingle(13.37)
        stream.flush()
        assert stream.size() == 20 + 4
        assert stream.tell() == 20 + 4


@pytest.mark.parametrize(*parameters)
def test03_read_back(class_, args, request, tmpdir_factory):
    stream = class_(*[(arg if arg is not tmpfile
                      else arg(request, tmpdir_factory))
                      for arg in args])

    if stream.canWrite():
        write_contents(stream)
        if stream.canRead():
            check_contents(stream)


@pytest.mark.parametrize(*parameters)
def test04_read_back(class_, args, request, tmpdir_factory):
    stream = class_(*[(arg if arg is not tmpfile
                      else arg(request, tmpdir_factory))
                      for arg in args])

    otherEndianness = Stream.EBigEndian
    if Stream.hostByteOrder() == otherEndianness:
        otherEndianness = Stream.ELittleEndian

    if stream.canWrite():
        write_contents(stream)
        if stream.canRead():
            check_contents(stream)

    stream.close()

    assert not stream.canRead()
    assert not stream.canWrite()


@pytest.mark.parametrize(*parameters)
def test05_read_back_through_zstream(class_, args, request, tmpdir_factory):
    stream = class_(*[(arg if arg is not tmpfile
                      else arg(request, tmpdir_factory))
                      for arg in args])

    zstream = ZStream(stream)

    assert stream.canRead() == zstream.childStream().canRead()
    assert stream.canWrite() == zstream.childStream().canWrite()

    if stream.canWrite():
        write_contents(zstream)
        del zstream
        stream.seek(0)

        if stream.canRead():
            zstream = ZStream(stream)
            check_contents(zstream)
            del zstream


def test06_dummy_stream():
    s = DummyStream()
    assert s.canWrite()
    assert not s.canRead()
    # string length as a uint32_t (4) + string (11)
    s.writeString('hello world')
    s.seek(0)
    with pytest.raises(RuntimeError):
        s.readLong()
    s.setByteOrder(Stream.EBigEndian)
    assert str(s) == """DummyStream[
  hostByteOrder = little-endian,
  byteOrder = big-endian,
  canRead = 0,
  canWrite = 0,
  pos = 0,
  size = 15
]"""


def test07_memory_stream():
    s = MemoryStream(64)

    assert s.canWrite()
    assert s.canRead()

    s.writeString('hello world')
    s.setByteOrder(Stream.EBigEndian)

    assert str(s) == """MemoryStream[
  hostByteOrder = little-endian,
  byteOrder = big-endian,
  canRead = 1,
  canWrite = 1,
  ownsBuffer = 1,
  capacity = 64,
  pos = 15,
  size = 15
]"""


@pytest.mark.parametrize('rw', [True, False])
def test08_fstream(rw, tmpfile):
        s = FileStream(tmpfile, rw)

        assert s.canRead()
        assert s.canWrite() == rw

        if s.canWrite():
            s.writeLong(42)
            s.flush()
            s.seek(0)
            assert s.readLong() == 42
            s.seek(0)

            # Truncating shouldn't change the position if not necessary
            assert s.tell() == 0
            s.truncate(5)
            assert s.tell() == 0
            s.writeString('hello')
            s.flush()
            assert s.tell() == 0 + 9
            s.truncate(5)
            assert s.tell() == 5
        else:
            with pytest.raises(RuntimeError):
                s.writeString('hello')
            with pytest.raises(RuntimeError):
                s.truncate(5)

        assert str(s) == """FileStream[
  path = "%s",
  hostByteOrder = little-endian,
  byteOrder = little-endian,
  canRead = 1,
  canWrite = 1,
  pos = %i,
  size = %i
]""" % (s.path(), 5 if rw else 0, 5 if rw else 0)

        s.close()

        # File should only be created when opening in write mode
        new_name = tmpfile + "_2"
        if rw:
            s = FileStream(new_name, rw)
            s.close()
            assert os.path.exists(new_name)
        else:
            with pytest.raises(RuntimeError):
                FileStream(new_name, rw)
