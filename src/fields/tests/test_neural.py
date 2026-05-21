import pytest
import drjit as dr
import mitsuba as mi


def encoding_dict(plugin="hashgridencoding", input_dim=2, out_dim=8, **kwargs):
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


def surface_interaction():
    si = dr.zeros(mi.SurfaceInteraction3f)
    si.p = mi.Point3f(0.25, 0.5, 0.75)
    si.uv = mi.Point2f(0.25, 0.75)
    si.n = mi.Normal3f(0, 0, 1)
    si.wi = mi.Vector3f(0, 0, 1)
    si.sh_frame = mi.Frame3f(si.n)
    return si


@pytest.mark.parametrize("bad_args", [[1.0, 2.0, 3.0], [1.0] * 5])
def test01_direct_field_args_are_validated_in_python_bindings(variant_llvm_ad_rgb, bad_args):
    field = mi.load_dict(neural_field_dict(args_dim=4))
    si = surface_interaction()

    valid = [0.1, 0.2, 0.3, 0.4]
    value_from_list = field.eval_color3(si, args=valid)
    value_from_array = field.eval_color3(si, args=mi.ArrayXf(valid))
    assert dr.allclose(value_from_list, value_from_array)

    with pytest.raises(RuntimeError, match="args_dim|4"):
        field.eval_color3(si, args=bad_args)


def test02_neural_fields_reject_scalar_variants_early(variant_scalar_rgb):
    with pytest.raises(RuntimeError, match="neuralfield|scalar_rgb|LLVM|CUDA|JIT"):
        mi.load_dict(neural_field_dict())


@pytest.mark.parametrize(
    "field_factory, pattern",
    [
        (
            lambda: encoding_dict("hashgridencoding", encoding=encoding_dict("sinusoidalencoding")),
            "encoding.*child|nested|compose",
        ),
        (
            lambda: neural_field_dict(encoding=grid_field_dict(channels=3)),
            "encoding|Features|field",
        ),
        (
            lambda: neural_field_dict(decoder="linear", encoding=encoding_dict("sinusoidalencoding")),
            "linear|hashgrid|permuto",
        ),
    ],
)
def test03_invalid_encoding_nesting_and_decoder_combinations_are_rejected(
    variant_llvm_ad_rgb, field_factory, pattern
):
    with pytest.raises(RuntimeError, match=pattern):
        mi.load_dict(field_factory())


def test04_neural_field_traversal_exposes_trainable_state(variant_llvm_ad_rgb):
    field = mi.load_dict(neural_field_dict())
    params = mi.traverse(field)

    assert "network_weights" in params
    assert any("encoding" in key for key in params.keys())
    assert params.flags("network_weights") & mi.ParamFlags.Differentiable

    si = surface_interaction()
    loss = dr.sum(field.eval_color3(si, args=[0.0, 0.0, 0.0, 0.0]))
    dr.enable_grad(params["network_weights"])
    dr.backward(loss)
    assert dr.any(dr.isfinite(dr.grad(params["network_weights"])))
