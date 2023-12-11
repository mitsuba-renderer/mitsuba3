import drjit as dr
import mitsuba as mi
import numpy as np
import pytest

from ..tools import sample_eval_pdf_bsdf


def rtls_reference(
    f_iso, f_vol, f_geo, theta_i, phi_i, theta_o, phi_o, h=2.0, r=1.0, b=1.0
):
    #
    # Ross-Thick, Li-Sparse BRDF
    #
    # Model from:
    # MODIS BRDF/Albedo Product: Algorithm Theoretical Basis Document Version 5.0
    # MODIS Product ID: MOD43
    # (Strahler et al, 1999)
    #
    # Implementation by Christian Lanconelli and Fabrizio Cappucci, JRC
    #

    phi = phi_i - phi_o

    cos_phi = np.cos(phi)
    sin_phi = np.sin(phi)
    cos_sun = np.cos(theta_i)
    cos_view = np.cos(theta_o)
    sin_sun = np.sin(theta_i)
    sin_view = np.sin(theta_o)

    cos_psi = cos_sun * cos_view + sin_sun * sin_view * cos_phi
    sin_psi = np.sqrt(1.0 - cos_psi * cos_psi)
    psi = np.arccos(cos_psi)

    thv_p = np.arctan(b / r * sin_view / cos_view)
    ths_p = np.arctan(b / r * sin_sun / cos_sun)
    phi_p = np.arctan(b / r * sin_phi / cos_phi)

    sec_thv_p = 1.0 / np.cos(thv_p)
    sec_ths_p = 1.0 / np.cos(ths_p)

    big_d = np.sqrt(
        np.square(np.tan(ths_p))
        + np.square(np.tan(thv_p))
        - 2.0 * np.tan(ths_p) * np.tan(thv_p) * cos_phi
    )

    cos_t = (
        h
        / b
        * np.sqrt(np.square(big_d) + np.square(np.tan(thv_p) * np.tan(ths_p) * sin_phi))
        / (sec_ths_p + sec_thv_p)
    )

    # Constrain cos_t to [-1, 1]
    cos_t = np.array(cos_t)
    if np.count_nonzero(cos_t < -1):
        cos_t[cos_t < -1] = -1
    if np.count_nonzero(cos_t > 1):
        cos_t[cos_t > 1] = 1

    t = np.arccos(cos_t)
    sin_t = np.sqrt(1.0 - cos_t * cos_t)
    big_o = 1.0 / np.pi * (t - sin_t * cos_t) * (sec_thv_p + sec_ths_p)

    cos_psi_p = (np.cos(ths_p) * np.cos(thv_p)) + (
        np.sin(ths_p) * np.sin(thv_p) * cos_phi
    )

    k_tick = (
        ((np.pi / 2.0 - psi) * cos_psi + sin_psi) / (cos_sun + cos_view)
    ) - np.pi / 4.0
    k_sparse = (
        big_o - sec_ths_p - sec_thv_p + 0.5 * (1 + cos_psi_p) * sec_thv_p * sec_ths_p
    )

    brdf = (f_iso + f_geo * k_sparse + f_vol * k_tick) / np.pi

    return brdf, k_tick, k_sparse


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


def test_defaults_and_print(variant_scalar_mono):
    rtls = mi.load_dict({"type": "rtls"})
    value = str(rtls)
    reference = "\n".join(
        [
            "RTLSBSDF[",
            "  f_iso = UniformSpectrum[value=0.209741],",
            "  f_vol = UniformSpectrum[value=0.081384],",
            "  f_geo = UniformSpectrum[value=0.004140],",
            "  h = 2,",
            "  r = 1,",
            "  b = 1",
            "]",
        ]
    )
    assert reference == value


regression_test_geometries = [
    (0.18430089, 1.46592582, 3.92553451, 0.39280281),
]


