import mitsuba
import pytest
import enoki as ek


def test01_cie1931(variant_scalar_rgb):
    from mitsuba.core import cie1931_xyz, cie1931_y
    """CIE 1931 observer"""
    XYZw = cie1931_xyz(600)
    assert ek.allclose(XYZw, [1.0622, 0.631, 0.000], atol=1e-3)

    Y = cie1931_y(600)
    assert ek.allclose(Y, 0.631)


def test02_d65(variant_scalar_spectral):
    """d65: Spot check the model in a few places, the chi^2 test will ensure
    that sampling works."""

    from mitsuba.core import load_string
    from mitsuba.render import PositionSample3f, SurfaceInteraction3f

    d65 = load_string("<spectrum version='2.0.0' type='d65'/>").expand()[0]
    ps = PositionSample3f()

    assert ek.allclose(d65.eval(SurfaceInteraction3f(ps, [350, 456, 700, 840])),
                       ek.scalar.Array4f([0, 117.49, 71.6091, 0]) / 10568.0)


def test03_blackbody(variant_scalar_spectral):
    """blackbody: Spot check the model in a few places, the chi^2 test will
    ensure that sampling works."""

    from mitsuba.core import load_string
    from mitsuba.render import PositionSample3f, SurfaceInteraction3f

    bb = load_string("""<spectrum version='2.0.0' type='blackbody'>
        <float name='temperature' value='5000'/>
    </spectrum>""")
    ps = PositionSample3f()
    assert ek.allclose(bb.eval(SurfaceInteraction3f(ps, [350, 456, 700, 840])),
                       [0, 10997.9, 11812, 0])


def test04_srgb_d65(variant_scalar_spectral, np_rng):
    """srgb_d65 emitters should evaluate to the product of D65 and sRGB spectra,
    with intensity factored out when evaluating the sRGB model."""

    from mitsuba.core import load_string, MTS_WAVELENGTH_SAMPLES
    from mitsuba.render import PositionSample3f, SurfaceInteraction3f
    import numpy as np


    wavelengths = np.linspace(300, 800, MTS_WAVELENGTH_SAMPLES)
    wavelengths += (10 * np_rng.uniform(size=wavelengths.shape)).astype(int)
    d65 = load_string("<spectrum version='2.0.0' type='d65'/>").expand()[0]

    ps = PositionSample3f()
    d65_eval = d65.eval(SurfaceInteraction3f(ps, wavelengths))

    for color in [[1, 1, 1], [0.1, 0.2, 0.3], [34, 0.1, 62],
                  [0.001, 0.02, 11.4]]:
        intensity  = (np.max(color) * 2.0) or 1.0
        normalized = np.array(color) / intensity

        srgb_d65 = load_string("""
            <spectrum version="2.0.0" type="srgb_d65">
                <rgb name="color" value="{}"/>
            </spectrum>
        """.format(', '.join(map(str, color))))

        srgb = load_string("""
            <spectrum version="2.0.0" type="srgb">
                <rgb name="color" value="{}"/>
            </spectrum>
        """.format(', '.join(map(str, normalized))))


        assert ek.allclose(srgb_d65.eval(SurfaceInteraction3f(ps, wavelengths)),
                           d65_eval * intensity * srgb.eval(SurfaceInteraction3f(ps, wavelengths)), atol=1e-5)


def test05_sample_rgb_spectrum(variant_scalar_spectral):
    """rgb_spectrum: Spot check the model in a few places, the chi^2 test will
    ensure that sampling works."""

    from mitsuba.core import sample_rgb_spectrum, pdf_rgb_spectrum
    from mitsuba.core import MTS_CIE_MIN, MTS_CIE_MAX

    def spot_check(sample, expected_wav, expected_weight):
        wav, weight = sample_rgb_spectrum(sample)
        pdf         = pdf_rgb_spectrum(wav)
        assert ek.allclose(wav, expected_wav)
        assert ek.allclose(weight, expected_weight)
        assert ek.allclose(pdf, 1.0 / weight)

    if (MTS_CIE_MIN == 360 and MTS_CIE_MAX == 830):
        spot_check(0.1, 424.343, 465.291)
        spot_check(0.5, 545.903, 254.643)
        spot_check(0.8, 635.381, 400.432)
        # PDF is zero outside the range
        assert pdf_rgb_spectrum(MTS_CIE_MIN - 10.0) == 0.0
        assert pdf_rgb_spectrum(MTS_CIE_MAX + 0.5)  == 0.0


def test06_rgb2spec_fetch_eval_mean(variant_scalar_spectral):
    from mitsuba.render import srgb_model_fetch, srgb_model_eval, srgb_model_mean
    from mitsuba.core import MTS_CIE_MIN, MTS_CIE_MAX, MTS_WAVELENGTH_SAMPLES
    import numpy as np

    rgb_values = np.array([
        [0, 0, 0],
        [0.0129831, 0.0129831, 0.0129831],
        [0.0241576, 0.0241576, 0.0241576],
        [0.0423114, 0.0423114, 0.0423114],
        [0.0561285, 0.0561285, 0.0561285],
        [0.0703601, 0.0703601, 0.0703601],
        [0.104617, 0.104617, 0.104617],
        [0.171441, 0.171441, 0.171441],
        [0.242281, 0.242281, 0.242281],
        [0.814846, 0.814846, 0.814846],
        [0.913098, 0.913098, 0.913098],
        [0.930111, 0.930111, 0.930111],
        [0.955973, 0.955973, 0.955973],
        [0.973445, 0.973445, 0.973445],
        [0.982251, 0.982251, 0.982251],
        [0.991102, 0.991102, 0.991102],
        [1, 1, 1],
    ])
    wavelengths = np.linspace(MTS_CIE_MIN, MTS_CIE_MAX, MTS_WAVELENGTH_SAMPLES)

    for i in range(rgb_values.shape[0]):
        rgb = rgb_values[i, :]
        assert np.all((rgb >= 0.0) & (rgb <= 1.0)), "Invalid RGB attempted"
        coeff = srgb_model_fetch(rgb)
        mean  = srgb_model_mean(coeff)
        value = srgb_model_eval(coeff, wavelengths)

        assert not ek.any(ek.isnan(coeff)), "{} => coeff = {}".format(rgb, coeff)
        assert not ek.any(ek.isnan(mean)),  "{} => mean = {}".format(rgb, mean)
        assert not ek.any(ek.isnan(value)), "{} => value = {}".format(rgb, value)


def test07_cie1931_srgb_roundtrip(variants_vec_spectral):
    from mitsuba.core import Float, Spectrum
    from mitsuba.core import spectrum_to_xyz, spectrum_to_srgb, xyz_to_srgb, srgb_to_xyz

    n = 50
    wavelengths = Spectrum(
        ek.linspace(Float, 100, 1000, n),
        ek.linspace(Float, 400, 500, n),
        ek.linspace(Float, 100, 500, n),
        ek.linspace(Float, 500, 1000, n)
    )

    srgb = spectrum_to_srgb(Spectrum(1.0), wavelengths)
    xyz = spectrum_to_xyz(Spectrum(1.0), wavelengths)

    assert ek.allclose(xyz_to_srgb(xyz), srgb)
    assert ek.allclose(srgb_to_xyz(srgb), xyz, atol=1e-6)
