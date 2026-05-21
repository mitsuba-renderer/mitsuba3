import pytest
import drjit as dr
import mitsuba as mi


def bitmap_data(channels=3):
    values = dr.arange(mi.Float, 4 * channels) / 10 + 0.1
    return mi.TensorXf(values, shape=(2, 2, channels))


def volume_data(channels=6):
    values = dr.arange(mi.Float, 8 * channels) / 10 + 0.1
    return mi.TensorXf(values, shape=(2, 2, 2, channels))


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


def surface_interaction():
    si = dr.zeros(mi.SurfaceInteraction3f)
    si.p = mi.Point3f(0.25, 0.5, 0.75)
    si.uv = mi.Point2f(0.25, 0.75)
    si.n = mi.Normal3f(0, 0, 1)
    si.wi = mi.Vector3f(0, 0, 1)
    si.sh_frame = mi.Frame3f(si.n)
    return si


def interaction():
    it = dr.zeros(mi.Interaction3f)
    it.p = mi.Point3f(0.25, 0.5, 0.75)
    return it


def test01_storage_fields_report_metadata_and_capabilities(variant_scalar_rgb):
    color_field = mi.load_dict(bitmap_field_dict(channels=3))
    volume_field = mi.load_dict(grid_field_dict(channels=6))

    assert isinstance(color_field, mi.Field)
    assert color_field.domain() == mi.FieldDomain.Surface
    assert color_field.out_type() == mi.FieldValueType.Color3
    assert color_field.out_dim() == 3
    assert color_field.args_dim() == 0
    assert color_field.supports_scalar()
    assert color_field.supports_jit()

    assert isinstance(volume_field, mi.Field)
    assert volume_field.domain() == mi.FieldDomain.Interaction
    assert volume_field.out_type() == mi.FieldValueType.Features
    assert volume_field.out_dim() == 6
    assert volume_field.args_dim() == 0
    assert volume_field.supports_scalar()
    assert volume_field.supports_jit()


def test02_bitmap_field_matches_bitmap_texture_fixed_paths(variant_scalar_rgb):
    data = bitmap_data(channels=3)
    texture = mi.load_dict({
        "type": "bitmap",
        "data": data,
        "raw": True,
        "filter_type": "nearest",
        "wrap_mode": "clamp",
    })
    field = mi.load_dict(bitmap_field_dict(channels=3, data=data))
    si = surface_interaction()

    assert dr.allclose(field.eval_color3(si), texture.eval_3(si))
    assert dr.allclose(field.eval_spec(si), texture.eval(si))

    mono_data = bitmap_data(channels=1)
    mono_texture = mi.load_dict({
        "type": "bitmap",
        "data": mono_data,
        "raw": True,
        "filter_type": "nearest",
        "wrap_mode": "clamp",
    })
    mono_field = mi.load_dict(bitmap_field_dict(channels=1, data=mono_data))
    assert mono_field.out_type() == mi.FieldValueType.Float
    assert mono_field.out_dim() == 1
    assert dr.allclose(mono_field.eval_1(si), mono_texture.eval_1(si))


def test03_grid_field_matches_grid_volume_fixed_and_variable_paths(variant_scalar_rgb):
    data = volume_data(channels=6)
    volume = mi.load_dict({
        "type": "gridvolume",
        "data": data,
        "raw": True,
        "filter_type": "nearest",
        "wrap_mode": "clamp",
    })
    field = mi.load_dict(grid_field_dict(channels=6, data=data))
    it = interaction()

    assert dr.allclose(field.eval_array6(it), volume.eval_6(it))
    assert dr.allclose(field.eval_n(it, 6), volume.eval_n(it))

    data3 = volume_data(channels=3)
    volume3 = mi.load_dict({
        "type": "gridvolume",
        "data": data3,
        "raw": True,
        "filter_type": "nearest",
        "wrap_mode": "clamp",
    })
    field3 = mi.load_dict(grid_field_dict(channels=3, data=data3))
    assert field3.out_type() == mi.FieldValueType.Array3
    assert dr.allclose(field3.eval_array3(it), volume3.eval_3(it))


@pytest.mark.parametrize(
    "field_factory, query, method, pattern",
    [
        (lambda: grid_field_dict(channels=6), interaction, "eval_color3", "Color3|color|6|Features"),
        (lambda: bitmap_field_dict(channels=3), surface_interaction, "eval_array6", "6|out_dim|Color3"),
        (lambda: bitmap_field_dict(channels=3), interaction, "eval_color3", "domain|Interaction|Surface"),
        (lambda: grid_field_dict(channels=1), surface_interaction, "eval_1", "domain|Surface|Interaction"),
    ],
)
def test04_fixed_query_metadata_and_domain_mismatches_are_errors(
    variant_scalar_rgb, field_factory, query, method, pattern
):
    field = mi.load_dict(field_factory())

    with pytest.raises(RuntimeError, match=pattern):
        getattr(field, method)(query())


def test05_eval_n_count_must_match_output_dimension(variant_scalar_rgb):
    field = mi.load_dict(grid_field_dict(channels=6))
    it = interaction()

    assert len(field.eval_n(it, 6)) == 6
    with pytest.raises(RuntimeError, match="count|out_dim|6"):
        field.eval_n(it, 5)
    with pytest.raises(RuntimeError, match="count|out_dim|6"):
        field.eval_n(it, 7)
