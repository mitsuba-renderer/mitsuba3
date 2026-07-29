import pytest
import drjit as dr
import mitsuba as mi

from mitsuba.scalar_rgb.test.util import fresolver_append_path, \
    quad_corners

@fresolver_append_path
def test01_init(variants_all_ad_rgb):
    pytest.importorskip("cholespy")

    mesh = mi.load_dict({
        "type" : "ply",
        "filename" : "resources/data/tests/ply/triangle.ply",
    })
    params = mi.traverse(mesh)


    lambda_ = 25
    ls = mi.ad.LargeSteps(params['positions'], params['faces'], lambda_)


@fresolver_append_path
def test02_roundtrip(variants_all_ad_rgb):
    pytest.importorskip("cholespy")

    mesh = mi.load_dict({
        "type" : "ply",
        "filename" : "resources/data/tests/ply/triangle.ply",
    })
    params = mi.traverse(mesh)

    lambda_ = 25
    ls = mi.ad.LargeSteps(params['positions'], params['faces'], lambda_)

    u = ls.to_differential(params['positions'])
    assert u.shape == (ls.n_verts, 3)
    roundtrip = ls.from_differential(u)
    assert roundtrip.shape == params['positions'].shape
    assert dr.allclose(params['positions'], roundtrip, atol=1e-6)


def test03_non_unique_vertices(variants_all_ad_rgb):
    pytest.importorskip("cholespy")

    import numpy as np
    mesh = mi.Mesh("MyMesh", faces=[[0, 1, 2], [3, 4, 5]],
                   positions=[[0, 0, 0], [1, 0, 0], [0, 1, 0],
                              [1, 0, 0], [1, 1, 0], [0, 1, 0]])
    params = mi.traverse(mesh)

    lambda_ = 25
    ls = mi.ad.LargeSteps(params['positions'], params['faces'], lambda_)
    assert ls.n_verts == 4


def test04_from_mesh_geometric_topology(variants_all_ad_rgb):
    pytest.importorskip("cholespy")
    import numpy as np

    # A quad with a UV seam, built from shared source vertices: the seam splits
    # the vertices while the surface points stay connected
    positions, cv, uv = quad_corners(seam=True)
    mesh = mi.Mesh("seam")
    mesh.from_corners(positions=positions, corner_vertex=cv, texcoords=uv)
    assert mesh.vertex_count() == 6 and mesh.position_count() == 4

    # The Laplacian consumes the coarse representation directly
    ls = mi.ad.LargeSteps.from_mesh(mesh)
    assert ls.n_verts == 4

    v = mesh.positions()
    v2 = ls.from_differential(ls.to_differential(v))
    assert dr.allclose(v, v2, atol=1e-5)
