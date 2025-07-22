import drjit as dr
import mitsuba as mi


def generate_rays(render_res):
    phis, thetas = dr.meshgrid(
        dr.linspace(mi.Float, 0.0, dr.two_pi, render_res[0], endpoint=False),
        dr.linspace(mi.Float, 0, dr.pi, render_res[1], endpoint=False)
    )

    sp, cp = dr.sincos(phis)
    st, ct = dr.sincos(thetas)

    si = mi.SurfaceInteraction3f()
    si.wi = mi.Vector3f(cp*st, sp*st, ct)

    return si


def test01_chi2(variants_vec_backends_once):
    avg_sunsky = {
        "type": "avg_sunsky",
        "time_resolution": 50,
        "end_year": 2025,
        "end_month": 1,
        "end_day": 5
    }

    sample_func, pdf_func = mi.chi2.EmitterAdapter("avg_sunsky", avg_sunsky)
    test = mi.chi2.ChiSquareTest(
        domain=mi.chi2.SphericalDomain(),
        pdf_func= pdf_func,
        sample_func= sample_func,
        sample_dim=2,
        sample_count=200_000,
        res=64,
        ires=32
    )

    assert test.run(), "Chi2 test failed"

def test02_average_of_average(variants_vec_backends_once):
    render_res = (512, 256)

    monthly_average = mi.load_dict({
        "type": "avg_sunsky",
        "time_resolution": 20,
        "end_year": 2025,
        "end_month": 2,
        "end_day": 1
    })

    bimonthly_average = mi.load_dict({
        "type": "avg_sunsky",
        "time_resolution": 20,
        "end_year": 2025,
        "end_month": 3,
        "end_day": 1
    })

    rays = generate_rays(render_res)

    january_image = monthly_average.eval(rays)

    monthly_params = mi.traverse(monthly_average)
    monthly_params["start_month"] = 2
    monthly_params["end_month"] = 3
    monthly_params.update()

    february_image = monthly_average.eval(rays)

    average_of_average = (31 / 59) * january_image + (28 / 59) * february_image
    real_average = bimonthly_average.eval(rays)

    err = dr.mean(dr.abs(average_of_average - real_average) / (dr.abs(real_average) + 0.001), axis=None)

    assert err < 0.0001, f"Average of average is incorrect {err = }"


