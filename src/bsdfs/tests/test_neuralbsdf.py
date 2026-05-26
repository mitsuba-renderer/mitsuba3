import math

import pytest
import drjit as dr
import mitsuba as mi


def albedo_field(value=(0.2, 0.4, 0.6)):
    return {
        "type": "bitmap",
        "data": mi.TensorXf(value, shape=(1, 1, 3)),
        "raw": True,
        "filter_type": "nearest",
        "wrap_mode": "clamp",
    }


def features6_field():
    class Features6Field(mi.Field):
        def __init__(self):
            super().__init__(mi.Properties("features6_field"))

        def out_type(self):
            return mi.FieldValueType.Features

        def domain(self):
            return mi.FieldDomain.Surface

        def out_dim(self):
            return 6

        def args_dim(self):
            return 0

        def supports_scalar(self):
            return True

        def supports_jit(self):
            return True

        def supports_surface_queries(self):
            return True

        def supports_interaction_queries(self):
            return False

    return Features6Field()


def neural_field(args_dim=0):
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


def make_metadata_trap_field(value=(0.25, 0.5, 0.75)):
    class MetadataTrapField(mi.Field):
        def __init__(self):
            mi.Field.__init__(self, mi.Properties("metadata_trap_field"))
            self.fail_metadata = False

        def _check_metadata_allowed(self):
            if self.fail_metadata:
                raise RuntimeError("reflectance metadata was queried during eval")

        def out_type(self):
            self._check_metadata_allowed()
            return mi.FieldValueType.Color3

        def domain(self):
            return mi.FieldDomain.Surface

        def out_dim(self):
            self._check_metadata_allowed()
            return 3

        def args_dim(self):
            return 0

        def supports_scalar(self):
            return True

        def supports_jit(self):
            return True

        def supports_surface_queries(self):
            return True

        def supports_interaction_queries(self):
            return False

        def eval_color3(self, si, args=None, active=True):
            return mi.Color3f(*value) & active

    return MetadataTrapField()


def make_metadata_trap_texture(value=(0.35, 0.45, 0.55)):
    class MetadataTrapTexture(mi.Texture):
        def __init__(self):
            mi.Texture.__init__(self, mi.Properties("metadata_trap_texture"))
            self.fail_metadata = False

        def _check_metadata_allowed(self):
            if self.fail_metadata:
                raise RuntimeError("reflectance metadata was queried during eval")

        def out_type(self):
            self._check_metadata_allowed()
            return mi.FieldValueType.Color3

        def out_dim(self):
            self._check_metadata_allowed()
            return 3

        def eval_3(self, si, active=True):
            return mi.Color3f(*value) & active

    return MetadataTrapTexture()


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

    for theta in dr.linspace(dr.scalar.ArrayXf, 0.0, dr.pi / 2 - 1e-4, 8):
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


def test03_neuralbsdf_requires_argument_free_reflectance_field(variant_cuda_ad_rgb):
    with pytest.raises(RuntimeError, match="reflectance|args_dim|11|0"):
        mi.load_dict(neuralbsdf_dict(neural_field(args_dim=4)))

    with pytest.raises(RuntimeError, match="reflectance|args_dim|11|0"):
        mi.load_dict(neuralbsdf_dict(make_args_reflectance_field()))


def test04_neuralbsdf_traverses_field_children(variant_cuda_ad_rgb):
    bsdf = mi.load_dict(neuralbsdf_dict(neural_field()))
    params = mi.traverse(bsdf)

    assert any("reflectance" in key or "field" in key for key in params.keys())
    assert any("network_weights" in key for key in params.keys())
    assert any("encoding" in key for key in params.keys())

    si = make_si()
    wo = mi.Vector3f(0.2, 0.1, math.sqrt(1.0 - 0.2 * 0.2 - 0.1 * 0.1))
    assert dr.all(dr.isfinite(bsdf.eval(mi.BSDFContext(), si, wo)))


def test05_neuralbsdf_rejects_direction_dependent_reflectance_args(variant_scalar_rgb):
    with pytest.raises(RuntimeError, match="reflectance|args_dim|11|0"):
        mi.load_dict(neuralbsdf_dict(make_args_reflectance_field()))


def test06_neuralbsdf_rejects_unstructured_neural_scattering_mode(variant_cuda_ad_rgb):
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
    neural = mi.load_dict(neuralbsdf_dict(mi.Color3f(*reflectance)))
    diffuse = mi.load_dict({
        "type": "diffuse",
        "reflectance": {"type": "rgb", "value": reflectance},
    })

    si = make_si()
    wo = mi.Vector3f(0.3, 0.2, math.sqrt(1.0 - 0.3 * 0.3 - 0.2 * 0.2))
    ctx = mi.BSDFContext()
    assert dr.allclose(neural.eval(ctx, si, wo), diffuse.eval(ctx, si, wo))


@pytest.mark.parametrize(
    "factory, expected",
    [
        (make_metadata_trap_field, (0.25, 0.5, 0.75)),
        (make_metadata_trap_texture, (0.35, 0.45, 0.55)),
    ],
)
def test08_neuralbsdf_uses_cached_reflectance_dispatch_after_setup(
    variant_scalar_rgb, factory, expected
):
    reflectance = factory()
    bsdf = mi.load_dict(neuralbsdf_dict(reflectance))
    reflectance.fail_metadata = True

    si = make_si()
    wo = mi.Vector3f(0, 0, 1)
    ctx = mi.BSDFContext()

    assert dr.allclose(bsdf.eval_diffuse_reflectance(si), expected)
    assert dr.allclose(bsdf.eval(ctx, si, wo),
                       mi.Color3f(*expected) * dr.inv_pi)


def test09_neuralbsdf_clamps_diffuse_reflectance_to_albedo_range(variant_scalar_rgb):
    bsdf = mi.load_dict(neuralbsdf_dict(albedo_field((1.5, -0.25, 0.5))))
    si = make_si()

    assert dr.allclose(bsdf.eval_diffuse_reflectance(si), [1.0, 0.0, 0.5])
