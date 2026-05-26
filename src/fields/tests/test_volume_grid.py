import os

import pytest
import drjit as dr
import mitsuba as mi


def test01_grid_construct(variant_scalar_rgb, tmpdir):
    tmp_file = os.path.join(str(tmpdir), "out.vol")
    grid = dr.full(mi.TensorXf, 1, [4, 8, 16, 3]) * 1.5
    grid[..., 1] = 0.5
    grid[..., 2] = 0.0

    mi.VolumeGrid(grid).write(tmp_file)
    vol = mi.load_dict({
        "type": "gridvolume",
        "filename": tmp_file,
    })
    it = dr.zeros(mi.Interaction3f, 1)

    assert vol is not None
    assert dr.allclose(vol.eval(it), mi.Vector3f(1.5, 0.5, 0.0))


def test02_nearest_interpolation(variants_all, tmpdir):
    tmp_file = os.path.join(str(tmpdir), "out.vol")
    grid = dr.full(mi.TensorXf, 1, [3, 3, 3])
    grid[0, 0, 0] = 0.0
    grid[1, 1, 1] = 0.5
    mi.VolumeGrid(grid).write(tmp_file)
    vol = mi.load_dict({
        "type": "gridvolume",
        "filename": tmp_file,
        "filter_type": "nearest",
    })
    assert vol is not None
    it = dr.zeros(mi.Interaction3f, 1)
    assert dr.allclose(vol.eval(it), 0.0)
    it.p = mi.Point3f(1.0) * 1 / 6
    assert dr.allclose(vol.eval(it), 0.0)


def test03_trilinear_interpolation(variants_all, tmpdir):
    tmp_file = os.path.join(str(tmpdir), "out.vol")
    grid = dr.full(mi.TensorXf, 1, [3, 3, 3])
    grid[0, 0, 0] = 0.0
    grid[1, 1, 1] = 0.5
    mi.VolumeGrid(grid).write(tmp_file)
    vol = mi.load_dict({
        "type": "gridvolume",
        "filename": tmp_file,
    })
    it = dr.zeros(mi.Interaction3f, 1)
    assert vol is not None
    assert dr.allclose(vol.eval(it), 0.0)
    it.p = mi.Point3f(1.0) / 3
    assert dr.allclose(vol.eval(it), (6 * 1 + 1 * 0.5 + 1 * 0.0) / 8)


def test04_trilinear_interpolation_boundary(variants_all, tmpdir):
    tmp_file = os.path.join(str(tmpdir), "out.vol")
    grid = dr.full(mi.TensorXf, 1, [3, 8, 5])
    grid[0, 0, 0] = 0.0
    grid[1, 1, 1] = 0.5
    grid[:, 7, :] = 0.55
    grid[0, :, :] = 0.32

    mi.VolumeGrid(grid).write(tmp_file)
    vol = mi.load_dict({
        "type": "gridvolume",
        "filename": tmp_file,
    })
    it = dr.zeros(mi.Interaction3f, 1)
    assert vol is not None
    it.p = mi.Point3f(0.2, 1.0, 0.7)
    assert dr.allclose(vol.eval(it), 0.55)

    it.p = mi.Point3f(0.5, 0.5, 0.0)
    assert dr.allclose(vol.eval(it), 0.32)


def test05_trilinear_interpolation_6_channels(variants_all_rgb, tmpdir):
    tmp_file = os.path.join(str(tmpdir), "out.vol")
    grid = dr.full(mi.TensorXf, 1, [3, 3, 3, 6])
    grid[0, 0, 0, :] = 0.0
    grid[1, 1, 1, :] = 0.5
    mi.VolumeGrid(grid).write(tmp_file)
    vol = mi.load_dict({
        "type": "gridvolume",
        "filename": tmp_file,
    })
    it = dr.zeros(mi.Interaction3f, 1)
    assert vol is not None
    assert dr.allclose(vol.eval_6(it), 0.0)
    it.p = mi.Point3f(1.0) / 3
    assert dr.allclose(vol.eval_6(it), (6 * 1 + 1 * 0.5 + 1 * 0.0) / 8)


def test06_eval_per_channel(variants_all_rgb, tmpdir):
    tmp_file = os.path.join(str(tmpdir), "out.vol")
    grid = dr.full(mi.TensorXf, 1, [3, 3, 3, 6])
    grid[0, 0, 0, :] = 0.0
    for i in range(6):
        grid[2, 2, 2, i] = i + 1
    mi.VolumeGrid(grid).write(tmp_file)
    vol = mi.load_dict({
        "type": "gridvolume",
        "filename": tmp_file,
    })
    it = dr.zeros(mi.Interaction3f, 1)
    assert vol is not None
    assert dr.allclose(vol.eval_n(it), [0.0] * 6)
    it.p = mi.Point3f(1.0)
    assert dr.allclose(vol.eval_n(it), [1.0, 2.0, 3.0, 4.0, 5.0, 6.0])


def test07_parameter_update_rejects_channel_count_changes(variant_scalar_rgb):
    vol = mi.load_dict({
        "type": "gridvolume",
        "data": mi.TensorXf([1.0], shape=(1, 1, 1, 1)),
        "raw": True,
        "filter_type": "nearest",
        "wrap_mode": "clamp",
    })

    assert vol.out_dim() == 1
    assert vol.channel_count() == 1
    assert dr.allclose(vol.max_per_channel(), [1.0])

    params = mi.traverse(vol)
    params["data"] = mi.TensorXf([2.0], shape=(1, 1, 1, 1))
    params.update()

    it = dr.zeros(mi.Interaction3f)
    assert vol.out_dim() == 1
    assert vol.channel_count() == 1
    assert dr.allclose(vol.max_per_channel(), [2.0])
    assert dr.allclose(mi.Field.eval_n(vol, it, 1), [2.0])

    params["data"] = mi.TensorXf([1.0, 2.0], shape=(1, 1, 1, 2))
    with pytest.raises(RuntimeError, match="resolution|channel count|traversal"):
        params.update()
