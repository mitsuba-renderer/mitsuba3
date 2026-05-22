import math

import pytest
import drjit as dr
import mitsuba as mi


def albedo_field(value=(0.2, 0.4, 0.6)):
    return {
        "type": "bitmapfield",
        "data": mi.TensorXf(value, shape=(1, 1, 3)),
        "raw": True,
        "filter_type": "nearest",
        "wrap_mode": "clamp",
    }


def features6_field():
    return {
        "type": "gridfield",
        "data": dr.ones(mi.TensorXf, shape=[1, 1, 1, 6]),
        "raw": True,
    }


def neural_field(args_dim=11):
    return {
        "type": "neuralfield",
        "domain": "Surface",
        "out_type": "Color3",
        "out_dim": 3,
        "args_dim": args_dim,
        "encoding": {
            "type": "hashgridfield",
            "input_dim": 2,
            "out_dim": 8,
            "n_levels": 4,
            "n_features_per_level": 2,
            "base_resolution": 4,
            "per_level_scale": 1.5,
            "hashmap_size": 128,
        },
        "hidden_size": 16,
        "num_layers": 2,
    }


def neuralbsdf_dict(field):
    return {
        "type": "neuralbsdf",
        "reflectance": field,
    }


def make_si():
    si = mi.SurfaceInteraction3f()
    si.p = mi.Point3f(0.25, 0.5, 0.75)
    si.uv = mi.Point2f(0.3, 0.7)
    si.n = mi.Normal3f(0, 0, 1)
    si.wi = mi.Vector3f(0, 0, 1)
    si.sh_frame = mi.Frame3f(si.n)
    return si


def make_args_reflectance_field():
    class ArgsReflectanceField(mi.Field):
        def __init__(self):
            super().__init__(mi.Properties("args_reflectance_field"))

        def out_type(self):
            return mi.FieldValueType.Color3

        def domain(self):
            return mi.FieldDomain.Surface

        def out_dim(self):
            return 3

        def args_dim(self):
            return 11

        def supports_scalar(self):
            return True

        def supports_jit(self):
            return True

        def supports_surface_queries(self):
            return True

        def supports_interaction_queries(self):
            return False

        def eval_color3(self, si, args=None, active=True):
            assert args is not None
            return mi.Color3f(
                0.1 + 0.1 * args[0] + 0.2 * args[3] + 0.3 * args[8],
                0.1 + 0.1 * args[1] + 0.2 * args[4] + 0.3 * args[9],
                0.1 + 0.1 * args[2] + 0.2 * args[7] + 0.3 * args[10],
            )

    return ArgsReflectanceField()


def expected_args_reflectance(si, wo):
    return (
        0.1 + 0.1 * float(si.p.x) + 0.2 * float(si.uv.x) + 0.3 * float(wo.x),
        0.1 + 0.1 * float(si.p.y) + 0.2 * float(si.uv.y) + 0.3 * float(wo.y),
        0.1 + 0.1 * float(si.p.z) + 0.2 * float(si.wi.z) + 0.3 * float(wo.z),
    )


def test01_constant_field_neuralbsdf_matches_diffuse_eval_pdf_sample_and_reflectance(variant_scalar_rgb):
    reflectance = (0.2, 0.4, 0.6)
    neural = mi.load_dict(neuralbsdf_dict(albedo_field(reflectance)))
    diffuse = mi.load_dict({
        "type": "diffuse",
        "reflectance": {"type": "rgb", "value": reflectance},
    })

    si = make_si()
    ctx = mi.BSDFContext()

    assert neural.flags() == diffuse.flags()
    assert neural.component_count() == diffuse.component_count()
    assert dr.allclose(neural.eval_diffuse_reflectance(si),
                       diffuse.eval_diffuse_reflectance(si))

    for theta in dr.linspace(mi.Float, 0.0, dr.pi / 2 - 1e-4, 8):
        wo = mi.Vector3f(dr.sin(theta), 0, dr.cos(theta))
        assert dr.allclose(neural.eval(ctx, si, wo), diffuse.eval(ctx, si, wo))
        assert dr.allclose(neural.pdf(ctx, si, wo), diffuse.pdf(ctx, si, wo))
        assert dr.allclose(neural.eval_pdf(ctx, si, wo)[0], diffuse.eval_pdf(ctx, si, wo)[0])
        assert dr.allclose(neural.eval_pdf(ctx, si, wo)[1], diffuse.eval_pdf(ctx, si, wo)[1])

    wo_back = mi.Vector3f(0, 0, -1)
    assert dr.allclose(neural.eval(ctx, si, wo_back), 0)
    assert dr.allclose(neural.pdf(ctx, si, wo_back), 0)

    sample1 = mi.Float(0.37)
    sample2 = mi.Point2f(0.41, 0.73)
    neural_sample, neural_weight = neural.sample(ctx, si, sample1, sample2)
    diffuse_sample, diffuse_weight = diffuse.sample(ctx, si, sample1, sample2)
    assert dr.allclose(neural_sample.wo, diffuse_sample.wo)
    assert dr.allclose(neural_sample.pdf, diffuse_sample.pdf)
    assert neural_sample.sampled_type == diffuse_sample.sampled_type
    assert dr.allclose(neural_weight, diffuse_weight)


