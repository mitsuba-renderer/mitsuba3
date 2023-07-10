import drjit as dr
import mitsuba as mi
import numpy as np
import pytest

#from ..tools import sample_eval_pdf_bsdf


def test_create_rtls(variant_scalar_rgb):
    # Test constructor of 3-parameter version of RPV
    rtls = mi.load_dict({"type": "rtls"})

    assert isinstance(rtls, mi.BSDF)
    assert rtls.component_count() == 1
    assert rtls.flags(0) == mi.BSDFFlags.GlossyReflection | mi.BSDFFlags.FrontSide
    assert rtls.flags() == rtls.flags(0)

    params = mi.traverse(rtls)
    assert "f_iso.value" in params
    assert "f_geo.value" in params
    assert "f_vol.value" in params


def angles_to_directions(theta, phi):
    return mi.Vector3f(
        np.sin(theta) * np.cos(phi), np.sin(theta) * np.sin(phi), np.cos(theta)
    )


def eval_bsdf(bsdf, wi, wo):
    si = mi.SurfaceInteraction3f()
    si.wi = wi
    ctx = mi.BSDFContext()
    return bsdf.eval(ctx, si, wo, True)[0]

def test_defaults_and_print(variant_llvm_ad_rgb):
    rtls = mi.load_dict({"type": "rtls"})
    value = str(rtls)
    print(value)


@pytest.mark.parametrize("f_iso", [0.004, 0.1, 0.497])
@pytest.mark.parametrize("f_geo", [0.543, 0.634, 0.851])
@pytest.mark.parametrize("f_vol", [0.29, 0.086, 0.2])
def test_rtls(variant_llvm_ad_rgb, f_iso, f_geo, f_vol):

    rtls = mi.load_dict({"type": "rtls", "f_iso": f_iso, "f_geo": f_geo, "f_vol": f_vol})
    num_samples = 100

    theta_i = np.random.rand(num_samples) * np.pi / 2.0
    theta_o = np.random.rand(num_samples) * np.pi / 2.0
    phi_i = np.random.rand(num_samples) * np.pi * 2.0
    phi_o = np.random.rand(num_samples) * np.pi * 2.0

    wi = angles_to_directions(theta_i, phi_i)
    wo = angles_to_directions(theta_o, phi_o)
    values = eval_bsdf(rtls, wi, wo)

    raise RuntimeError(values)
