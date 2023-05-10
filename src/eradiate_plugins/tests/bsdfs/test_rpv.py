import drjit as dr
import mitsuba as mi
import numpy as np
import pytest

from eradiate.test_tools.plugin import sample_eval_pdf_bsdf


def test_create_rpv3(variant_scalar_rgb):
    # Test constructor of 3-parameter version of RPV
    rpv = mi.load_dict({"type": "rpv"})
    assert isinstance(rpv, mi.BSDF)
    assert rpv.component_count() == 1
    assert rpv.flags(0) == mi.BSDFFlags.GlossyReflection | mi.BSDFFlags.FrontSide
    assert rpv.flags() == rpv.flags(0)

    # rho_c is not exposed: it is always equal to rho_0
    params = mi.traverse(rpv)
    assert "rho_c.value" not in params


def rpv_reference(rho_0, rho_0_hotspot, g, k, theta_i, phi_i, theta_o, phi_o):
    """Reference for RPV, adapted from a C implementation."""

    sin_theta_i, cos_theta_i = np.sin(theta_i), np.cos(theta_i)
    tan_theta_i = sin_theta_i / cos_theta_i
    sin_theta_o, cos_theta_o = np.sin(theta_o), np.cos(theta_o)
    tan_theta_o = sin_theta_o / cos_theta_o
    cos_delta_phi = np.cos(phi_i - phi_o)

    K1 = np.power(cos_theta_i * cos_theta_o * (cos_theta_i + cos_theta_o), k - 1.0)

    cos_Theta = cos_theta_i * cos_theta_o + sin_theta_i * sin_theta_o * cos_delta_phi

    FgDenum = 1.0 + g * g + 2.0 * g * cos_Theta
    Fg = (1.0 - g * g) / np.power(FgDenum, 1.5)

    G = np.sqrt(
        tan_theta_i * tan_theta_i
        + tan_theta_o * tan_theta_o
        - 2.0 * tan_theta_i * tan_theta_o * cos_delta_phi
    )
    K3 = 1.0 + (1.0 - rho_0_hotspot) / (1.0 + G)

    return rho_0 * K1 * Fg * K3 / np.pi * np.abs(cos_theta_o)
    # 1/π factor because the paper gives BRF expression (not BRDF)
    # Foreshortening factor included


def angles_to_directions(theta, phi):
    return mi.Vector3f(
        np.sin(theta) * np.cos(phi), np.sin(theta) * np.sin(phi), np.cos(theta)
    )


def eval_bsdf(bsdf, wi, wo):
    si = mi.SurfaceInteraction3f()
    si.wi = wi
    ctx = mi.BSDFContext()
    return bsdf.eval(ctx, si, wo, True)[0]


@pytest.mark.parametrize("rho_0", [0.004, 0.1, 0.497])
@pytest.mark.parametrize("k", [0.543, 0.634, 0.851])
@pytest.mark.parametrize("g", [-0.29, 0.086, 0.2])
def test_eval(variant_llvm_ad_rgb, rho_0, k, g):
    """
    Test the eval method of the RPV plugin, comparing to a reference implementation.
    """

    rpv = mi.load_dict({"type": "rpv", "k": k, "rho_0": rho_0, "g": g})
    num_samples = 100

    theta_i = np.random.rand(num_samples) * np.pi / 2.0
    theta_o = np.random.rand(num_samples) * np.pi / 2.0
    phi_i = np.random.rand(num_samples) * np.pi * 2.0
    phi_o = np.random.rand(num_samples) * np.pi * 2.0

    wi = angles_to_directions(theta_i, phi_i)
    wo = angles_to_directions(theta_o, phi_o)
    values = eval_bsdf(rpv, wi, wo)
    reference = rpv_reference(rho_0, rho_0, g, k, theta_i, phi_i, theta_o, phi_o)

    assert dr.allclose(values, reference, rtol=1e-3, atol=1e-3)


@pytest.mark.parametrize("rho_0", [0.0, 0.5, 1.0])
def test_eval_diffuse(variant_llvm_ad_rgb, rho_0):
    """
    Compare a degenerate RPV case with a diffuse BRDF.
    """
    k = 1.0
    g = 0.0
    rho_c = 1.0

    rpv = mi.load_dict({"type": "rpv", "rho_0": rho_0, "k": k, "g": g, "rho_c": rho_c})
    diffuse = mi.load_dict({"type": "diffuse", "reflectance": rho_0})

    num_samples = 100

    theta_i = np.random.rand(num_samples) * np.pi / 2.0
    theta_o = np.random.rand(num_samples) * np.pi / 2.0
    phi_i = np.random.rand(num_samples) * np.pi * 2.0
    phi_o = np.random.rand(num_samples) * np.pi * 2.0

    wi = angles_to_directions(theta_i, phi_i)
    wo = angles_to_directions(theta_o, phi_o)
    values = eval_bsdf(rpv, wi, wo)
    reference = eval_bsdf(diffuse, wi, wo)

    assert np.allclose(values, reference)


def test_chi2_rpv3(variant_llvm_ad_rgb):
    from mitsuba.chi2 import BSDFAdapter, ChiSquareTest, SphericalDomain

    sample_func, pdf_func = BSDFAdapter("rpv", "")

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
    )

    assert chi2.run()


def test_chi2_rpv4(variant_llvm_ad_rgb):
    from mitsuba.chi2 import BSDFAdapter, ChiSquareTest, SphericalDomain

    sample_func, pdf_func = BSDFAdapter(
        "rpv",
        """
            <float name="rho_0" value="0.1"/>
            <float name="g" value="0.497"/>
            <float name="k" value="0.004"/>
            <float name="rho_c" value="1.05"/>
        """,
    )

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
    )

    result = chi2.run()
    assert result


@pytest.mark.parametrize("wi", [[0, 0, 1], [0, 1, 1], [1, 1, 1]])
def test_sampling_weights_rpv(variant_llvm_ad_rgb, wi):
    """
    Sampling weights are correctly computed, i.e. equal to eval() / pdf().
    """
    rpv = mi.load_dict({"type": "rpv"})
    (_, weight), eval, pdf = sample_eval_pdf_bsdf(
        rpv, dr.normalize(mi.ScalarVector3f(wi))
    )
    assert dr.allclose(weight, eval / pdf)
