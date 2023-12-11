import drjit as dr
import mitsuba as mi
import numpy as np
import xarray as xr
import pytest

from os.path import join

import matplotlib.pyplot as plt

from ..tools import sample_eval_pdf_bsdf
from mitsuba.scalar_rgb.test.util import find_resource


def angles_to_directions(theta, phi):
    return mi.Vector3f(
        np.sin(theta) * np.cos(phi), np.sin(theta) * np.sin(phi), np.cos(theta)
    )


def eval_bsdf(bsdf, wi, wo):
    si = mi.SurfaceInteraction3f()
    si.wi = wi
    ctx = mi.BSDFContext()
    return bsdf.eval(ctx, si, wo, True)[0]


@pytest.fixture
def static_pplane():
    references = find_resource("resources/tests/eradiate_plugins/bsdfs")

    hapke_reference_filename = join(references, "hapke_principal_plane_example.nc")

    return xr.load_dataset(hapke_reference_filename)


@pytest.fixture
def static_hemisphere():
    references = find_resource("resources/tests/eradiate_plugins/bsdfs")

    hapke_reference_filename = join(references, "hapke_hemisphere_example.nc")

    return xr.load_dataset(hapke_reference_filename)


@pytest.fixture
def plot_figures():
    return False


def test_create_hapke(variant_scalar_rgb):
    # Test constructor of 3-parameter version of RPV
    rtls = mi.load_dict(
        {
            "type": "hapke",
            "w": 0.0,
            "b": 0.0,
            "c": 0.0,
            "theta": 0.0,
            "h": 0.0,
            "B_0": 0.0,
        }
    )

    assert isinstance(rtls, mi.BSDF)
    assert rtls.component_count() == 1
    assert rtls.flags(0) == mi.BSDFFlags.GlossyReflection | mi.BSDFFlags.FrontSide
    assert rtls.flags() == rtls.flags(0)

    params = mi.traverse(rtls)
    assert "w.value" in params
    assert "b.value" in params
    assert "c.value" in params
    assert "theta.value" in params
    assert "B_0.value" in params
    assert "h.value" in params


def test_defaults_and_print(variant_scalar_rgb):
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


def test_eval_hotspot(variant_scalar_rgb):
    c = 0.273
    c = (1 + c) / 2

    hapke = mi.load_dict(
        {  # Pommerol et al. (2013)
            "type": "hapke",
            "w": 0.526,
            "theta": 13.3,
            "b": 0.187,
            "c": c,
            "h": 0.083,
            "B_0": 1.0,
        }
    )

    theta_i = np.deg2rad(30.0)
    theta_o = np.deg2rad(30.0)
    phi_i = np.deg2rad(0.0)
    phi_o = np.deg2rad(0.0)

    wi = angles_to_directions(theta_i, phi_i)
    wo = angles_to_directions(theta_o, phi_o)

    values = np.asarray(eval_bsdf(hapke, wi, wo)) / np.abs(np.cos(theta_o))
    assert np.allclose(values, 0.24746648)


def test_hapke_grazing_outgoing_direction(variant_scalar_rgb):
    c = 0.273
    c = (1 + c) / 2

    hapke = mi.load_dict(
        {  # Pommerol et al. (2013)
            "type": "hapke",
            "w": 0.526,
            "theta": 13.3,
            "b": 0.187,
            "c": c,
            "h": 0.083,
            "B_0": 1.0,
        }
    )

    theta_i = np.deg2rad(30.0)
    theta_o = np.deg2rad(-89.0)
    phi_i = np.deg2rad(0.0)
    phi_o = np.deg2rad(0.0)

    wi = angles_to_directions(theta_i, phi_i)
    wo = angles_to_directions(theta_o, phi_o)

    values = np.asarray(eval_bsdf(hapke, wi, wo)) / np.abs(np.cos(theta_o))
    assert np.allclose(values, 0.15426355)


def test_eval_backward(variant_scalar_rgb):
    c = 0.273
    c = (1 + c) / 2

    hapke = mi.load_dict(
        {  # Pommerol et al. (2013)
            "type": "hapke",
            "w": 0.526,
            "theta": 13.3,
            "b": 0.187,
            "c": c,
            "h": 0.083,
            "B_0": 1.0,
        }
    )

    theta_i = np.deg2rad(30.0)
    theta_o = np.deg2rad(80.0)
    phi_i = np.deg2rad(0.0)
    phi_o = np.deg2rad(0.0)

    wi = angles_to_directions(theta_i, phi_i)
    wo = angles_to_directions(theta_o, phi_o)

    values = np.asarray(eval_bsdf(hapke, wi, wo)) / np.abs(np.cos(theta_o))
    assert np.allclose(values, 0.19555340)