@pytest.mark.parametrize("theta_i,theta_o,phi_i,phi_o", regression_test_geometries)
def test_k_vol_angles(variant_scalar_mono, theta_i, theta_o, phi_i, phi_o):
    rtls = mi.load_dict({"type": "rtls", "f_iso": 0.0, "f_vol": 1.0, "f_geo": 0.0})

    wi = angles_to_directions(theta_i, phi_i)
    wo = angles_to_directions(theta_o, phi_o)
    value = eval_bsdf(rtls, wi, wo) / np.cos(theta_o)

    reference, k_vol, k_geo = rtls_reference(
        0.0,
        1.0,
        0.0,
        np.array([theta_i]),
        np.array([phi_i]),
        np.array([theta_o]),
        np.array([phi_o]),
    )

    assert dr.allclose(value, reference, rtol=1e-3, atol=1e-3, equal_nan=True)


@pytest.mark.parametrize("theta_i,theta_o,phi_i,phi_o", regression_test_geometries)
def test_k_geo_angles(variant_scalar_rgb, theta_i, theta_o, phi_i, phi_o):
    rtls = mi.load_dict({"type": "rtls", "f_iso": 0.0, "f_vol": 0.0, "f_geo": 1.0})

    wi = angles_to_directions(theta_i, phi_i)
    wo = angles_to_directions(theta_o, phi_o)
    value = eval_bsdf(rtls, wi, wo) / np.cos(theta_o)

    reference, k_vol, k_geo = rtls_reference(
        0.0,
        0.0,
        1.0,
        np.array([theta_i]),
        np.array([phi_i]),
        np.array([theta_o]),
        np.array([phi_o]),
    )

    assert dr.allclose(value, reference, rtol=1e-3, atol=1e-3, equal_nan=True)


@pytest.mark.parametrize("f_iso", [0.0, 1.0])
@pytest.mark.parametrize("f_vol", [0.0, 1.0])
@pytest.mark.parametrize("f_geo", [0.0, 1.0])
def test_rtls_scalar_combinations(variant_scalar_rgb, f_iso, f_vol, f_geo):
    rtls = mi.load_dict(
        {"type": "rtls", "f_iso": f_iso, "f_vol": f_vol, "f_geo": f_geo}
    )
    num_samples = 100

    theta_i = np.random.rand(num_samples) * np.pi / 2.0
    theta_o = np.random.rand(num_samples) * np.pi / 2.0
    phi_i = np.random.rand(num_samples) * np.pi * 2.0
    phi_o = np.random.rand(num_samples) * np.pi * 2.0

    values = []
    for j in range(num_samples):
        wi = angles_to_directions(theta_i[j], phi_i[j])
        wo = angles_to_directions(theta_o[j], phi_o[j])
        value = eval_bsdf(rtls, wi, wo) / np.cos(theta_o[j])
        values.append(value)

    values = np.asarray(values)

    reference, k_vol, k_geo = rtls_reference(
        f_iso, f_vol, f_geo, theta_i, phi_i, theta_o, phi_o
    )

    assert dr.allclose(values, reference, rtol=1e-3, atol=1e-3, equal_nan=True)


@pytest.mark.parametrize("f_iso", [0.004, 0.1, 0.497])
@pytest.mark.parametrize("f_vol", [0.29, 0.086, 0.2])
@pytest.mark.parametrize("f_geo", [0.543, 0.634, 0.851])
def test_rtls_vect(variant_llvm_ad_rgb, f_iso, f_vol, f_geo):
    rtls = mi.load_dict(
        {"type": "rtls", "f_iso": f_iso, "f_vol": f_vol, "f_geo": f_geo}
    )
    num_samples = 100

    theta_i = np.random.rand(num_samples) * np.pi / 2.0
    theta_o = np.random.rand(num_samples) * np.pi / 2.0
    phi_i = np.random.rand(num_samples) * np.pi * 2.0
    phi_o = np.random.rand(num_samples) * np.pi * 2.0

    wi = angles_to_directions(theta_i, phi_i)
    wo = angles_to_directions(theta_o, phi_o)
    values = eval_bsdf(rtls, wi, wo) / np.cos(theta_o)

    reference, k_vol, k_geo = rtls_reference(
        f_iso, f_vol, f_geo, theta_i, phi_i, theta_o, phi_o
    )
    assert dr.allclose(values, reference, rtol=1e-3, atol=1e-3, equal_nan=True)


