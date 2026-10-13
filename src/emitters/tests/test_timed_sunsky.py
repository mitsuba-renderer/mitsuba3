import pytest
import numpy as np

import drjit as dr
import mitsuba as mi

UINT_32_MAX = (1 << 32) - 1

def TimedSunskyAdapter(plugin, time_value):
    """
    Extracts the sampling and pdf functions of a TimedSunskyEmitter
    It also adds a varying time in order to get more code coverage
    :param plugin: TimedSunskyPlugin
    :param time_value: Time value for the current Chi2 test
    :return: Sampling function and pdf function
    """
    def sample_functor(sample, *args):
        si = dr.zeros(mi.Interaction3f)
        si.time = mi.Float(time_value)
        ds, w = plugin.sample_direction(si, sample)
        return ds.d

    def pdf_functor(wo, *args):
        si = dr.zeros(mi.Interaction3f)
        ds = dr.zeros(mi.DirectionSample3f)
        ds.d = wo
        ds.time = mi.Float(time_value)

        return plugin.pdf_direction(si, ds)

    return sample_functor, pdf_functor

def generate_rays(render_res):
    phis, thetas = dr.meshgrid(
        dr.linspace(mi.Float, 0.0, dr.two_pi, render_res[0], endpoint=False),
        dr.linspace(mi.Float, 0, dr.pi, render_res[1], endpoint=False)
    )

    sp, cp = dr.sincos(phis)
    st, ct = dr.sincos(thetas)

    si = dr.zeros(mi.SurfaceInteraction3f)
    si.wi = -mi.Vector3f(cp*st, sp*st, ct)

    return si

def generate_average(plugin, render_res, time_res):
    nb_rays = dr.prod(render_res)
    pixel_idx = dr.arange(mi.UInt32, nb_rays)

    rays = generate_rays(render_res).wi
    times = dr.linspace(mi.Float, 0, 1, time_res)

    time_width = UINT_32_MAX // nb_rays
    time_width = dr.minimum(time_width, time_res)

    if time_width * nb_rays > UINT_32_MAX:
        time_width -= 1

    time_idx = dr.arange(mi.UInt32, time_width)

    pixel_idx_wav, time_idx = dr.meshgrid(pixel_idx, time_idx)

    result = dr.zeros(mi.Float, dr.size_v(mi.Spectrum) * nb_rays)
    si = dr.zeros(mi.SurfaceInteraction3f, nb_rays * time_width)
    for frame_start in range(0, time_res, time_width):
        time_idx_wav = time_idx + frame_start
        active = time_idx_wav < time_res

        si.wi = dr.gather(mi.Vector3f, rays, pixel_idx_wav, active)
        si.time = dr.gather(mi.Float, times, time_idx_wav, active)

        color = plugin.eval(si, active) / time_res
        dr.scatter_add(result, color, pixel_idx_wav, active)


    return mi.TensorXf(dr.ravel(result), (*render_res[::-1], dr.size_v(mi.Spectrum)))

def test01_average_of_average(variants_vec_backends_once):
    if mi.is_polarized:
        pytest.skip('Test must be adapted to polarized rendering.')

    render_res = (64, 32)

    monthly_average = mi.load_dict({
        "type": "timed_sunsky",
        "end_year": 2025,
        "end_month": 2,
        "end_day": 1
    })

    bimonthly_average = mi.load_dict({
        "type": "timed_sunsky",
        "end_year": 2025,
        "end_month": 3,
        "end_day": 1
    })

    samples_per_day = 500

    january_image = generate_average(monthly_average, render_res, 31*samples_per_day)

    # Update emitter to average in february
    monthly_params = mi.traverse(monthly_average)
    monthly_params["start_month"] = 2
    monthly_params["end_month"] = 3
    monthly_params.update()

    february_image = generate_average(monthly_average, render_res, 28*samples_per_day)

    average_of_average = (31 / 59) * january_image + (28 / 59) * february_image
    real_average = generate_average(bimonthly_average, render_res, (31 + 28)*samples_per_day)

    err = dr.mean(dr.abs(average_of_average - real_average) / (dr.abs(real_average) + 0.001), axis=None)

    assert err < 0.01, f"Average of average is incorrect {err = }"

