import pytest
import numpy as np
import drjit as dr
import mitsuba as mi

from mitsuba.scalar_rgb.test.util import find_resource

eps = 1e-4
SIN_OFFSET = 0.00775
SUN_HALF_APERTURE_ANGLE = dr.deg2rad(0.5388/2.0)

sunsky_ref_folder = "resources/data/tests/sunsky"


def make_emitter_hour(turb, hour, albedo, sky_scale, sun_scale, complex_sun=True):
    return mi.load_dict({
        "type": "sunsky",
        "hour": hour,
        "sun_scale": sun_scale,
        "sky_scale": sky_scale,
        "turbidity": turb,
        "albedo": albedo,
        "complex_sun": complex_sun
    })


def make_emitter_angles(turb, sun_phi, sun_theta, albedo, sky_scale, sun_scale, complex_sun=True, sun_aperture=0.5358):
    sp_sun, cp_sun = dr.sincos(sun_phi)
    st, ct = dr.sincos(sun_theta)

    return mi.load_dict({
        "type": "sunsky",
        "sun_direction": [cp_sun * st, sp_sun * st, ct],
        "sun_scale": sun_scale,
        "sky_scale": sky_scale,
        "turbidity": turb,
        "albedo": albedo,
        "complex_sun": complex_sun,
        "sun_aperture": sun_aperture
    })


def eval_full_spec(plugin, si, wavelengths, render_res = (512, 1024)):
    """
    Evaluates the plugin for the given surface interaction over all the given wavelengths
    :param plugin: Sunsky plugin to evaluate
    :param si: Surface interaction to evaluate the plugin with
    :param wavelengths: List of wavelengths to evaluate
    :param render_res: Resolution of the output image (the number of rays in SI should be equal to render_res[0] * render_res[1])
    :return: The output TensorXf image in format (render_res[0], render_res[1], len(wavelengths))
    """
    nb_channels = len(wavelengths)

    output_image = dr.zeros(mi.Float, render_res[0] * render_res[1] * nb_channels)
    for i, lbda in enumerate(wavelengths):
        si.wavelengths = lbda
        res = plugin.eval(si)[0]
        dr.scatter(output_image, res, i + nb_channels * dr.arange(mi.UInt32, render_res[0] * render_res[1]))

    return mi.TensorXf(output_image, (*render_res, nb_channels))

