import drjit as dr
import mitsuba as mi
import numpy as np


def grid_mesh(n=5):
    """An n x n vertex grid over [0, 1]^2 in the xy plane, UVs equal to xy."""
    x, y = np.meshgrid(np.linspace(0, 1, n), np.linspace(0, 1, n))
    p = np.stack([x.ravel(), y.ravel(), np.zeros(n * n)], 1)
    i = np.arange(n * n).reshape(n, n)
    a, b, c, d = i[:-1, :-1], i[:-1, 1:], i[1:, 1:], i[1:, :-1]
    f = np.concatenate([np.stack([a, b, c], -1).reshape(-1, 3),
                        np.stack([a, c, d], -1).reshape(-1, 3)])
    return mi.Mesh('grid', mi.TensorXu(f.astype(np.uint32)),
                   mi.TensorXf(p.astype(np.float32)),
                   texcoords=mi.TensorXf(p[:, :2].astype(np.float32)))


def step_texture():
    """0.2 on the left half of UV space, 0.8 on the right half."""
    img = np.array([[[0.2], [0.8]]], np.float32)
    return {'type': 'bitmap', 'bitmap': mi.Bitmap(img), 'raw': True,
            'filter_type': 'nearest', 'wrap_mode': 'clamp'}


def positions(mesh):
    return np.array(mesh.positions())[np.array(mesh.position_index())] \
        if len(mesh.position_index()) else np.array(mesh.positions())


def test01_constant_height(variants_all_backends_once):
    base = grid_mesh(n=3)
    base.add_attribute('vertex_weight', mi.TensorXf(
        np.arange(9, dtype=np.float32).reshape(9, 1)))
    mesh = mi.load_dict({
        'type': 'displace',
        'mesh': base,
        'height': 0.75,
        'scale': 1.0,
        'midlevel': 0.25,
        'to_world': mi.ScalarTransform4f().scale([-2, 2, 2]),
    })
    assert mesh.face_count() == 8 and mesh.vertex_count() == 9
    # Displaced in object space, then transformed
    p = positions(mesh)
    assert np.allclose(p[:, 2], 1.0)
    assert np.allclose(p[:, 0].min(), -2.0)
    # The mirroring transform keeps the geometric normal consistent
    assert dr.allclose(mesh.face_normal(mi.UInt32(0)), [0, 0, 1])
    assert np.allclose(np.array(mesh.attribute('vertex_weight')).ravel(),
                       np.arange(9))


def test02_texture(variants_all_backends_once):
    mesh = mi.load_dict({
        'type': 'displace',
        'mesh': grid_mesh(n=3),
        'height': step_texture(),
        'scale': 0.5,
        'midlevel': 0.0,
    })
    p = positions(mesh)
    # The middle column samples the texel boundary, the others are clear
    assert np.allclose(p[p[:, 0] < 0.25, 2], 0.1)
    assert np.allclose(p[p[:, 0] > 0.75, 2], 0.4)
    # Vertex normals were recomputed on the displaced surface
    assert np.any(np.abs(np.array(mesh.normals())[:, 0]) > 0.1)


def test03_footprint_prefilter(variants_all_backends_once):
    # A 64 x 64 checkerboard sampled by a 2 x 2 vertex quad. The footprint
    # spans the texture, so trilinear filtering returns the mean height
    i, j = np.meshgrid(np.arange(64), np.arange(64))
    img = ((i + j) % 2).astype(np.float32)[..., None]
    mesh = mi.load_dict({
        'type': 'displace',
        'mesh': grid_mesh(n=2),
        'height': {'type': 'bitmap', 'bitmap': mi.Bitmap(img),
                   'raw': True, 'filter_type': 'trilinear'},
        'midlevel': 0.0,
    })
    assert np.allclose(positions(mesh)[:, 2], 0.5, atol=0.02)