def test_hapke_hemisphere(variant_llvm_ad_rgb, static_hemisphere, plot_figures):
    hapke = mi.load_dict(
        {  # Pommerol et al. (2013)
            "type": "hapke",
            "w": static_hemisphere.w.item(),
            "theta": static_hemisphere.theta.item(),
            "b": static_hemisphere.b.item(),
            "c": static_hemisphere.c.item(),
            "h": static_hemisphere.h.item(),
            "B_0": static_hemisphere.B0.item(),
        }
    )

    azimuths_s = 361
    zeniths_s = 90

    azimuths = np.radians(np.linspace(0, 360, azimuths_s))
    zeniths = np.linspace(0, 89, zeniths_s)

    r, theta = np.meshgrid(np.sin(np.deg2rad(zeniths)), azimuths)
    values = np.random.random((azimuths.size, zeniths.size))

    theta_i = np.deg2rad(30.0 * np.ones((zeniths_s,))).reshape(-1, 1) @ np.ones(
        (1, azimuths_s)
    )
    theta_o = np.deg2rad(zeniths).reshape(-1, 1) @ np.ones((1, azimuths_s))
    phi_i = np.zeros((zeniths_s, azimuths_s))
    phi_o = np.ones((zeniths_s, 1)) @ azimuths.reshape(1, -1)

    wi = angles_to_directions(theta_i.reshape(1, -1), phi_i.reshape(1, -1))
    wo = angles_to_directions(theta_o.reshape(1, -1), phi_o.reshape(1, -1))

    npvalues = np.asarray(eval_bsdf(hapke, wi, wo)) / np.abs(np.cos(theta_o)).reshape(
        1, -1
    )
    npvalues = npvalues.reshape((zeniths_s, azimuths_s)).T

    ref = np.asarray(static_hemisphere.reflectance.values)
    print(npvalues.shape, ref.shape)

    if plot_figures:
        fig, ax = plt.subplots(subplot_kw=dict(projection="polar"))
        contour = ax.contourf(theta, r, npvalues, levels=25, cmap="turbo")
        plt.colorbar(contour)
        plt.grid(False)
        plt.savefig("hapke_hemisphere_mitsuba.png")
        plt.close()

        fig, ax = plt.subplots(subplot_kw=dict(projection="polar"))
        contour = ax.contourf(theta, r, ref, levels=25, cmap="turbo")
        plt.colorbar(contour)
        plt.grid(False)
        plt.savefig("hapke_hemisphere_reference.png")
        plt.close()

        fig, ax = plt.subplots(subplot_kw=dict(projection="polar"))
        contour = ax.contourf(theta, r, ref - npvalues, levels=25, cmap="turbo")
        plt.colorbar(contour)
        plt.grid(False)
        plt.savefig("hapke_hemisphere_diff.png")
        plt.close()

    assert np.allclose(ref, npvalues)


def test_hapke_static_principal_plane_reference(
    variant_llvm_ad_rgb, static_pplane, plot_figures
):
    hapke = mi.load_dict(
        {  # Pommerol et al. (2013)
            "type": "hapke",
            "w": static_pplane.w.item(),
            "theta": static_pplane.theta.item(),
            "b": static_pplane.b.item(),
            "c": static_pplane.c.item(),
            "h": static_pplane.h.item(),
            "B_0": static_pplane.B0.item(),
        }
    )

    theta_i = np.deg2rad(30.0 * np.ones((180,)))
    theta_o = np.deg2rad(static_pplane.svza.values)
    phi_i = np.deg2rad(np.zeros((180,)))
    phi_o = np.deg2rad(np.zeros((180,)))

    wi = angles_to_directions(theta_i, phi_i)
    wo = angles_to_directions(theta_o, phi_o)

    values = np.asarray(eval_bsdf(hapke, wi, wo)) / np.abs(np.cos(theta_o))

    if plot_figures:
        fig = plt.figure()
        plt.grid()
        plt.plot(np.rad2deg(theta_o), values, marker="+", label="Eradiate/Mitsuba")
        plt.plot(
            np.rad2deg(theta_o),
            static_pplane.reflectance,
            label="Nguyen, Jacquemoud et al. (reference)",
        )
        plt.legend()

    ref = static_pplane.reflectance.values

    assert np.allclose(ref, values)


def test_chi2_hapke(variants_vec_backends_once_rgb):
    from mitsuba.chi2 import BSDFAdapter, ChiSquareTest, SphericalDomain

    sample_func, pdf_func = BSDFAdapter(
        "hapke",
        """
        <float name="w" value="0.526"/>
        <float name="theta" value="13.3"/>
        <float name="b" value="0.187"/>
        <float name="c" value="0.273"/>
        <float name="h" value="0.083"/>
        <float name="B_0" value="1"/>
    """,
    )

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3,
    )

    assert chi2.run()
