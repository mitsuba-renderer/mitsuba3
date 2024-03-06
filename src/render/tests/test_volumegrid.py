import pytest
import drjit as dr
import mitsuba as mi
import numpy as np
import os


def test01_numpy_conversion(variants_all_scalar, np_rng):
    a = np_rng.random((4, 8, 16, 3))
    grid = mi.VolumeGrid(a)
    assert dr.allclose(a, np.array(grid), atol=1e-3, rtol=1e-5)
    assert dr.allclose(np.max(a), grid.max())
    assert dr.allclose([a.shape[2], a.shape[1], a.shape[0]], grid.size())
    assert dr.allclose(a.shape[3], grid.channel_count())

    # Don't ask for computation of the maximum value
    grid = mi.VolumeGrid(a, False)
    assert dr.allclose(a, np.array(grid), atol=1e-3, rtol=1e-5)
    assert dr.allclose(grid.max(), 0.0)
    assert dr.allclose([a.shape[2], a.shape[1], a.shape[0]], grid.size())
    assert dr.allclose(a.shape[3], grid.channel_count())


def test02_read_write(variants_all_scalar, tmpdir, np_rng):
    tmp_file = os.path.join(str(tmpdir), "out.vol")
    grid = np_rng.random((4, 8, 16, 3))
    mi.VolumeGrid(grid).write(tmp_file)
    loaded = mi.VolumeGrid(tmp_file)
    assert dr.allclose(np.array(loaded), grid)
    assert dr.allclose(loaded.max(), np.max(grid))
    assert dr.allclose([grid.shape[2], grid.shape[1], grid.shape[0]], loaded.size())
    assert dr.allclose(grid.shape[3], loaded.channel_count())

def test03_max_per_channel(variants_all_scalar, tmpdir, np_rng):
    tmp_file = os.path.join(str(tmpdir), "out.vol")
    data = np_rng.random((4, 8, 16, 3))
    grid = mi.VolumeGrid(data)
    grid.write(tmp_file)
    np_max_per_channel = np.array([np.max(data[:,:,:,0]),
                                   np.max(data[:,:,:,1]),
                                   np.max(data[:,:,:,2])])
    # Check direct construction compute the maximum values
    mi_max_per_channel = grid.max_per_channel()
    assert dr.allclose(np_max_per_channel, mi_max_per_channel)
    # Check disk construction compute the maximum values
    grid = mi.VolumeGrid(tmp_file)
    mi_max_per_channel = grid.max_per_channel()
    assert dr.allclose(np_max_per_channel, mi_max_per_channel)
