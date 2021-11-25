import numpy as np
import os

import mitsuba
import enoki as ek


def test01_grid_construct(variant_scalar_rgb, tmpdir):
    from mitsuba.core import load_string, Vector3f
    from mitsuba.render import VolumeGrid, Interaction3f

    tmp_file = os.path.join(str(tmpdir), "out.vol")
    grid = np.ones((4, 8, 16, 3)).astype(np.float32) * 1.5
    grid[..., 1] = 0.5
    grid[..., 2] = 0.0

    VolumeGrid(grid).write(tmp_file)
    vol = load_string(f"""<volume type="gridvolume" version="2.0.0">
                              <string name="filename" value="{tmp_file}"/>
                          </volume>""").expand()[0]
    it = ek.zero(Interaction3f, 1)

    assert vol is not None
    assert np.allclose(vol.eval(it), Vector3f(1.5, 0.5, 0.0))


def test02_nearest_interpolation(variants_all, tmpdir):
    from mitsuba.core import load_string, Point3f
    from mitsuba.render import VolumeGrid, Interaction3f

    tmp_file = os.path.join(str(tmpdir), "out.vol")
    grid = np.ones((3, 3, 3)).astype(np.float32)
    grid[0, 0, 0] = 0.0
    grid[1, 1, 1] = 0.5
    VolumeGrid(grid).write(tmp_file)
    vol = load_string(f"""<volume type="gridvolume" version="2.0.0">
                              <string name="filename" value="{tmp_file}"/>
                              <string name="filter_type" value="nearest"/>
                          </volume>""").expand()[0]
    assert vol is not None
    it = ek.zero(Interaction3f, 1)
    assert np.allclose(vol.eval(it), 0.0)
    it.p = Point3f(1.0) * 1/6
    assert np.allclose(vol.eval(it), 0.0)


def test03_trilinear_interpolation(variants_all, tmpdir):
    from mitsuba.core import load_string, Point3f
    from mitsuba.render import VolumeGrid, Interaction3f

    tmp_file = os.path.join(str(tmpdir), "out.vol")
    grid = np.ones((3, 3, 3)).astype(np.float32)
    grid[0, 0, 0] = 0.0
    grid[1, 1, 1] = 0.5
    VolumeGrid(grid).write(tmp_file)
    vol = load_string(f"""<volume type="gridvolume" version="2.0.0">
                              <string name="filename" value="{tmp_file}"/>
                          </volume>""").expand()[0]
    it = ek.zero(Interaction3f, 1)
    assert vol is not None
    assert np.allclose(vol.eval(it), 0.0)
    it.p = Point3f(1.0) / 3
    assert np.allclose(vol.eval(it), (6 * 1 + 1 * 0.5 + 1 * 0.0) / 8)


def test04_trilinear_interpolation_boundary(variants_all, tmpdir):
    from mitsuba.core import load_string, Point3f
    from mitsuba.render import VolumeGrid, Interaction3f

    tmp_file = os.path.join(str(tmpdir), "out.vol")
    grid = np.ones((3, 8, 5)).astype(np.float32)
    grid[0, 0, 0] = 0.0
    grid[1, 1, 1] = 0.5
    grid[:, 7, :] = 0.55
    grid[0, :, :] = 0.32

    VolumeGrid(grid).write(tmp_file)
    vol = load_string(f"""<volume type="gridvolume" version="2.0.0">
                              <string name="filename" value="{tmp_file}"/>
                          </volume>""").expand()[0]
    it = ek.zero(Interaction3f, 1)
    assert vol is not None
    it.p = Point3f(0.2, 1.0, 0.7)
    assert np.allclose(vol.eval(it), 0.55)

    it.p = Point3f(0.5, 0.5, 0.0)
    assert np.allclose(vol.eval(it), 0.32)