@pytest.mark.parametrize("R", [0.0, 0.5, 1.0])
def test_eval_diffuse(variant_scalar_rgb, R):
    """
    Compare a degenerate RTLS case with a diffuse BRDF.
    """

    f_iso = R / np.pi
    f_vol = 0.0
    f_geo = 0.0

    rtls = mi.load_dict(
        {"type": "rtls", "f_iso": f_iso, "f_vol": f_vol, "f_geo": f_geo}
    )
    diffuse = mi.load_dict({"type": "diffuse", "reflectance": R})

    theta_i = np.random.rand(1) * np.pi / 2.0
    theta_o = np.random.rand(1) * np.pi / 2.0
    phi_i = np.random.rand(1) * np.pi * 2.0
    phi_o = np.random.rand(1) * np.pi * 2.0

    wi = angles_to_directions(theta_i, phi_i)
    wo = angles_to_directions(theta_o, phi_o)
    values = eval_bsdf(rtls, wi, wo)
    reference = eval_bsdf(diffuse, wi, wo) / np.pi

    assert np.allclose(values, reference, rtol=1e-3, atol=1e-3, equal_nan=True)


def test_chi2_rtls(variants_vec_backends_once_rgb):
    from mitsuba.chi2 import BSDFAdapter, ChiSquareTest, SphericalDomain

    sample_func, pdf_func = BSDFAdapter("rtls",
    """
        <float name="f_iso" value="0.209741"/>
        <float name="f_vol" value="0.081384"/>
        <float name="f_geo" value="0.004140"/>
        <float name="h" value="2"/>
        <float name="r" value="1"/>
        <float name="b" value="1"/>
    """)

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
    )

    assert chi2.run()


@pytest.mark.parametrize("wi", [[0, 0, 1], [0, 1, 1], [1, 1, 1]])
def test_sampling_weights_rpv(variant_scalar_rgb, wi):
    """
    Sampling weights are correctly computed, i.e. equal to eval() / pdf().
    """
    rtls = mi.load_dict({"type": "rtls"})
    (_, weight), eval, pdf = sample_eval_pdf_bsdf(
        rtls, dr.normalize(mi.ScalarVector3f(wi))
    )
    assert dr.allclose(weight, eval / pdf)


rami4atm_RLI_parameters = [
    (0.024950, -0.002250, 0.001513),  # S2A MSI B02
    (0.050877, -0.004504, 0.003073),  # S2A MSI B03
    (0.032171, -0.002886, 0.001949),  # S2A MSI B04
    (0.209741, 0.081384, 0.004140),  # S2A MSI B8A
    (0.095761, 0.035598, 0.002086),  # S2A MSI B11
    (0.046899, 0.017130, 0.001060),  # S2A MSI B12
]