def render_and_compare(plugin, ref_path, rtol):
    """
    Renders the sky radiance with the given parameters and compares it to the reference image
    :param plugin: sunsky emitter to evaluate
    :param ref_path: Path to the reference image
    :param rtol: Relative error tolerance
    """
    render_res = (64//2, 64)

    # Generate the wavelengths
    start, end = 360, 830
    step = (end - start) / 10
    wavelengths = [start + step/2 + i*step for i in range(10)]

    # Generate the rays
    phis, thetas = dr.meshgrid(
        dr.linspace(mi.Float, 0.0, dr.two_pi, render_res[1]),
        dr.linspace(mi.Float, dr.pi, 0.0, render_res[0]))
    sp, cp = dr.sincos(phis)
    st, ct = dr.sincos(thetas)

    si = dr.zeros(mi.SurfaceInteraction3f)
    si.wi = mi.Vector3f(cp*st, sp*st, ct)

    rendered_scene = mi.TensorXf(dr.ravel(mi.unpolarized_spectrum(plugin.eval(si))), (*render_res, 3)) if mi.is_rgb \
                else eval_full_spec(plugin, si, wavelengths, render_res)

    # Load the reference image
    reference_scene = mi.TensorXf32(mi.Bitmap(find_resource(ref_path)))
    rel_err = dr.mean(dr.abs(rendered_scene - reference_scene) / (dr.abs(reference_scene) + 0.001))

    assert rel_err <= rtol, (f"Fail when rendering plugin: {plugin}\n"
                             f"Mean relative error: {rel_err}, threshold: {rtol}")


@pytest.mark.parametrize("render_params", [
    (9.5, 2, 0.2),
    (12.25, 5.2, 0.0),
    (18.3, 9.8, 0.5),
])
def test01_sky_radiance_rgb(variants_vec_rgb, render_params):
    hour, turb, albedo = render_params
    if mi.is_polarized:
        pytest.skip('Test must be adapted to polarized rendering.')

    ref_path = f"{sunsky_ref_folder}/renders/sky_rgb_hour{hour:.2f}_t{turb:.3f}_a{albedo:.3f}.exr"

    plugin = make_emitter_hour(turb=turb,
                               hour=hour,
                               albedo=albedo,
                               sun_scale=0.0,
                               sky_scale=1.0)

    render_and_compare(plugin, ref_path, 0.017)


@pytest.mark.parametrize("render_params", [
    (dr.deg2rad(2), 2, 0.0),
    (dr.deg2rad(20), 5.2, 0.0),
    (dr.deg2rad(45), 9.8, 0.0),
])
def test02_sky_radiance_spectral(variants_vec_spectral, render_params):
    sun_eta, turb, albedo = render_params
    if mi.is_polarized:
        pytest.skip('Test must be adapted to polarized rendering.')


    ref_path = f"{sunsky_ref_folder}/renders/sky_spec_eta{sun_eta:.3f}_t{turb:.3f}_a{albedo:.3f}.exr"

    plugin = make_emitter_angles(turb=turb,
                                 sun_phi=0,
                                 sun_theta=dr.pi/2 - sun_eta,
                                 albedo=albedo,
                                 sun_scale=0.0,
                                 sky_scale=1.0)

    render_and_compare(plugin, ref_path, 0.037)


def test03_sky_radiance_spectral_albedo(variants_vec_spectral):
    if mi.is_polarized:
        pytest.skip('Test must be adapted to polarized rendering.')

    special_albedo = {
        'type': 'irregular',
        'wavelengths': '320, 360, 400, 440, 480, 520, 560, 600, 640, 680, 720',
        'values': '0.56, 0.21, 0.58, 0.24, 0.92, 0.42, 0.53, 0.75, 0.54, 0.20, 0.46'
    }

    sun_eta, turb, albedo = dr.deg2rad(60), 4.2, special_albedo
    plugin = make_emitter_angles(turb=turb,
                                 sun_phi=0,
                                 sun_theta=dr.pi/2 - sun_eta,
                                 albedo=albedo,
                                 sun_scale=0.0,
                                 sky_scale=1.0)

    render_and_compare(plugin, f"{sunsky_ref_folder}/renders/sky_spectrum_special.exr", 0.03)


def extract_spectrum(turb, eta, gamma):
    with open(find_resource(f"{sunsky_ref_folder}/spectrum/sun_spectrum_t{turb:.1f}_eta{eta:.2f}_gamma{gamma:.3e}.spd"), "r") as f:
        return [float(line.split()[1]) for line in f.readlines()]


@pytest.mark.parametrize("turb",    np.linspace(1, 10, 5))
@pytest.mark.parametrize("eta_ray", np.linspace(eps, dr.pi/2 - eps, 4))
@pytest.mark.parametrize("gamma",   np.linspace(0, SUN_HALF_APERTURE_ANGLE - eps, 4))
def test04_sun_radiance(variants_vec_spectral, turb, eta_ray, gamma):
    if mi.is_polarized:
        pytest.skip('Test must be adapted to polarized rendering.')

    wavelengths = np.linspace(310, 800, 15)

    phi = dr.pi/5
    theta_ray = dr.pi/2 - eta_ray

    # Determine the sun's elevation based on the queried ray elevation
    # There are two solutions to this problem, if the one with the - sign is less than 0, we take the other one
    sun_theta = theta_ray - gamma
    if sun_theta < 0:
        sun_theta = theta_ray + gamma

    plugin = make_emitter_angles(turb=turb,
                                 sun_phi=phi,
                                 sun_theta=sun_theta,
                                 albedo=0.0,
                                 sun_scale=1.0,
                                 sky_scale=0.0)

    # Generate rays
    si = dr.zeros(mi.SurfaceInteraction3f)
    si.wavelengths = mi.Float(wavelengths)

    sp, cp = dr.sincos(phi)
    st, ct = dr.sincos(theta_ray)
    si.wi = -mi.Vector3f(cp * st, sp * st, ct)

    # Evaluate the plugin
    res = plugin.eval(si)[0]

    # Load the reference spectrum
    ref_rad = extract_spectrum(turb, eta_ray, gamma)
    ref_rad = mi.Float(ref_rad)
    rel_err = dr.mean(dr.abs(res - ref_rad) / (ref_rad + 1e-6))

    rtol = 1e-2
    assert rel_err <= rtol, (f"Fail when rendering sun with ray at turbidity {turb}, elevation {eta_ray} and gamma {gamma}\n"
                             f"Reference spectrum: {ref_rad}\n"
                             f"Rendered spectrum: {res}\n")


@pytest.mark.parametrize("sun_theta", np.linspace(0, dr.pi/2, 4))
def test05_sun_sampling(variants_vec_backends_once, sun_theta):
    sun_phi = -dr.pi / 5
    sp, cp = dr.sincos(sun_phi)
    st, ct = dr.sincos(sun_theta)
    sun_dir = mi.Vector3f(cp * st, sp * st, ct)

    plugin = make_emitter_angles(turb=4.0,
                          sun_phi=sun_phi,
                          sun_theta=sun_theta,
                          albedo=0.0,
                          sun_scale=1.0,
                          sky_scale=0.0)

    rng = mi.PCG32(size=5_000)
    sample = mi.Point2f(
        rng.next_float32(),
        rng.next_float32())

    it = dr.zeros(mi.Interaction3f)
    ds, w = plugin.sample_direction(it, sample, True)

    # With epsilon to account for numerical precision
    all_in_sun_cone = dr.all(dr.dot(ds.d, sun_dir) >= (dr.cos(SUN_HALF_APERTURE_ANGLE) - dr.epsilon(mi.Float)), axis=None)
    assert all_in_sun_cone, "Sampled direction should be in the sun's direction"


class CroppedSphericalDomain(mi.chi2.SphericalDomain):
    """
    Cropped spherical domain that avoids the singularity at the north pole by SIN_OFFSET
    """
    def bounds(self):
        cos_bound = dr.sqrt(1 - dr.square(SIN_OFFSET))
        return mi.ScalarBoundingBox2f([-dr.pi, -cos_bound], [dr.pi, 1])


@pytest.mark.slow
@pytest.mark.parametrize("turb",      [2.2, 4.8, 6.0])
@pytest.mark.parametrize("sun_theta", [dr.deg2rad(20), dr.deg2rad(50)])
def test06_sky_sampling(variants_vec_backends_once, turb, sun_theta):
    phi_sun = -4*dr.pi/5
    sp_sun, cp_sun = dr.sincos(phi_sun)
    st, ct = dr.sincos(sun_theta)

    sky = {
        "type": "sunsky",
        "sun_direction": [cp_sun * st, sp_sun * st, ct],
        "sun_scale": 0.0,
        "turbidity": turb,
        "albedo": 0.5
    }

    sample_func, pdf_func = mi.chi2.EmitterAdapter("sunsky", sky)
    test = mi.chi2.ChiSquareTest(
        domain=CroppedSphericalDomain(),
        pdf_func= pdf_func,
        sample_func= sample_func,
        sample_dim=2,
        sample_count=200_000,
        res=55,
        ires=32
    )

    assert test.run(), "Chi2 test failed"


@pytest.mark.slow
@pytest.mark.parametrize("turb",      [2.2, 4.8, 6.0])
@pytest.mark.parametrize("sun_theta", [dr.deg2rad(20), dr.deg2rad(50)])
def test07_sun_and_sky_sampling(variants_vec_backends_once, turb, sun_theta):
    phi_sun = -4*dr.pi/5
    sp_sun, cp_sun = dr.sincos(phi_sun)
    st, ct = dr.sincos(sun_theta)

    sky = {
        "type": "sunsky",
        "sun_direction": [cp_sun * st, sp_sun * st, ct],
        "turbidity": turb,
        # Increase the sun aperture and scale to avoid errors with chi2's resolution
        "sun_scale": 1.25,
        "sun_aperture": 30.0,
        "albedo": 0.5
    }

    sample_func, pdf_func = mi.chi2.EmitterAdapter("sunsky", sky)
    test = mi.chi2.ChiSquareTest(
        domain=CroppedSphericalDomain(),
        pdf_func= pdf_func,
        sample_func= sample_func,
        sample_dim=2,
        sample_count=200_000,
        res=55,
        ires=32
    )

    assert test.run(), "Chi2 test failed"

def test08_update_params(variants_vec_rgb):
    rtol = 0.017

    hour, turb, albedo = 9.5, 2, 0.2
    ref_path = f"{sunsky_ref_folder}/renders/sky_rgb_hour{hour:.2f}_t{turb:.3f}_a{albedo:.3f}.exr"

    plugin = make_emitter_hour(turb=turb,
                               hour=hour,
                               albedo=albedo,
                               sun_scale=0.0,
                               sky_scale=1.0)

    render_and_compare(plugin, ref_path, rtol)


    hour, turb, albedo = 12.25, 5.2, 0.0
    ref_path = f"{sunsky_ref_folder}/renders/sky_rgb_hour{hour:.2f}_t{turb:.3f}_a{albedo:.3f}.exr"

    params = mi.traverse(plugin)
    params["hour"] = hour
    params["turbidity"] = turb
    params["albedo.value"] = albedo

    params.update()

    render_and_compare(plugin, ref_path, rtol)

def sun_integrand(emitter, quad_points, quad_weights, sun_direction, sun_cos_cutoff):
    j = 0.5 * dr.pi * (1. - sun_cos_cutoff)
    phi = dr.pi * (quad_points + 1)
    cos_theta = 0.5 * ((1. - sun_cos_cutoff) * quad_points + (1 + sun_cos_cutoff))

    phi, cos_theta = dr.meshgrid(phi, cos_theta)
    w_phi, w_cos_theta = dr.meshgrid(quad_weights, quad_weights)
    sin_phi, cos_phi = dr.sincos(phi)
    sin_theta = dr.safe_sqrt(1 - cos_theta * cos_theta)

    sun_wo = mi.Vector3f(sin_theta * cos_phi, sin_theta * sin_phi, cos_theta)

    si = dr.zeros(mi.SurfaceInteraction3f)
    si.wi = -mi.Frame3f(sun_direction).to_world(sun_wo)


    if dr.hint(mi.is_rgb, mode="scalar"):
        radiance = emitter.eval(si)
        return j * radiance * w_phi * w_cos_theta

    else:
        wav_res = dr.zeros(mi.ArrayXf, (11, 1))
        for i in range(11):
            si.wavelengths = 320 + i * 40
            radiance = emitter.eval(si)
            wav_res[i] = radiance[0]

        return j * wav_res * w_phi * w_cos_theta


@pytest.mark.parametrize("turb", [2.0, 4.0, 6.0])
@pytest.mark.parametrize("theta", [dr.deg2rad(20), dr.deg2rad(45)])
@pytest.mark.parametrize("sun_aperture", [0.5358, 5.0])
def test09_complex_sun(variants_vec_rgb, turb, theta, sun_aperture):
    """
    Validates that the complex sun model is consistent with the simple
    sun model in terms of integrated radiance over the sun's solid angle.
    """

    if mi.is_polarized:
        pytest.skip('Test must be adapted to polarized rendering.')

    complex_sun_plugin = make_emitter_angles(
        turb=turb, sun_phi=0, sun_theta=theta,
        albedo=0.5, sun_scale=1.0, sky_scale=0.0,
        complex_sun=True, sun_aperture=sun_aperture)
    complex_sun_plugin_params = mi.traverse(complex_sun_plugin)

    simple_sun_plugin = make_emitter_angles(
        turb=turb, sun_phi=0, sun_theta=theta,
        albedo=0.5, sun_scale=1.0, sky_scale=0.0,
        complex_sun=False, sun_aperture=sun_aperture)
    simple_sun_plugin_params = mi.traverse(simple_sun_plugin)

    points, weights = mi.quad.gauss_legendre(200)
    complex_sun_integrand = sun_integrand(complex_sun_plugin, points, weights,
                                          complex_sun_plugin_params["sun_direction"],
                                          dr.cos(dr.deg2rad(sun_aperture) / 2.0))

    simple_sun_integrand = sun_integrand(simple_sun_plugin, points, weights,
                                         simple_sun_plugin_params["sun_direction"],
                                         dr.cos(dr.deg2rad(sun_aperture) / 2.0))

    complex_irradiance = dr.sum(complex_sun_integrand, axis=1)
    simple_irradiance = dr.sum(simple_sun_integrand, axis=1)

    assert np.allclose(complex_irradiance, simple_irradiance, rtol=0.01)


def test10_invalid_sun_angles(variants_vec_backends_once_spectral):
    """
    Validates that invalid sun angles are correctly clamped and does not throw errors
    """

    if mi.is_polarized:
        pytest.skip('Test must be adapted to polarized rendering.')

    si = dr.zeros(mi.SurfaceInteraction3f)
    si.wi = mi.Vector3f(0, 0, -1)

    # Use `sun_direction` constructor
    plugin = make_emitter_angles(
        turb=2.0, sun_phi=0, sun_theta=dr.deg2rad(-5),
        albedo=0.5, sun_scale=1.0, sky_scale=1.0,
        complex_sun=True)
    assert dr.all(plugin.eval(si) == 0.0, axis=None)

    # Use `time` and `position` constructor
    plugin = make_emitter_hour(
        turb=2.0, hour=0.0,
        albedo=0.5, sun_scale=1.0, sky_scale=1.0,
        complex_sun=True)
    assert dr.all(plugin.eval(si) == 0.0, axis=None)
