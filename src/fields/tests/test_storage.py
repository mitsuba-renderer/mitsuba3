import os

import pytest
import drjit as dr
import mitsuba as mi


def bitmap_data(channels=3, height=3, width=4):
    values = (dr.arange(mi.Float, height * width * channels) + 1) / 17.0
    return mi.TensorXf(values, shape=(height, width, channels))


def volume_data(channels=6, z=3, y=4, x=5):
    values = (dr.arange(mi.Float, z * y * x * channels) + 1) / 29.0
    return mi.TensorXf(values, shape=(z, y, x, channels))


def bitmap_field_dict(channels=3, **kwargs):
    result = {
        "type": "bitmapfield",
        "data": bitmap_data(channels),
        "raw": True,
        "filter_type": "nearest",
        "wrap_mode": "clamp",
    }
    result.update(kwargs)
    return result


def grid_field_dict(channels=6, **kwargs):
    result = {
        "type": "gridfield",
        "data": volume_data(channels),
        "raw": True,
        "filter_type": "nearest",
        "wrap_mode": "clamp",
    }
    result.update(kwargs)
    return result


def bitmap_texture_dict(data, **kwargs):
    result = {
        "type": "bitmap",
        "data": data,
        "raw": True,
        "filter_type": "nearest",
        "wrap_mode": "clamp",
    }
    result.update(kwargs)
    return result


def grid_volume_dict(data, **kwargs):
    result = {
        "type": "gridvolume",
        "data": data,
        "raw": True,
        "filter_type": "nearest",
        "wrap_mode": "clamp",
    }
    result.update(kwargs)
    return result


def surface_interaction(width=1):
    si = dr.zeros(mi.SurfaceInteraction3f, width)
    if width == 1:
        si.uv = mi.Point2f(0.25, 0.75)
    else:
        si.uv = mi.Point2f(
            mi.Float(-0.25, 0.125, 0.5, 0.9, 1.25),
            mi.Float(1.20, 0.375, 0.5, -0.1, 0.75),
        )
    si.p = mi.Point3f(si.uv.x, si.uv.y, 0)
    si.n = mi.Normal3f(0, 0, 1)
    si.wi = mi.Vector3f(0, 0, 1)
    si.sh_frame = mi.Frame3f(si.n)
    return si


def interaction(width=1):
    it = dr.zeros(mi.Interaction3f, width)
    if width == 1:
        it.p = mi.Point3f(0.25, 0.5, 0.75)
    else:
        it.p = mi.Point3f(
            mi.Float(-0.25, 0.125, 0.5, 0.9, 1.25),
            mi.Float(1.20, 0.375, 0.5, -0.1, 0.75),
            mi.Float(0.1, 0.25, 0.5, 0.75, 1.4),
        )
    return it


def test01_storage_fields_report_metadata_and_capabilities(variant_scalar_rgb):
    mono = mi.load_dict(bitmap_field_dict(channels=1))
    color = mi.load_dict(bitmap_field_dict(channels=3))
    volume3 = mi.load_dict(grid_field_dict(channels=3))
    volume6 = mi.load_dict(grid_field_dict(channels=6))

    assert isinstance(color, mi.Field)
    assert color.domain() == mi.FieldDomain.Surface
    assert color.out_type() == mi.FieldValueType.Color3
    assert color.out_dim() == 3
    assert color.args_dim() == 0
    assert color.supports_scalar()
    assert color.supports_jit()
    assert color.supports_surface_queries()
    assert not color.supports_interaction_queries()

    assert mono.out_type() == mi.FieldValueType.Float
    assert mono.out_dim() == 1

    assert isinstance(volume6, mi.Field)
    assert volume6.domain() == mi.FieldDomain.Interaction
    assert volume6.out_type() == mi.FieldValueType.Features
    assert volume6.out_dim() == 6
    assert volume6.args_dim() == 0
    assert volume6.supports_scalar()
    assert volume6.supports_jit()
    assert not volume6.supports_surface_queries()
    assert volume6.supports_interaction_queries()

    assert volume3.out_type() == mi.FieldValueType.Array3
    assert volume3.out_dim() == 3


@pytest.mark.parametrize("filter_type", ["nearest", "bilinear"])
@pytest.mark.parametrize("wrap_mode", ["repeat", "clamp", "mirror"])
def test02_bitmap_field_matches_bitmap_texture_for_filters_wraps_and_masks(
    variants_all_rgb, filter_type, wrap_mode
):
    data = bitmap_data(channels=3)
    texture = mi.load_dict(bitmap_texture_dict(
        data, filter_type=filter_type, wrap_mode=wrap_mode
    ))
    field = mi.load_dict(bitmap_field_dict(
        channels=3, data=data, filter_type=filter_type, wrap_mode=wrap_mode
    ))
    si = surface_interaction(width=5)
    active = mi.Bool([True, False, True, True, False])

    assert dr.allclose(field.eval_color3(si, active=active),
                       texture.eval_3(si, active), atol=1e-6)

    mono_data = bitmap_data(channels=1)
    mono_texture = mi.load_dict(bitmap_texture_dict(
        mono_data, filter_type=filter_type, wrap_mode=wrap_mode
    ))
    mono_field = mi.load_dict(bitmap_field_dict(
        channels=1, data=mono_data, filter_type=filter_type, wrap_mode=wrap_mode
    ))

    assert dr.allclose(mono_field.eval_1(si, active=active),
                       mono_texture.eval_1(si, active), atol=1e-6)
    assert dr.allclose(mono_field.eval(si, active=active)[0],
                       mono_texture.eval_1(si, active), atol=1e-6)


