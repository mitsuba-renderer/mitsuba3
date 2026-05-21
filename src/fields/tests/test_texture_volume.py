import pytest
import drjit as dr
import mitsuba as mi


def make_si():
    si = dr.zeros(mi.SurfaceInteraction3f)
    si.uv = mi.Point2f(0.4, 0.6)
    si.p = mi.Point3f(0.4, 0.6, 0.0)
    return si


def make_it():
    it = dr.zeros(mi.Interaction3f)
    it.p = mi.Point3f(0.25, 0.5, 0.75)
    return it


def test01_bitmap_texture_public_surface_does_not_grow_field_args(variant_scalar_rgb):
    texture = mi.load_dict({
        "type": "bitmap",
        "data": dr.ones(mi.TensorXf, shape=[4, 4, 3]),
        "raw": True,
        "filter_type": "bilinear",
        "wrap_mode": "repeat",
    })
    si = make_si()

    texture.eval(si)
    texture.eval_1(si)
    texture.eval_3(si)
    texture.eval_1_grad(si)
    texture.sample_position(mi.Point2f(0.2, 0.8))
    texture.pdf_position(mi.Point2f(0.2, 0.8))

    for method in ["eval", "eval_1", "eval_3", "eval_1_grad"]:
        with pytest.raises(TypeError):
            getattr(texture, method)(si, args=[1.0])


def test02_grid_volume_public_surface_does_not_grow_field_args(variant_scalar_rgb):
    volume1 = mi.load_dict({
        "type": "gridvolume",
        "data": dr.ones(mi.TensorXf, shape=[4, 4, 4, 1]),
        "raw": True,
        "filter_type": "trilinear",
        "wrap_mode": "clamp",
    })
    volume3 = mi.load_dict({
        "type": "gridvolume",
        "data": dr.ones(mi.TensorXf, shape=[4, 4, 4, 3]),
        "raw": True,
        "filter_type": "trilinear",
        "wrap_mode": "clamp",
    })
    volume6 = mi.load_dict({
        "type": "gridvolume",
        "data": dr.ones(mi.TensorXf, shape=[4, 4, 4, 6]),
        "raw": True,
        "filter_type": "trilinear",
        "wrap_mode": "clamp",
    })
    it = make_it()

    volume1.eval(it)
    volume1.eval_1(it)
    volume3.eval_3(it)
    volume6.eval_6(it)
    volume6.eval_n(it)
    volume6.max()
    volume6.max_per_channel()

    for volume, method in [
        (volume1, "eval"),
        (volume1, "eval_1"),
        (volume3, "eval_3"),
        (volume6, "eval_6"),
        (volume6, "eval_n"),
        (volume1, "eval_gradient"),
    ]:
        with pytest.raises(TypeError):
            getattr(volume, method)(it, args=[1.0])


def test03_bitmap_texture_rejects_public_nested_field_property(variant_scalar_rgb):
    with pytest.raises(RuntimeError, match="bitmapfield|field|unqueried|unexpected|object"):
        mi.load_dict({
            "type": "bitmap",
            "data": dr.ones(mi.TensorXf, shape=[2, 2, 3]),
            "raw": True,
            "storage": {
                "type": "bitmapfield",
                "data": dr.ones(mi.TensorXf, shape=[2, 2, 3]),
                "raw": True,
            },
        })


def test04_gridvolume_keeps_transform_bbox_and_channel_metadata(variant_scalar_rgb):
    data = dr.zeros(mi.TensorXf, shape=[2, 2, 2, 6])
    for i in range(6):
        data[..., i] = i + 1

    volume = mi.load_dict({
        "type": "gridvolume",
        "data": data,
        "raw": True,
        "to_world": mi.ScalarTransform4f.translate([1.0, 2.0, 3.0])
                    @ mi.ScalarTransform4f.scale([2.0, 4.0, 8.0]),
    })

    assert volume.channel_count() == 6
    assert dr.all(volume.resolution() == [2, 2, 2])
    assert dr.allclose(volume.max(), 6.0)
    assert dr.allclose(volume.max_per_channel(), [1, 2, 3, 4, 5, 6])

    bbox = volume.bbox()
    assert dr.allclose(bbox.min, [1.0, 2.0, 3.0])
    assert dr.allclose(bbox.max, [3.0, 6.0, 11.0])


def test05_field_refactor_preserves_texture_and_volume_plugin_types(variant_scalar_rgb):
    pmgr = mi.PluginManager.instance()

    assert pmgr.plugin_type("bitmap") == mi.ObjectType.Texture
    assert pmgr.plugin_type("gridvolume") == mi.ObjectType.Volume
