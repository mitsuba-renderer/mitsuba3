import pytest
import drjit as dr
import mitsuba as mi

from mitsuba.scalar_rgb.test.util import fresolver_append_path

@fresolver_append_path
def test01_init(variants_all_ad_rgb):
    pytest.importorskip("cholespy")

    mesh = mi.load_dict({
        "type" : "ply",
        "filename" : "resources/data/tests/ply/triangle.ply",
    })
    params = mi.traverse(mesh)


    lambda_ = 25
    ls = mi.ad.LargeSteps(params['vertex_positions'], params['faces'], lambda_)


@fresolver_append_path
def test02_roundtrip(variants_all_ad_rgb):
    pytest.importorskip("cholespy")

    mesh = mi.load_dict({
        "type" : "ply",
        "filename" : "resources/data/tests/ply/triangle.ply",
    })
    params = mi.traverse(mesh)

    lambda_ = 25
    ls = mi.ad.LargeSteps(params['vertex_positions'], params['faces'], lambda_)

    initial = params['vertex_positions']
    roundtrip = ls.from_differential(ls.to_differential(params['vertex_positions']))
    assert dr.allclose(initial, roundtrip, atol=1e-6)


def test03_non_unique_vertices(variants_all_ad_rgb):
    pytest.importorskip("cholespy")

    mesh = mi.Mesh("MyMesh", 5, 2)
    params = mi.traverse(mesh)
    params['vertex_positions'] = [
        0.0, 0.0, 0.0,
        1.0, 0.0, 0.0,
        0.0, 1.0, 0.0,
        1.0, 0.0, 0.0,
        1.0, 1.0, 0.0,
        0.0, 1.0, 0.0,
    ]
    params['faces'] = [0, 1, 2, 3, 4, 5]
    params.update()

    lambda_ = 25
    ls = mi.ad.LargeSteps(params['vertex_positions'], params['faces'], lambda_)
    assert ls.n_verts == 4