def test02_neuralbsdf_rejects_feature6_field_for_reflectance(variant_scalar_rgb):
    with pytest.raises(RuntimeError, match="reflectance|Float|Color3|Spectrum|Features|6"):
        mi.load_dict(neuralbsdf_dict(features6_field()))


def test03_neuralbsdf_requires_zero_or_bsdf_context_argument_dimension(variant_llvm_ad_rgb):
    with pytest.raises(RuntimeError, match="reflectance|args_dim|11|0"):
        mi.load_dict(neuralbsdf_dict(neural_field(args_dim=4)))


def test04_neuralbsdf_traverses_field_children(variant_llvm_ad_rgb):
    bsdf = mi.load_dict(neuralbsdf_dict(neural_field(args_dim=11)))
    params = mi.traverse(bsdf)

    assert any("reflectance" in key or "field" in key for key in params.keys())
    assert any("network_weights" in key for key in params.keys())
    assert any("encoding" in key for key in params.keys())


def test05_neuralbsdf_passes_surface_and_direction_context_through_field_args(variant_scalar_rgb):
    si = make_si()
    wo = mi.Vector3f(0.2, 0.3, math.sqrt(1.0 - 0.2 * 0.2 - 0.3 * 0.3))
    reflectance = expected_args_reflectance(si, wo)

    neural = mi.load_dict(neuralbsdf_dict(make_args_reflectance_field()))
    diffuse = mi.load_dict({
        "type": "diffuse",
        "reflectance": {"type": "rgb", "value": reflectance},
    })
    ctx = mi.BSDFContext()

    neural_eval, neural_pdf = neural.eval_pdf(ctx, si, wo)
    diffuse_eval, diffuse_pdf = diffuse.eval_pdf(ctx, si, wo)

    assert dr.allclose(neural.eval(ctx, si, wo), diffuse.eval(ctx, si, wo))
    assert dr.allclose(neural_eval, diffuse_eval)
    assert dr.allclose(neural_pdf, diffuse_pdf)


def test06_neuralbsdf_rejects_unstructured_neural_scattering_mode(variant_llvm_ad_rgb):
    with pytest.raises(RuntimeError, match="sample|pdf|structured|diffuse"):
        mi.load_dict({
            "type": "neuralbsdf",
            "mode": "arbitrary_value",
            "value": {
                "type": "neuralfield",
                "domain": "Surface",
                "out_type": "Features",
                "out_dim": 3,
                "args_dim": 11,
            },
        })


def test07_neuralbsdf_matches_diffuse_in_polarized_variants(variants_all_spectral):
    if "polarized" not in variants_all_spectral:
        pytest.skip("polarized spectral variant not selected")

    reflectance = (0.2, 0.4, 0.6)
    neural = mi.load_dict(neuralbsdf_dict(albedo_field(reflectance)))
    diffuse = mi.load_dict({
        "type": "diffuse",
        "reflectance": {"type": "rgb", "value": reflectance},
    })

    si = make_si()
    wo = mi.Vector3f(0.3, 0.2, math.sqrt(1.0 - 0.3 * 0.3 - 0.2 * 0.2))
    ctx = mi.BSDFContext()
    assert dr.allclose(neural.eval(ctx, si, wo), diffuse.eval(ctx, si, wo))
