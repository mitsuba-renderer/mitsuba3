import os
import struct

import numpy as np
import pytest
import mitsuba as mi


def write_container(path, entries):
    """Write ``entries`` (name -> (raw bytes, array bytes)) to ``path``"""
    pf = mi.PackedFile(path)
    assert pf.can_write()
    for name, (raw, array) in entries.items():
        pf.begin(name)
        pf.stream().write(raw)
        pf.write_array(array)
    pf.close()
    assert not pf.can_write()


def test01_roundtrip(variant_scalar_rgb, tmp_path):
    """Entries come back with their names, raw bytes and decompressed arrays"""
    rng = np.random.default_rng(0)
    entries = {
        'first': (b'HEAD', bytes(rng.integers(0, 4, 100000, dtype=np.uint8))),
        '': (b'', b''),
        'third': (b'xyz', bytes(range(256)) * 10),
    }
    fname = str(tmp_path / 'test.packed')
    write_container(fname, entries)

    pf = mi.PackedFile.open(fname)
    assert not pf.can_write()
    assert pf.entry_count() == 3
    assert [pf.entry_name(i) for i in range(3)] == list(entries.keys())
    assert pf.find('third') == 2 and pf.find('first') == 0
    assert pf.find('missing') == 3

    for i, (name, (raw, array)) in enumerate(entries.items()):
        e = pf.entry(i)
        assert e.name() == name and e.index() == i
        assert e.read(len(raw)) == raw
        assert e.read_array(len(array)) == array
        assert e.remaining() == 0

    # Reading past the end or with a wrong array size is an error
    e = pf.entry(0)
    with pytest.raises(RuntimeError, match='size of a compressed array'):
        e.skip(4)
        e.read_array(10)
    e = pf.entry(2)
    e.skip(e.remaining() - 1)
    with pytest.raises(RuntimeError, match='unexpected end'):
        e.read(4)
    with pytest.raises(RuntimeError, match='out of range'):
        pf.entry(3)


def test02_layout(variant_scalar_rgb, tmp_path):
    """The on-disk layout matches the documented format"""
    fname = str(tmp_path / 'layout.packed')
    write_container(fname, {'ab': (b'12', b'')})
    data = open(fname, 'rb').read()

    magic, version, count, unused, dict_offset = struct.unpack_from('<8sHHIQ', data, 0)
    assert magic == b'MIPACKED' and version == 1 and count == 1 and unused == 0
    offset, size, name_offset, name_length = struct.unpack_from('<QQII', data, dict_offset)
    assert offset == 24 and name_offset == 0 and name_length == 2
    assert data[dict_offset + 24:] == b'ab'
    entry = data[offset:offset + size]
    # Raw bytes, then an empty compressed array: size, block size, no blocks
    assert entry == b'12' + struct.pack('<QI', 0, 16 * 1024 * 1024)


def test03_large_array(variant_scalar_rgb, tmp_path):
    """Arrays larger than one block split into several blocks"""
    rng = np.random.default_rng(1)
    array = bytes(rng.integers(0, 2, 20 * 1024 * 1024, dtype=np.uint8))
    fname = str(tmp_path / 'large.packed')
    write_container(fname, {'big': (b'', array)})

    pf = mi.PackedFile.open(fname)
    e = pf.entry(0)
    assert e.read_array(len(array)) == array

    # The same encoding can be produced ahead of time and written verbatim
    packed = mi.PackedFile.compress_array(array)
    fname2 = str(tmp_path / 'large2.packed')
    pf2 = mi.PackedFile(fname2)
    pf2.begin('big')
    pf2.stream().write(packed)
    pf2.close()
    assert open(fname, 'rb').read() == open(fname2, 'rb').read()


def test04_cache(variant_scalar_rgb, tmp_path):
    """Opening a file twice shares the mapping until the file changes"""
    fname = str(tmp_path / 'cache.packed')
    write_container(fname, {'a': (b'1', b'')})
    pf1 = mi.PackedFile.open(fname)
    pf2 = mi.PackedFile.open(fname)
    assert pf1 is pf2

    write_container(fname, {'a': (b'1', b''), 'b': (b'2', b'')})
    pf3 = mi.PackedFile.open(fname)
    assert pf3 is not pf1
    assert pf3.entry_count() == 2 and pf1.entry_count() == 1

    mi.PackedFile.clear_cache()
    assert mi.PackedFile.open(fname) is not pf3


def test05_invalid(variant_scalar_rgb, tmp_path):
    fname = str(tmp_path / 'bad.packed')
    with open(fname, 'wb') as f:
        f.write(b'MIPACKED' + struct.pack('<HHIQ', 1, 0, 0, 0))
    with pytest.raises(RuntimeError, match='invalid dictionary offset'):
        mi.PackedFile.open(fname)
    with open(fname, 'wb') as f:
        f.write(b'not a packed file at all')
    with pytest.raises(RuntimeError, match='not a packed file'):
        mi.PackedFile.open(fname)
    with pytest.raises(RuntimeError, match='does not exist'):
        mi.PackedFile.open(str(tmp_path / 'missing.packed'))
