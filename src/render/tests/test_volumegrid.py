import mitsuba
import pytest

import numpy as np
import os


def test01_numpy_conversion(variants_all_scalar, np_rng):
    from mitsuba.render import VolumeGrid

    a = np_rng.random((4, 8, 16, 3))
    grid = VolumeGrid(a)
    assert np.allclose(a, np.array(grid))
    assert np.allclose(np.max(a), grid.max())
    assert np.allclose([a.shape[2], a.shape[1], a.shape[0]], grid.size())
    assert np.allclose(a.shape[3], grid.channel_count())

    # Don't ask for computation of the maximum value
    grid = VolumeGrid(a, False)
    assert np.allclose(a, np.array(grid))
    assert np.allclose(grid.max(), 0.0)
    assert np.allclose([a.shape[2], a.shape[1], a.shape[0]], grid.size())
    assert np.allclose(a.shape[3], grid.channel_count())


def test02_read_write(variants_all_scalar, tmpdir, np_rng):
    from mitsuba.render import VolumeGrid

    tmp_file = os.path.join(str(tmpdir), "out.vol")
    grid = np_rng.random((4, 8, 16, 3))
    VolumeGrid(grid).write(tmp_file)
    loaded = VolumeGrid(tmp_file)
    assert np.allclose(np.array(loaded), grid)
    assert np.allclose(loaded.max(), np.max(grid))
    assert np.allclose([grid.shape[2], grid.shape[1], grid.shape[0]], loaded.size())
    assert np.allclose(grid.shape[3], loaded.channel_count())
