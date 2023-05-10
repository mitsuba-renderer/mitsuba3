import drjit as dr
import mitsuba as mi
import numpy as np
import pytest


def sph_to_dir(theta, phi):
    """Map spherical to Euclidean coordinates"""
    st, ct = dr.sincos(theta)
    sp, cp = dr.sincos(phi)
    return mi.Vector3f(cp * st, sp * st, ct)


def grid(bsdf, n_theta_o, n_phi_o, theta_i, phi_i, apply_cos_theta=False):
    # Create a (dummy) surface interaction to use for the evaluation of the BSDF
    si = dr.zeros(mi.SurfaceInteraction3f)

    # Specify incident direction
    si.wi = sph_to_dir(theta_i, phi_i)

    # Create grid in spherical coordinates and map it onto the hemisphere
    theta_o, phi_o = dr.meshgrid(
        dr.linspace(mi.Float, 0, 0.5 * dr.pi, n_theta_o),
        dr.linspace(mi.Float, 0, 2 * dr.pi, n_phi_o),
    )
    wo = sph_to_dir(theta_o, phi_o)
    value = bsdf.eval(mi.BSDFContext(), si, wo)
    if apply_cos_theta:
        value /= dr.cos(theta_o)

    return value


def discretize(bsdf, n_theta_o, n_theta_i, n_phi_d):
    # Build list of incident and outgoing directions
    cos_theta_os = dr.linspace(mi.Float, 0, 1, n_theta_o)
    theta_os = dr.acos(cos_theta_os)
    cos_theta_is = dr.linspace(mi.Float, 0, 1, n_theta_i)
    theta_is = dr.acos(cos_theta_is)
    phi_os = dr.linspace(mi.Float, -dr.pi, dr.pi, n_phi_d)
    phi_i = 0.0

    # Generate vector coordinate lists
    theta_ov, phi_ov, theta_iv, phi_iv = dr.meshgrid(
        theta_os,
        phi_os,
        theta_is,
        phi_i,
    )

    wos = sph_to_dir(theta_ov, phi_ov)
    wis = sph_to_dir(theta_iv, phi_iv)

    # Evaluate BSDF
    si = dr.zeros(mi.SurfaceInteraction3f)
    si.wi = wis
    return bsdf.eval(mi.BSDFContext(), si, wos)


def test_construct(variant_scalar_rgb):
    grid = mi.VolumeGrid(np.ones((5, 4, 3, 1)))
    # print(grid)

    mqd = mi.load_dict({"type": "mqdiffuse", "grid": grid})
    # print(mqd)
    assert mqd is not None


SAMPLE_DATA = np.array(
    [
        # cos_theta_i = 0
        [
            # phi_d = 0
            np.linspace(0, 1, 5),
            # phi_d = π
            -np.linspace(0, 1, 5),
            # phi_d = 2π
            np.linspace(0, 1, 5),
        ],
        # cos_theta_i = 1
        [
            # phi_d = 0
            np.linspace(1, 2, 5),
            # phi_d = π
            -np.linspace(1, 2, 5),
            # phi_d = 2π
            np.linspace(1, 2, 5),
        ],
    ],
)


@pytest.fixture(scope="module")
def plugin():
    yield mi.load_dict({"type": "mqdiffuse", "grid": mi.VolumeGrid(SAMPLE_DATA)})


@pytest.mark.parametrize(
    "theta_o, phi_o, theta_i, phi_i, expected",
    [
        # The following values are hand-picked to be easily tracked in the table
        # used to initialize the plugin
        [np.pi / 3, 0.0, 0.0, 0.0, 1.5],
        [np.pi * 0.4195693767448338, 0.0, 0.0, 0.0, 1.25],  # Such that cos θ = 0.25
        [np.pi / 3, np.pi, 0.0, 0.0, -1.5],
        [np.pi / 3, 0.5 * np.pi, 0.0, 0.0, 0.0],
        [np.pi / 3, 1.5 * np.pi, 0.0, 0.0, 0.0],
        [np.pi / 2, 0.0, np.pi / 2, 0.0, 0.0],
        [np.pi / 3, 0.0, np.pi / 2, 0.0, 0.5],
        [np.pi / 3, np.pi, np.pi / 2, 0.0, -0.5],
        [np.pi / 2, np.pi, np.pi / 2, 0.0, 0.0],
        [np.pi / 2, np.pi, 0.0, 0.0, -1.0],
    ],
)
def test_eval(variant_scalar_rgb, plugin, theta_o, phi_o, theta_i, phi_i, expected):
    """
    Check that the eval() method evaluates the underlying Dr.Jit texture
    correctly.
    """
    wo = sph_to_dir(theta_o, phi_o)
    si = dr.zeros(mi.SurfaceInteraction3f)
    si.wi = sph_to_dir(theta_i, phi_i)
    value = plugin.eval(mi.BSDFContext(), si, wo)[0]
    assert dr.allclose(value, expected * dr.cos(theta_o), atol=1e-6)


def test_chi2(variants_vec_backends_once_rgb, tmp_path):
    from mitsuba.chi2 import BSDFAdapter, ChiSquareTest, SphericalDomain

    filename = str(tmp_path / "data.vol")
    mi.VolumeGrid(SAMPLE_DATA).write(filename)

    sample_func, pdf_func = BSDFAdapter(
        "mqdiffuse",
        f"<string name='filename' value='{filename}'/>",
    )

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
    )

    result = chi2.run()
    # chi2._dump_tables()
    assert result
