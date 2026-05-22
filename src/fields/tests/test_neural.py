import pytest
import drjit as dr
import mitsuba as mi


def encoding_dict(plugin="hashgridfield", input_dim=2, out_dim=8, **kwargs):
    result = {
        "type": plugin,
        "input_dim": input_dim,
        "out_dim": out_dim,
        "n_levels": 4,
        "n_features_per_level": 2,
        "base_resolution": 4,
        "per_level_scale": 1.5,
        "hashmap_size": 128,
    }
    result.update(kwargs)
    return result


def neural_field_dict(out_type="Color3", out_dim=3, args_dim=4, **kwargs):
    result = {
        "type": "neuralfield",
        "domain": "Surface",
        "out_type": out_type,
        "out_dim": out_dim,
        "args_dim": args_dim,
        "encoding": encoding_dict(),
        "hidden_size": 16,
        "num_layers": 2,
    }
    result.update(kwargs)
    return result


def grid_field_dict(channels=3):
    return {
        "type": "gridfield",
        "data": dr.ones(mi.TensorXf, shape=[2, 2, 2, channels]),
        "raw": True,
        "filter_type": "nearest",
        "wrap_mode": "clamp",
    }


def surface_interaction(width=1):
    si = dr.zeros(mi.SurfaceInteraction3f, width)
    si.p = mi.Point3f(0.25, 0.5, 0.75)
    si.uv = mi.Point2f(0.25, 0.75)
    si.n = mi.Normal3f(0, 0, 1)
    si.wi = mi.Vector3f(0, 0, 1)
    si.sh_frame = mi.Frame3f(si.n)
    return si


def test01_encoding_and_neural_plugins_are_registered_as_fields(variant_scalar_rgb):
    pmgr = mi.PluginManager.instance()
    for name in [
        "hashgridfield",
        "permutofield",
        "sinusoidalfield",
        "neuralfield",
    ]:
        assert pmgr.plugin_type(name) == mi.ObjectType.Field

    for name in [
        "hashgridencoding",
        "permutoencoding",
        "sinusoidalencoding",
    ]:
        assert pmgr.plugin_type(name) == mi.ObjectType.Unknown


@pytest.mark.parametrize("plugin", ["hashgridfield", "permutofield"])
def test02_trainable_encodings_reject_scalar_variants_early(variant_scalar_rgb, plugin):
    config = encoding_dict(plugin)
    if plugin == "permutofield":
        config.pop("hashmap_size")
    with pytest.raises(RuntimeError, match=f"{plugin}|scalar_rgb|LLVM|CUDA|JIT"):
        mi.load_dict(config)


def test03_sinusoidal_field_uses_per_coordinate_sincos_features(variant_scalar_rgb):
    field = mi.load_dict({
        "type": "sinusoidalfield",
        "input_dim": 2,
        "out_dim": 4,
        "n_frequencies": 1,
        "min_frequency": 1.0,
        "max_frequency": 1.0,
    })
    si = surface_interaction()
    assert dr.allclose(field.eval(si), [1.0, 0.0, -1.0, 0.0], atol=1e-6)


@pytest.mark.parametrize("bad_args", [[1.0, 2.0, 3.0], [1.0] * 5])
def test04_direct_field_args_are_validated_in_python_bindings(field_ad_rgb_variant, bad_args):
    field = mi.load_dict(neural_field_dict(args_dim=4))
    si = surface_interaction()

    valid = [0.1, 0.2, 0.3, 0.4]
    value_from_list = field.eval_color3(si, args=valid)
    value_from_array = field.eval_color3(si, args=mi.ArrayXf(valid))
    assert dr.allclose(value_from_list, value_from_array)

    with pytest.raises(RuntimeError, match="args_dim|4"):
        field.eval_color3(si, args=bad_args)


def test05_zero_args_field_accepts_no_args_without_allocating_argument_storage(field_ad_rgb_variant):
    field = mi.load_dict(neural_field_dict(args_dim=0))
    si = surface_interaction()

    assert dr.allclose(field.eval_color3(si), field.eval_color3(si, args=[]))
    with pytest.raises(RuntimeError, match="args_dim|0"):
        field.eval_color3(si, args=[1.0])


def test06_neural_fields_reject_scalar_variants_early(variant_scalar_rgb):
    with pytest.raises(RuntimeError, match="neuralfield|scalar_rgb|LLVM|CUDA|JIT"):
        mi.load_dict(neural_field_dict(encoding={
            "type": "sinusoidalfield",
            "input_dim": 2,
            "out_dim": 8,
        }))


@pytest.mark.parametrize(
    "field_factory, pattern",
    [
        (
            lambda: encoding_dict("hashgridfield", encoding=encoding_dict("hashgridfield")),
            "encoding.*child|nested|compose",
        ),
        (
            lambda: neural_field_dict(encoding=grid_field_dict(channels=3)),
            "encoding|Features|field",
        ),
        (
            lambda: neural_field_dict(out_type="Features", out_dim=0),
            "out_dim|Features|positive",
        ),
    ],
)
def test07_invalid_encoding_nesting_and_output_metadata_are_rejected(
    field_ad_rgb_variant, field_factory, pattern
):
    with pytest.raises(RuntimeError, match=pattern):
        mi.load_dict(field_factory())


@pytest.mark.parametrize(
    "out_type,out_dim,method",
    [
        ("Float", 1, "eval_1"),
        ("Color3", 3, "eval_color3"),
        ("Array2", 2, "eval_array2"),
        ("Array3", 3, "eval_array3"),
        ("Features", 6, "eval_array6"),
    ],
)
def test08_neural_field_fixed_output_methods_match_metadata(field_ad_rgb_variant, out_type, out_dim, method):
    field = mi.load_dict(neural_field_dict(out_type=out_type, out_dim=out_dim, args_dim=0))
    si = surface_interaction(width=8)

    value = getattr(field, method)(si)
    assert dr.all(dr.isfinite(value))

    with pytest.raises(RuntimeError, match="out_type|out_dim|metadata"):
        field.eval_color3(si) if method != "eval_color3" else field.eval_array3(si)


def test09_neural_field_traversal_and_update_are_preserved(field_ad_rgb_variant):
    field = mi.load_dict(neural_field_dict())
    params = mi.traverse(field)

    assert "network_weights" in params
    assert any("encoding" in key for key in params.keys())
    assert params.flags("network_weights") & mi.ParamFlags.Differentiable

    params["network_weights"] = params["network_weights"] + 0.01
    params.update()

    value = field.eval_color3(surface_interaction(width=8),
                              args=[0.0, 0.0, 0.0, 0.0])
    assert dr.all(dr.isfinite(value))
