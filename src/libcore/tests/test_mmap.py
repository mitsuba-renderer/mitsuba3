import numpy as np
import os

import mitsuba
mitsuba.set_variant('scalar_rgb')
from mitsuba.core import MemoryMappedFile

def test01_open_read_only(tmpdir):
    tmp_file = os.path.join(str(tmpdir), "mmap_test")
    with open(tmp_file, "w") as f:
        f.write('hello!')
    mmap = MemoryMappedFile(tmp_file)
    assert mmap.size() == 6
    assert not mmap.can_write()
    array_view = np.array(mmap, copy=False)
    assert np.all(array_view == np.array('hello!', 'c').view(np.uint8))
    del array_view
    del mmap
    os.remove(tmp_file)


def test02_open_read_write(tmpdir):
    tmp_file = os.path.join(str(tmpdir), "mmap_test")
    with open(tmp_file, "w") as f:
        f.write('hello!')
    mmap = MemoryMappedFile(tmp_file, write=True)
    assert mmap.size() == 6
    assert mmap.can_write()
    array_view = np.array(mmap, copy=False)
    array_view[1] = ord('a')
    with open(tmp_file, "r") as f:
        assert f.readline() == 'hallo!'
    del array_view
    del mmap
    os.remove(tmp_file)


def test03_create_resize(tmpdir):
    tmp_file = os.path.join(str(tmpdir), "mmap_test")
    mmap = MemoryMappedFile(tmp_file, 8192)
    assert mmap.size() == 8192
    assert mmap.can_write()
    array_view = np.array(mmap, copy=False).view(np.uint32)
    array_view[:] = np.arange(2048, dtype=np.uint32)
    del array_view
    del mmap
    mmap = MemoryMappedFile(tmp_file, write=True)
    array_view = np.array(mmap, copy=False).view(np.uint32)
    assert np.all(array_view == np.arange(2048, dtype=np.uint32))
    del array_view
    mmap.resize(4096)
    assert mmap.size() == 4096
    assert mmap.can_write()
    array_view = np.array(mmap, copy=False).view(np.uint32)
    assert np.all(array_view[:] == np.arange(1024, dtype=np.uint32))
    del array_view
    assert os.path.getsize(tmp_file) == 4096
    del mmap
    os.remove(tmp_file)


def test04_create_temp():
    mmap = MemoryMappedFile.create_temporary(123)
    fname = str(mmap.filename())
    assert os.path.exists(fname)
    assert mmap.size() == 123
    assert mmap.can_write()
    del mmap
    assert not os.path.exists(fname)
