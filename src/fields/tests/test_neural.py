import pytest
import drjit as dr
import drjit.nn as nn
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


def sinusoidal_encoding_dict(input_dim=2, out_dim=8):
    return {
        "type": "sinusoidalfield",
        "input_dim": input_dim,
        "out_dim": out_dim,
    }


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
        "type": "gridvolume",
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


@pytest.mark.parametrize("plugin", ["hashgridfield", "permutofield"])
def test01_trainable_encodings_reject_scalar_variants_early(variant_scalar_rgb, plugin):
    config = encoding_dict(plugin)
    if plugin == "permutofield":
        config.pop("hashmap_size")
    with pytest.raises(RuntimeError, match=f"{plugin}|scalar_rgb|LLVM|CUDA|JIT"):
        mi.load_dict(config)


@pytest.mark.parametrize(
    "plugin,encoding_cls",
    [
        ("hashgridfield", nn.HashGridEncoding),
        ("permutofield", nn.PermutoEncoding),
    ],
)
def test02_trainable_encoding_fields_match_drjit_nn(
    field_ad_rgb_variant, plugin, encoding_cls
):
    config = encoding_dict(plugin, seed=7)
    field = mi.load_dict(config)

    direct = encoding_cls(
        mi.TensorXf16,
        dimension=2,
        n_levels=config["n_levels"],
        n_features_per_level=config["n_features_per_level"],
        hashmap_size=config["hashmap_size"],
        base_resolution=config["base_resolution"],
        per_level_scale=config["per_level_scale"],
        rng=dr.rng(seed=config["seed"]),
    )

    si = surface_interaction(width=8)
    assert dr.width(mi.traverse(field)["params"]) == direct.n_params
    assert dr.allclose(
        field.eval_n(si, field.out_dim()),
        mi.ArrayXf(direct((si.uv.x, si.uv.y))),
        atol=1e-6,
    )


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
    assert dr.allclose(field.eval_n(si, 4), [1.0, 0.0, -1.0, 0.0], atol=1e-6)

    field3 = mi.load_dict({
        "type": "sinusoidalfield",
        "input_dim": 3,
        "out_dim": 6,
        "n_frequencies": 1,
        "min_frequency": 1.0,
        "max_frequency": 1.0,
    })
    assert dr.allclose(
        field3.eval_n(si, 6),
        [1.0, 0.0, 0.0, -1.0, -1.0, 0.0],
        atol=1e-6,
    )


@pytest.mark.parametrize("bad_args", [[1.0, 2.0, 3.0], [1.0] * 5])
def test04_direct_field_args_are_validated_in_python_bindings(
    field_ad_rgb_variant, bad_args
):
    field = mi.load_dict(neural_field_dict(args_dim=4))
    si = surface_interaction()

    valid = [0.1, 0.2, 0.3, 0.4]
    value_from_list = field.eval_color3(si, args=valid)
    value_from_array = field.eval_color3(si, args=mi.ArrayXf(valid))
    assert dr.allclose(value_from_list, value_from_array)

    with pytest.raises(RuntimeError, match="args_dim|4"):
        field.eval_color3(si, args=bad_args)


def test05_zero_args_field_accepts_no_args_without_allocating_argument_storage(
    field_ad_rgb_variant
):
    field = mi.load_dict(neural_field_dict(args_dim=0))
    si = surface_interaction()

    assert dr.allclose(field.eval_color3(si), field.eval_color3(si, args=[]))
    with pytest.raises(RuntimeError, match="args_dim|0"):
        field.eval_color3(si, args=[1.0])


def test05b_neural_field_string_parameters_accept_lowercase(field_ad_rgb_variant):
    field = mi.load_dict(neural_field_dict(
        domain="surface",
        out_type="color3",
        args_dim=0,
    ))

    assert field.domain() == mi.FieldDomain.Surface
    assert field.out_type() == mi.FieldValueType.Color3


def test06_single_scalar_arg_matches_one_element_sequence(field_ad_rgb_variant):
    field = mi.load_dict(
        neural_field_dict(
            out_type="Float",
            out_dim=1,
            args_dim=1,
            encoding=sinusoidal_encoding_dict(),
        )
    )
    si = surface_interaction(width=8)

    assert dr.allclose(field.eval_1(si, args=0.25),
                       field.eval_1(si, args=[0.25]))

    vector_arg = dr.linspace(mi.Float, 0.1, 0.8, 8)
    assert dr.allclose(field.eval_1(si, args=vector_arg),
                       field.eval_1(si, args=[vector_arg]))


def test07_neural_fields_reject_negative_args_dim(field_ad_rgb_variant):
    with pytest.raises(RuntimeError, match="args_dim must be non-negative"):
        mi.load_dict(neural_field_dict(args_dim=-1))


def test07b_neural_fields_reject_incompatible_encoding_domain(field_ad_rgb_variant):
    with pytest.raises(RuntimeError, match="encoding|Interaction|domain"):
        mi.load_dict(
            neural_field_dict(
                domain="Interaction",
                out_type="Float",
                out_dim=1,
                args_dim=0,
                encoding=encoding_dict(input_dim=2),
            )
        )


def test08_neural_fields_reject_scalar_variants_early(variant_scalar_rgb):
    with pytest.raises(RuntimeError, match="neuralfield|scalar_rgb|LLVM|CUDA|JIT"):
        mi.load_dict(neural_field_dict(encoding=sinusoidal_encoding_dict()))


