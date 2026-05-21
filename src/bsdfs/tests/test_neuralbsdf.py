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


def test01_constant_field_neuralbsdf_matches_diffuse_eval_pdf_and_sample(variant_scalar_rgb):
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

    for theta in dr.linspace(mi.Float, 0.0, dr.pi / 2 - 1e-4, 8):
        wo = mi.Vector3f(dr.sin(theta), 0, dr.cos(theta))
        assert dr.allclose(neural.eval(ctx, si, wo), diffuse.eval(ctx, si, wo))
        assert dr.allclose(neural.pdf(ctx, si, wo), diffuse.pdf(ctx, si, wo))
        assert dr.allclose(neural.eval_pdf(ctx, si, wo)[0], diffuse.eval_pdf(ctx, si, wo)[0])
        assert dr.allclose(neural.eval_pdf(ctx, si, wo)[1], diffuse.eval_pdf(ctx, si, wo)[1])

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


def test03_neuralbsdf_traverses_field_children(variant_llvm_ad_rgb):
    bsdf = mi.load_dict(neuralbsdf_dict({
        "type": "neuralfield",
        "domain": "Surface",
        "out_type": "Color3",
        "out_dim": 3,
        "args_dim": 4,
        "encoding": {
            "type": "hashgridencoding",
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
    }))
    params = mi.traverse(bsdf)

    assert any("reflectance" in key or "field" in key for key in params.keys())
    assert any("network_weights" in key for key in params.keys())
    assert any("encoding" in key for key in params.keys())


def test04_neuralbsdf_rejects_unstructured_neural_scattering_mode(variant_llvm_ad_rgb):
    with pytest.raises(RuntimeError, match="sample|pdf|structured|diffuse"):
        mi.load_dict({
            "type": "neuralbsdf",
            "mode": "arbitrary_value",
            "value": {
                "type": "neuralfield",
                "domain": "Surface",
                "out_type": "Features",
                "out_dim": 3,
                "args_dim": 4,
            },
        })
