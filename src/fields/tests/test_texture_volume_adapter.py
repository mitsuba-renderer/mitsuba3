import pytest
import drjit as dr
import mitsuba as mi
import os


def test01_eval(variants_vec_backends_once, tmpdir):

    tmp_file = os.path.join(str(tmpdir), "out.vol")
    grid = dr.full(mi.TensorXf, 0.9, [3, 3, 3])
    grid[:, 0, :] = 0.0
    mi.VolumeGrid(grid).write(tmp_file)
    texture = mi.load_dict({
        'type': 'volume',
        'volume': {'type': 'gridvolume', 'filename': tmp_file},
    })
    si = dr.zeros(mi.SurfaceInteraction3f)
    si.p = mi.Point3f([0.4, 0.4, 0.3], [0.5, 0.0, -0.5], [0.5, 0.4, 0.5])
    assert dr.allclose(dr.mean(texture.eval(si)), [0.9, 0.0, 0.0])
    assert dr.allclose(texture.max(), 0.9)


def test02_eval_1(variants_vec_backends_once, tmpdir):

    tmp_file = os.path.join(str(tmpdir), "out.vol")
    grid = dr.full(mi.TensorXf, 0.9, [3, 3, 3])
    grid[:, 0, :] = 0.0
    mi.VolumeGrid(grid).write(tmp_file)
    texture = mi.load_dict({
        'type': 'volume',
        'volume': {'type': 'gridvolume', 'filename': tmp_file},
    })
    si = dr.zeros(mi.SurfaceInteraction3f)
    si.p = mi.Point3f([0.4, 0.4, 0.3], [0.5, 0.0, -0.5], [0.5, 0.4, 0.5])
    assert dr.allclose(texture.eval_1(si), [0.9, 0.0, 0.0])


def test03_eval_3(variants_vec_backends_once_rgb, tmpdir):
    tmp_file = os.path.join(str(tmpdir), "out.vol")
    grid = dr.full(mi.TensorXf, 0.9, [3, 3, 3, 3])
    grid[..., 1] = 0.4
    grid[..., 2] = 0.2
    grid[:, 0, :, :] = 0.0

    mi.VolumeGrid(grid).write(tmp_file)
    texture = mi.load_dict({
        'type': 'volume',
        'volume': {'type': 'gridvolume', 'filename': tmp_file},
    })
    si = dr.zeros(mi.SurfaceInteraction3f)
    si.p = mi.Point3f([0.4, 0.4, 0.3], [0.5, 0.0, -0.5], [0.5, 0.4, 0.5])
    ref_values = mi.Color3f([0.9, 0.0, 0.0], [0.4, 0.0, 0.0], [0.2, 0.0, 0.0])
    assert dr.allclose(texture.eval_3(si), ref_values)
    assert dr.allclose(texture.max(), 0.9)


def test04_load_tensor(variants_vec_backends_once):
    grid = dr.full(mi.TensorXf, 0.9, [3, 3, 3])
    grid[:, 0, :] = 0.0
    texture = mi.load_dict({
        'type': 'volume',
        'volume': {'type': 'gridvolume', 'data': grid},
    })
    si = dr.zeros(mi.SurfaceInteraction3f)
    si.p = mi.Point3f([0.4, 0.4, 0.3], [0.5, 0.0, -0.5], [0.5, 0.4, 0.5])
    assert dr.allclose(dr.mean(texture.eval(si)), [0.9, 0.0, 0.0])
    assert dr.allclose(texture.max(), 0.9)
