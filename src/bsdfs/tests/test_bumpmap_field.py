import pytest
import drjit as dr
import mitsuba as mi


def make_si():
    si = dr.zeros(mi.SurfaceInteraction3f)
    si.p = mi.Point3f(0, 0, 0)
    si.uv = mi.Point2f(0.3, 0.7)
    si.n = mi.Normal3f(0, 0, 1)
    si.sh_frame = mi.Frame3f(si.n)
    si.dp_du = mi.Vector3f(1, 0, 0)
    si.dp_dv = mi.Vector3f(0, 1, 0)
    si.wi = mi.Vector3f(0, 0, 1)
    return si


def load_uniform_bumpmap():
    return mi.load_dict({
        "type": "bumpmap",
        "height": {
            "type": "uniform",
            "value": 0.5,
        },
        "bsdf": {
            "type": "diffuse",
            "reflectance": 0.6,
        },
    })


def assert_uniform_bumpmap_matches_diffuse():
    bsdf = load_uniform_bumpmap()
    reference = mi.load_dict({
        "type": "diffuse",
        "reflectance": 0.6,
    })
    si = make_si()
    ctx = mi.BSDFContext()
    wo = mi.Vector3f(0, 0, 1)

    assert dr.allclose(bsdf.eval(ctx, si, wo),
                       reference.eval(ctx, si, wo), atol=1e-6)


def test01_bumpmap_accepts_uniform_scalar_texture_in_scalar_rgb(variant_scalar_rgb):
    assert_uniform_bumpmap_matches_diffuse()


def test02_bumpmap_accepts_uniform_scalar_texture_in_cuda_ad_rgb(variant_cuda_ad_rgb):
    assert_uniform_bumpmap_matches_diffuse()


def test03_bumpmap_rejects_direct_fields_without_gradient_support(variant_scalar_rgb):
    with pytest.raises(RuntimeError, match="BumpMap requires a SurfaceField|Texture role"):
        mi.load_dict({
            "type": "bumpmap",
            "height": {
                "type": "sinusoidalfield",
                "input_dim": 2,
                "out_dim": 8,
            },
            "bsdf": {
                "type": "diffuse",
            },
        })
