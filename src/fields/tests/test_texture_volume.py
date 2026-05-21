import os

import pytest
import drjit as dr
import mitsuba as mi


def make_si(width=1):
    si = dr.zeros(mi.SurfaceInteraction3f, width)
    si.uv = mi.Point2f(0.4, 0.6)
    si.p = mi.Point3f(0.4, 0.6, 0.0)
    si.n = mi.Normal3f(0, 0, 1)
    si.wi = mi.Vector3f(0, 0, 1)
    si.sh_frame = mi.Frame3f(si.n)
    return si


def make_it(p=(0.25, 0.5, 0.75)):
    it = dr.zeros(mi.Interaction3f)
    it.p = mi.Point3f(*p)
    return it


def constant_bitmap_rgb(value=(0.2, 0.4, 0.6)):
    data = dr.zeros(mi.TensorXf, shape=[2, 2, 3])
    data[..., 0] = value[0]
    data[..., 1] = value[1]
    data[..., 2] = value[2]
    return data


def ramp_volume(channels=6):
    data = dr.zeros(mi.TensorXf, shape=[2, 2, 2, channels])
    for i in range(channels):
        data[..., i] = i + 1
    return data


def test01_bitmap_texture_public_surface_does_not_grow_field_args(variant_scalar_rgb):
    texture = mi.load_dict({
        "type": "bitmap",
        "data": constant_bitmap_rgb(),
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
        "data": ramp_volume(channels=1),
        "raw": True,
        "filter_type": "trilinear",
        "wrap_mode": "clamp",
    })
    volume3 = mi.load_dict({
        "type": "gridvolume",
        "data": ramp_volume(channels=3),
        "raw": True,
        "filter_type": "trilinear",
        "wrap_mode": "clamp",
    })
    volume6 = mi.load_dict({
        "type": "gridvolume",
        "data": ramp_volume(channels=6),
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


def test03_texture_and_volume_do_not_accept_public_field_storage_properties(variant_scalar_rgb):
    with pytest.raises(RuntimeError, match="bitmapfield|field|unqueried|unexpected|object"):
        mi.load_dict({
            "type": "bitmap",
            "data": constant_bitmap_rgb(),
            "raw": True,
            "storage": {
                "type": "bitmapfield",
                "data": constant_bitmap_rgb(),
                "raw": True,
            },
        })

    with pytest.raises(RuntimeError, match="gridfield|field|unqueried|unexpected|object"):
        mi.load_dict({
            "type": "gridvolume",
            "data": ramp_volume(channels=1),
            "raw": True,
            "storage": {
                "type": "gridfield",
                "data": ramp_volume(channels=1),
                "raw": True,
            },
        })


def test04_bitmap_texture_public_behavior_and_traversal_keys_are_unchanged(variant_scalar_rgb):
    color = (0.2, 0.4, 0.6)
    texture = mi.load_dict({
        "type": "bitmap",
        "data": constant_bitmap_rgb(color),
        "raw": True,
        "filter_type": "bilinear",
        "wrap_mode": "repeat",
    })
    si = make_si()

    assert dr.all(texture.resolution() == [2, 2])
    assert dr.allclose(texture.eval_3(si), color)
    assert dr.allclose(texture.eval(si), color)
    assert dr.allclose(texture.eval_1(si), mi.luminance(color))
    assert dr.allclose(texture.mean(), mi.luminance(color))

    sample, pdf = texture.sample_position(mi.Point2f(0.25, 0.75))
    assert dr.all(sample >= 0)
    assert dr.all(sample <= 1)
    assert dr.allclose(texture.pdf_position(sample), pdf)

    params = mi.traverse(texture)
    assert "data" in params
    assert "to_uv" in params
    assert not any("field" in key or "storage" in key for key in params.keys())


def test05_gridvolume_keeps_transform_bbox_channel_metadata_and_traversal_keys(variant_scalar_rgb):
    data = ramp_volume(channels=6)
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

    params = mi.traverse(volume)
    assert "data" in params
    assert not any("field" in key or "storage" in key for key in params.keys())


def test06_gridvolume_transformed_lookup_matches_unit_volume_lookup(variant_scalar_rgb):
    data = dr.zeros(mi.TensorXf, shape=[3, 3, 3, 1])
    data[1, 1, 1, 0] = 4.0
    unit = mi.load_dict({
        "type": "gridvolume",
        "data": data,
        "raw": True,
        "filter_type": "nearest",
        "wrap_mode": "clamp",
    })
    transformed = mi.load_dict({
        "type": "gridvolume",
        "data": data,
        "raw": True,
        "filter_type": "nearest",
        "wrap_mode": "clamp",
        "to_world": mi.ScalarTransform4f.translate([10.0, -2.0, 1.0])
                    @ mi.ScalarTransform4f.scale([2.0, 4.0, 8.0]),
    })

    local_it = make_it((0.5, 0.5, 0.5))
    world_it = make_it((11.0, 0.0, 5.0))
    assert dr.allclose(transformed.eval_1(world_it), unit.eval_1(local_it))
    assert dr.allclose(transformed.eval_n(world_it), unit.eval_n(local_it))


def test07_gridvolume_file_input_and_use_grid_bbox_behavior_is_preserved(variant_scalar_rgb, tmpdir):
    data = ramp_volume(channels=3)
    tmp_file = os.path.join(str(tmpdir), "grid.vol")
    mi.VolumeGrid(data).write(tmp_file)

    file_volume = mi.load_dict({
        "type": "gridvolume",
        "filename": tmp_file,
        "raw": True,
        "use_grid_bbox": True,
    })
    assert file_volume.channel_count() == 3
    assert dr.allclose(file_volume.max_per_channel(), [1, 2, 3])

    with pytest.raises(RuntimeError, match="use_grid_bbox|tensor input|volume grid"):
        mi.load_dict({
            "type": "gridvolume",
            "data": data,
            "raw": True,
            "use_grid_bbox": True,
        })


def test08_field_refactor_preserves_texture_and_volume_plugin_types(variant_scalar_rgb):
    pmgr = mi.PluginManager.instance()

    assert pmgr.plugin_type("bitmap") == mi.ObjectType.Texture
    assert pmgr.plugin_type("gridvolume") == mi.ObjectType.Volume
