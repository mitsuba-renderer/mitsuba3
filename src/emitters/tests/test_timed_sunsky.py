import pytest

import drjit as dr
import mitsuba as mi

UINT_32_MAX = (1 << 32) - 1

def TimedSunskyAdapter_(plugin_dict):
    """
    Extracts the sampling and pdf functions of a TimedSunskyEmitter
    It also adds a varying time in order to get more code coverage
    :param plugin_dict: TimedSunskyEmitter under it's mitsuba dictionary format
    :return: Sampling function and pdf function
    """
    def sample_functor(sample, *args):
        n = dr.width(sample)
        plugin = mi.load_dict(plugin_dict)
        si = dr.zeros(mi.Interaction3f)
        si.time = dr.linspace(mi.Float, 0, 1, n)
        ds, w = plugin.sample_direction(si, sample)
        return ds.d

    def pdf_functor(wo, *args):
        n = dr.width(wo)
        plugin = mi.load_dict(plugin_dict)
        si = dr.zeros(mi.Interaction3f)
        ds = dr.zeros(mi.DirectionSample3f)
        ds.d = wo
        ds.time = dr.linspace(mi.Float, 0, 1, n)

        return plugin.pdf_direction(si, ds)

    return sample_functor, pdf_functor

TimedSunskyAdapter = TimedSunskyAdapter_ if False else \
                            lambda dict_args: mi.chi2.EmitterAdapter("timed_sunsky", dict_args)

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
    times = mi.PCG32(size=time_res).next_float32()

    time_width = UINT_32_MAX // nb_rays
    time_width = dr.minimum(time_width, time_width)

    if time_width * nb_rays > UINT_32_MAX:
        time_width -= 1

    time_idx = dr.arange(mi.UInt32, time_width)

    pixel_idx_wav, time_idx = dr.meshgrid(pixel_idx, time_idx)

    result = dr.zeros(mi.Color3f, nb_rays)
    si = dr.zeros(mi.SurfaceInteraction3f, nb_rays * time_width)
    for frame_start in range(0, time_res, time_width):
        time_idx_wav = time_idx + frame_start
        active = time_idx_wav < time_res

        si.wi = dr.gather(mi.Vector3f, rays, pixel_idx_wav, active)
        si.time = dr.gather(mi.Float, times, time_idx_wav, active)

        color = plugin.eval(si, active) / time_res
        dr.scatter_add(result, color, pixel_idx_wav, active)


    return mi.TensorXf(dr.ravel(result), (*render_res, 3))

def TODO_average_of_average(variants_vec_backends_once):
    render_res = (512, 256)

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

    january_image = generate_average(monthly_average, render_res, 31 * 500)

    # Update emitter to average in february
    monthly_params = mi.traverse(monthly_average)
    monthly_params["start_month"] = 2
    monthly_params["end_month"] = 3
    monthly_params.update()

    february_image = generate_average(monthly_average, render_res, 28 * 500)

    average_of_average = (31 / 59) * january_image + (28 / 59) * february_image
    real_average = generate_average(bimonthly_average, render_res, (31 + 28) * 500)

    err = dr.mean(dr.abs(average_of_average - real_average) / (dr.abs(real_average) + 0.001), axis=None)

    assert err < 0.0001, f"Average of average is incorrect {err = }"

@pytest.mark.parametrize("hour", [10, 14.3])
def test02_average_of_an_instant(variants_vec_backends_once, hour):
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

    # We cut the image in two, removing the 2-pixel strip around the horizon
    # In order to avoid issues with texture interpolation

    half_image_idx_top = dr.arange(mi.UInt32, render_res[0] * (render_res[1]//2 - 1))
    sky_top = dr.gather(mi.Spectrum, sky_res, half_image_idx_top)
    point_average_top = dr.gather(mi.Spectrum, point_average_res, half_image_idx_top)

    half_image_idx_bot = half_image_idx_top + render_res[0] * (render_res[1]//2 + 1)
    sky_bottom = dr.gather(mi.Spectrum, sky_res, half_image_idx_bot)
    point_average_bottom = dr.gather(mi.Spectrum, point_average_res, half_image_idx_bot)

    err = dr.mean(dr.abs(point_average_top - sky_top) / (dr.abs(sky_top) + 0.001), axis=None)
    assert err < 0.003, f"Top portion of the image does not match reference {err = }"

    err = dr.mean(dr.abs(point_average_bottom - sky_bottom) / (dr.abs(sky_bottom) + 0.001), axis=None)
    assert err < 0.0001, f"Bottom portion of the image does not match reference {err = }"

@pytest.mark.slow
@pytest.mark.parametrize("turb",      [2.2, 4.8, 6.0])
@pytest.mark.parametrize("start_day", [2, 15, 22])
def test03_sky_sampling(variants_vec_backends_once, turb, start_day):
    from .test_sunsky import CroppedSphericalDomain

    timed_sunsky = {
        "type": "timed_sunsky",
        "start_day": start_day,

        "sun_scale": 0.0,
        "turbidity": turb,
        "albedo": 0.5
    }

    sample_func, pdf_func = TimedSunskyAdapter(timed_sunsky)
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
@pytest.mark.parametrize("start_day", [2, 15, 22])
def test04_sun_and_sky_sampling(variants_vec_backends_once, turb, start_day):
    from .test_sunsky import CroppedSphericalDomain

    timed_sunsky = {
        "type": "timed_sunsky",
        "turbidity": turb,
        "start_day": start_day,
        # Increase the sun aperture to avoid errors with chi2's resolution
        "sun_aperture": 30.0,
        "albedo": 0.5
    }

    sample_func, pdf_func = TimedSunskyAdapter(timed_sunsky)
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