@pytest.mark.parametrize("f_iso,f_vol,f_geo", rami4atm_RLI_parameters)
def test_rami4atm_sanity_check(variant_llvm_ad_rgb, f_iso, f_geo, f_vol, np_rng):
    # Given a set of parameters taken from the RAMI4ATM benchmark, assumed to be
    # physically correct, test the BRDF common properties:
    #  - Take positive values
    #  - Conserve energy
    #  - Obey the Helmholtz reciprocity relationship
    # Please refer to the RAMI4ATM scenario description by the JRC

    rtls = mi.load_dict(
        {"type": "rtls", "f_iso": f_iso, "f_vol": f_vol, "f_geo": f_geo}
    )

    num_samples = 1000

    # Use RAMI4ATM angle limits
    theta_i = np.deg2rad(np_rng.random(num_samples) * 80)
    theta_o = np.deg2rad(np_rng.random(num_samples) * 80)
    phi_i = np_rng.random(num_samples) * np.pi * 2.0
    phi_o = np_rng.random(num_samples) * np.pi * 2.0

    wi = angles_to_directions(theta_i, phi_i)
    wo = angles_to_directions(theta_o, phi_o)
    values = eval_bsdf(rtls, wi, wo)

    reference, k_vol, k_geo = rtls_reference(
        f_iso, f_vol, f_geo, theta_i, phi_i, theta_o, phi_o
    )

    # Sanity check against the reference implementation
    assert dr.allclose(values / np.cos(theta_o), reference, rtol=1e-3, atol=1e-3)

    # Check all positives
    problematic_geometries = np.concatenate(
        [
            np.rad2deg(theta_i.reshape(-1, 1)),
            np.rad2deg(theta_o.reshape(-1, 1)),
            np.rad2deg(phi_i.reshape(-1, 1)),
            np.rad2deg(phi_o.reshape(-1, 1)),
            np.array(values).reshape(-1, 1),
        ],
        axis=1,
    )[values / np.cos(theta_o) < 0.0]

    if len(problematic_geometries):
        print(
            "Found negative values in RTLS evaluation: [theta_i, theta_o, phi_i, phi_o, rtls_bsdf]"
        )
        print(problematic_geometries)

    assert len(problematic_geometries) == 0

    # Checking Helmholtz reciprocity
    reciprocal_values = eval_bsdf(rtls, wo, wi)
    assert dr.allclose(
        values / np.cos(theta_o),
        reciprocal_values / np.cos(theta_i),
        rtol=1e-3,
        atol=1e-3,
    )

    # Checking the integral over the hemisphere is lower than 1
    def sph_to_dir(theta, phi):
        """Map spherical to Euclidean coordinates"""
        st, ct = dr.sincos(theta)
        sp, cp = dr.sincos(phi)
        return mi.Vector3f(cp * st, sp * st, ct)

    si = dr.zeros(mi.SurfaceInteraction3f)
    si.wi = sph_to_dir(dr.deg2rad(45.0), 0.0)
    n = 100
    theta_o, phi_o = dr.meshgrid(
        dr.linspace(mi.Float, 0, dr.pi, n), dr.linspace(mi.Float, 0, 2 * dr.pi, 2 * n)
    )
    wo = sph_to_dir(theta_o, phi_o)
    rho_wi = np.array(rtls.eval(mi.BSDFContext(), si, wo))
    diff_wo = np.sin(theta_o) * (np.pi / n) * (np.pi / n)
    diff_wo = diff_wo.reshape(diff_wo.shape[0], 1) @ np.ones((1, rho_wi.shape[1]))
    rho_wi = rho_wi * diff_wo
    assert all(np.nan_to_num(np.sum(rho_wi, axis=0)) <= 1.0)


# check other values of h, r and b
@pytest.mark.parametrize(
    "h,r,b",
    [
        (2.0, 1.5, 1.0),
        (2.0, 1.0, 0.5),
        (1.8, 0.8, 0.5),
    ],
)
@pytest.mark.parametrize("f_iso,f_vol,f_geo", rami4atm_RLI_parameters)
def test_hrb_parameters(variant_scalar_rgb, f_iso, f_geo, f_vol, h, r, b):
    rtls = mi.load_dict(
        {
            "type": "rtls",
            "f_iso": f_iso,
            "f_vol": f_vol,
            "f_geo": f_geo,
            "h": h,
            "r": r,
            "b": b,
        }
    )
    num_samples = 100

    theta_i = np.random.rand(num_samples) * np.pi / 2.0
    theta_o = np.random.rand(num_samples) * np.pi / 2.0
    phi_i = np.random.rand(num_samples) * np.pi * 2.0
    phi_o = np.random.rand(num_samples) * np.pi * 2.0

    values = []
    for j in range(num_samples):
        wi = angles_to_directions(theta_i[j], phi_i[j])
        wo = angles_to_directions(theta_o[j], phi_o[j])
        value = eval_bsdf(rtls, wi, wo) / np.cos(theta_o[j])
        values.append(value)

    values = np.asarray(values)

    reference, k_vol, k_geo = rtls_reference(
        f_iso, f_vol, f_geo, theta_i, phi_i, theta_o, phi_o, h, r, b
    )
    assert dr.allclose(values, reference, rtol=1e-2, atol=1e-2, equal_nan=True)