@pytest.mark.parametrize("filter_type", ["nearest", "trilinear"])
@pytest.mark.parametrize("wrap_mode", ["repeat", "clamp", "mirror"])
def test03_grid_field_matches_grid_volume_for_filters_wraps_and_masks(
    variants_all_rgb, filter_type, wrap_mode
):
    data6 = volume_data(channels=6)
    volume6 = mi.load_dict(grid_volume_dict(
        data6, filter_type=filter_type, wrap_mode=wrap_mode
    ))
    field6 = mi.load_dict(grid_field_dict(
        channels=6, data=data6, filter_type=filter_type, wrap_mode=wrap_mode
    ))
    it = interaction(width=5)
    active = mi.Bool([True, False, True, True, False])

    assert dr.allclose(field6.eval_array6(it, active=active),
                       volume6.eval_6(it, active), atol=1e-6)
    assert dr.allclose(field6.eval_n(it, 6, active=active),
                       volume6.eval_n(it, active), atol=1e-6)

    data3 = volume_data(channels=3)
    volume3 = mi.load_dict(grid_volume_dict(
        data3, filter_type=filter_type, wrap_mode=wrap_mode
    ))
    field3 = mi.load_dict(grid_field_dict(
        channels=3, data=data3, filter_type=filter_type, wrap_mode=wrap_mode
    ))
    assert dr.allclose(field3.eval_array3(it, active=active),
                       volume3.eval_3(it, active), atol=1e-6)

    data1 = volume_data(channels=1)
    volume1 = mi.load_dict(grid_volume_dict(
        data1, filter_type=filter_type, wrap_mode=wrap_mode
    ))
    field1 = mi.load_dict(grid_field_dict(
        channels=1, data=data1, filter_type=filter_type, wrap_mode=wrap_mode
    ))
    assert dr.allclose(field1.eval_1(it, active=active),
                       volume1.eval_1(it, active), atol=1e-6)


def test04_fixed_query_metadata_and_domain_mismatches_are_errors(variant_scalar_rgb):
    si = surface_interaction()
    it = interaction()

    color = mi.load_dict(bitmap_field_dict(channels=3))
    grid3 = mi.load_dict(grid_field_dict(channels=3))
    grid6 = mi.load_dict(grid_field_dict(channels=6))

    with pytest.raises(RuntimeError, match="Array3|Color3|out_type"):
        color.eval_array3(si)
    with pytest.raises(RuntimeError, match="6|out_dim|Color3"):
        color.eval_array6(si)
    with pytest.raises(RuntimeError, match="Color3|Array3|out_type"):
        grid3.eval_color3(it)
    with pytest.raises(RuntimeError, match="Array3|6|out_dim|Features"):
        grid6.eval_array3(it)
    with pytest.raises(RuntimeError, match="domain|Interaction|Surface"):
        color.eval_color3(it)
    with pytest.raises(RuntimeError, match="domain|Surface|Interaction"):
        grid6.eval_array6(si)


def test05_eval_n_count_must_match_output_dimension(variant_scalar_rgb):
    color = mi.load_dict(bitmap_field_dict(channels=3))
    grid = mi.load_dict(grid_field_dict(channels=6))

    assert len(color.eval_n(surface_interaction(), 3)) == 3
    assert len(grid.eval_n(interaction(), 6)) == 6

    for field, record, count in [
        (color, surface_interaction(), 2),
        (color, surface_interaction(), 4),
        (grid, interaction(), 5),
        (grid, interaction(), 7),
    ]:
        with pytest.raises(RuntimeError, match="count|out_dim"):
            field.eval_n(record, count)


def test06_storage_field_traversal_and_parameters_changed_update_storage(field_ad_rgb_variant):
    field = mi.load_dict(bitmap_field_dict(channels=1))
    params = mi.traverse(field)

    assert "data" in params
    assert params.flags("data") & mi.ParamFlags.Differentiable

    si = surface_interaction()
    before = field.eval_1(si)
    dr.eval(before)
    dr.sync_thread()
    before_value = float(before[0])
    params["data"] = dr.full(mi.TensorXf, 0.75, shape=dr.shape(params["data"]))
    params.update()
    after = field.eval_1(si)
    dr.eval(after)
    dr.sync_thread()
    assert before_value != float(after[0])
    assert dr.allclose(after, 0.75)

    params = mi.traverse(field)
    params["data"] = dr.ones(mi.TensorXf, shape=(2, 2))
    with pytest.raises(RuntimeError, match="rank|dimension|Tensor|data"):
        params.update()


def test07_grid_field_loads_volume_grid_file_and_tensor_equivalently(variant_scalar_rgb, tmpdir):
    data = volume_data(channels=6)
    tmp_file = os.path.join(str(tmpdir), "field_grid.vol")
    mi.VolumeGrid(data).write(tmp_file)

    tensor_field = mi.load_dict(grid_field_dict(channels=6, data=data))
    file_field = mi.load_dict({
        "type": "gridfield",
        "filename": tmp_file,
        "raw": True,
        "filter_type": "nearest",
        "wrap_mode": "clamp",
    })

    it = interaction()
    assert dr.allclose(file_field.eval_array6(it), tensor_field.eval_array6(it))