@pytest.mark.parametrize(
    "field_factory, pattern",
    [
        (
            lambda: encoding_dict(
                "hashgridfield",
                encoding=encoding_dict("hashgridfield"),
            ),
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
def test09_invalid_encoding_nesting_and_output_metadata_are_rejected(
    field_ad_rgb_variant, field_factory, pattern
):
    with pytest.raises(RuntimeError, match=pattern):
        mi.load_dict(field_factory())


@pytest.mark.parametrize(
    "key,value",
    [
        ("n_levels", 0),
        ("n_features_per_level", 0),
        ("hashmap_size", 0),
        ("base_resolution", 0),
        ("per_level_scale", 0.0),
    ],
)
def test09b_trainable_encoding_parameters_are_validated(
    field_ad_rgb_variant, key, value
):
    with pytest.raises(RuntimeError, match=f"{key}.*positive"):
        mi.load_dict(encoding_dict("hashgridfield", **{key: value}))


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
def test10_neural_field_fixed_output_methods_match_metadata(
    field_ad_rgb_variant, out_type, out_dim, method
):
    field = mi.load_dict(
        neural_field_dict(out_type=out_type, out_dim=out_dim, args_dim=0)
    )
    si = surface_interaction(width=8)

    value = getattr(field, method)(si)
    if method == "eval_array6":
        assert len(value) == 6
        assert dr.all(dr.isfinite(mi.ArrayXf(value)))
    else:
        assert dr.all(dr.isfinite(value))

    with pytest.raises(RuntimeError, match="out_type|out_dim|metadata"):
        if method != "eval_color3":
            field.eval_color3(si)
        else:
            field.eval_array3(si)


def test11_neural_field_features6_round_trips_through_base_dispatch(field_ad_rgb_variant):
    field = mi.load_dict(
        neural_field_dict(out_type="Features", out_dim=6, args_dim=0)
    )
    si = surface_interaction(width=8)

    direct = field.eval_array6(si)
    via_base = mi.Field.eval_array6(field, si, args=[])
    assert dr.allclose(via_base, mi.ArrayXf(direct))


def test12_neural_field_without_summary_is_rejected_by_mean_users(field_ad_rgb_variant):
    field = mi.load_dict(neural_field_dict(args_dim=0))

    assert field.is_spatially_varying()
    with pytest.raises(RuntimeError, match="mean"):
        field.mean()

    with pytest.raises(RuntimeError, match="mean"):
        mi.load_dict({
            "type": "roughplastic",
            "diffuse_reflectance": neural_field_dict(args_dim=0),
        })


def test13_neural_field_traversal_and_update_are_preserved(field_ad_rgb_variant):
    field = mi.load_dict(neural_field_dict())
    params = mi.traverse(field)

    assert "network_weights" in params
    assert any("encoding" in key for key in params.keys())
    flags = params.flags("network_weights")
    assert (flags & mi.ParamFlags.NonDifferentiable) == 0
    assert (flags & mi.ParamFlags.ReadOnly) == 0

    si = surface_interaction(width=8)
    args = [0.0, 0.0, 0.0, 0.0]
    before = field.eval_color3(si, args=args)
    dr.eval(before)

    params["network_weights"] = params["network_weights"] + 0.01
    params.update()

    value = field.eval_color3(si, args=args)
    assert dr.all(dr.isfinite(value))
    assert dr.any(dr.abs(value - before) > 1e-6)


def neural_texture_xml(args_dim=0, out_type="Color3", out_dim=3):
    return f"""<texture version="3.0.0" type="neuralfield">
        <string name="domain" value="Surface"/>
        <string name="out_type" value="{out_type}"/>
        <integer name="out_dim" value="{out_dim}"/>
        <integer name="args_dim" value="{args_dim}"/>
        <integer name="hidden_size" value="8"/>
        <integer name="num_layers" value="1"/>
        <field name="encoding" type="sinusoidalfield">
            <integer name="input_dim" value="2"/>
            <integer name="out_dim" value="8"/>
        </field>
    </texture>"""


def neural_volume_xml():
    return """<volume version="3.0.0" type="neuralfield">
        <string name="domain" value="Interaction"/>
        <string name="out_type" value="Float"/>
        <integer name="out_dim" value="1"/>
        <integer name="args_dim" value="0"/>
        <integer name="hidden_size" value="8"/>
        <integer name="num_layers" value="1"/>
        <field name="encoding" type="sinusoidalfield">
            <integer name="input_dim" value="3"/>
            <integer name="out_dim" value="8"/>
        </field>
    </volume>"""


def test14_neural_fields_in_texture_roles_are_validated(field_ad_rgb_variant):
    texture = mi.load_string(neural_texture_xml())
    si = surface_interaction(width=4)

    assert isinstance(texture, mi.Field)
    assert texture.field() is texture
    assert texture.args_dim() == 0
    assert texture.out_type() == mi.FieldValueType.Color3
    assert dr.all(dr.isfinite(texture.eval_3(si)))

    with pytest.raises(RuntimeError, match="Texture role|args_dim=0|got 1"):
        mi.load_string(neural_texture_xml(args_dim=1))

    with pytest.raises(RuntimeError, match="Texture role|Features\\[8\\]"):
        mi.load_string(neural_texture_xml(out_type="Features", out_dim=8))


def test15_neural_fields_without_volume_metadata_are_rejected(field_ad_rgb_variant):
    with pytest.raises(RuntimeError, match="Volume role|VolumeField|metadata"):
        mi.load_string(neural_volume_xml())


def test16_neural_field_eval_rejects_color_outputs_in_spectral_variants(
    variants_all_ad_spectral
):
    field = mi.load_dict(
        neural_field_dict(
            out_type="Color3",
            out_dim=3,
            args_dim=0,
            encoding=sinusoidal_encoding_dict(),
        )
    )
    si = surface_interaction(width=4)

    assert dr.all(dr.isfinite(field.eval_color3(si)))
    with pytest.raises(RuntimeError, match="cannot convert Color3"):
        field.eval(si)