@pytest.mark.parametrize("hour", [10.83, 14.3, 15.24])
def test02_average_of_an_instant(variants_vec_backends_once, hour):
    if mi.is_polarized:
        pytest.skip('Test must be adapted to polarized rendering.')

    render_res = (512, 254)

    point_average = mi.load_dict({
        "type": "timed_sunsky",
        "end_year": 2025,
        "end_day": 2,
        "window_start_time": hour,
        "window_end_time": hour,
        "sun_scale": 0
    })

    sky = mi.load_dict({
        "type": "sunsky",
        "year": 2025,
        "month": 1,
        "day": 1,
        "hour": hour,
        "minute": 0,
        "second": 0,
        "sun_scale": 0
    })

    rays = generate_rays(render_res)
    point_average_res = point_average.eval(rays)
    sky_res = sky.eval(rays)

    err = dr.mean(dr.abs(point_average_res - sky_res) / (dr.abs(sky_res) + 0.001), axis=None)
    assert err < 0.00001, f"Point average does not match original sunsky emitter {err = }"


@pytest.mark.slow
@pytest.mark.parametrize("turb",      [2.2, 4.8, 6.0])
@pytest.mark.parametrize("start_day", [2, 15, 22])
def test03_sky_sampling(variants_vec_backends_once, turb, start_day):
    from .test_sunsky import CroppedSphericalDomain

    timed_sunsky = mi.load_dict({
        "type": "timed_sunsky",
        "start_day": start_day,

        "sun_scale": 0.0,
        "turbidity": turb,
        "albedo": 0.5
    })

    for time in np.linspace(0, 1, 6):
        sample_func, pdf_func = TimedSunskyAdapter(timed_sunsky, time)

        test = mi.chi2.ChiSquareTest(
            domain=CroppedSphericalDomain(),
            pdf_func= pdf_func,
            sample_func= sample_func,
            sample_dim=2,
            sample_count=400_000,
            res=95,
            ires=32
        )

        assert test.run(), f"Chi2 test failed at {time=}"


@pytest.mark.slow
@pytest.mark.parametrize("turb",      [2.2, 4.8, 6.0])
@pytest.mark.parametrize("start_day", [2, 15, 22])
def test04_sun_and_sky_sampling(variants_vec_backends_once, turb, start_day):
    from .test_sunsky import CroppedSphericalDomain

    timed_sunsky = mi.load_dict({
        "type": "timed_sunsky",
        "turbidity": turb,
        "start_day": start_day,
        # Increase the sun aperture to avoid errors with chi2's resolution
        "sun_aperture": 30.0,
        "window_start_time": 9,
        "albedo": 0.5
    })

    for time in np.linspace(0, 1, 6):
        sample_func, pdf_func = TimedSunskyAdapter(timed_sunsky, time)
        test = mi.chi2.ChiSquareTest(
            domain=CroppedSphericalDomain(),
            pdf_func= pdf_func,
            sample_func= sample_func,
            sample_dim=2,
            sample_count=400_000,
            res=101,
            ires=32,
            seed=5
        )

        assert test.run(), f"Chi2 test failed at {time=}"

@pytest.mark.parametrize("turb", [2.0, 4.0, 6.0])
@pytest.mark.parametrize("hour", [10, 12])
@pytest.mark.parametrize("sun_aperture", [0.5358, 5.0])
def test05_complex_sun(variants_vec_spectral,  turb, hour, sun_aperture):
    """
    Compare the irradiance of the classic sunsky with the sun gradients
    and the timed_sunsky with the simplified sun model. This test has quite a large tolerance
    since aligning the sun direction of both models is not perfect.
    """
    if mi.is_polarized:
        pytest.skip('Test must be adapted to polarized rendering.')

    from .test_sunsky import sun_integrand

    sun_cos_cutoff = dr.cos(dr.deg2rad(sun_aperture / 2))

    sunsky = mi.load_dict({
        "type": "sunsky",
        "year": 2025,
        "month": 1,
        "day": 1,
        "hour": hour,
        "sky_scale": 0.0,
        "complex_sun": True,
        "turbidity": turb,

        "sun_aperture": sun_aperture
    })
    sunsky_params = mi.traverse(sunsky)

    timed_sunsky = mi.load_dict({
        "type": "timed_sunsky",
        "start_year": 2025,
        "start_month": 1,
        "start_day": 1,
        "end_year": 2025,
        "end_month": 1,
        "end_day": 1,
        "window_start_time": hour,
        "window_end_time": hour,
        "turbidity": turb,

        "sky_scale": 0,
        "sun_aperture": sun_aperture
    })
    points, weights = mi.quad.gauss_legendre(200)
    sunsky_irrad = sun_integrand(sunsky, points, weights, sunsky_params["sun_direction"], sun_cos_cutoff)
    sunsky_irrad = dr.sum(sunsky_irrad, axis=1)

    timed_irrad = sun_integrand(timed_sunsky, points, weights, sunsky_params["sun_direction"], sun_cos_cutoff)
    timed_irrad = dr.sum(timed_irrad, axis=1)

    assert np.allclose(sunsky_irrad, timed_irrad, rtol=0.2)