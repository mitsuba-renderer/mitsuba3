import pytest
import drjit as dr
import mitsuba as mi


def test01_cie1931(variant_scalar_rgb):
    """CIE 1931 observer"""

    XYZw = mi.cie1931_xyz(600)
    assert dr.allclose(XYZw, [1.0622, 0.631, 0.000], atol=1e-3)

    Y = mi.cie1931_y(600)
    assert dr.allclose(Y, 0.631)


def test02_d65(variant_scalar_spectral):
    """d65: Spot check the model in a few places, the chi^2 test will ensure
    that sampling works."""

    d65 = mi.load_dict({ "type" : "d65" })
    ps = mi.PositionSample3f()

    assert dr.allclose(d65.eval(mi.SurfaceInteraction3f(ps, [350, 456, 700, 840])),
                       dr.scalar.Array4f([0, 117.49, 71.6091, 0]) * mi.MI_CIE_D65_NORMALIZATION)


def test03_blackbody(variant_scalar_spectral):
    """blackbody: Spot check the model in a few places, the chi^2 test will
    ensure that sampling works."""

    bb = mi.load_dict({
        "type" : "blackbody",
        "temperature" : 5000
    })
    ps = mi.PositionSample3f()
    assert dr.allclose(bb.eval(mi.SurfaceInteraction3f(ps, [350, 456, 700, 840])),
                       [0, 10997.9, 11812, 0])


def test04_d65(variant_scalar_spectral, np_rng):
    """d65 emitters should evaluate to the product of D65 and sRGB spectra,
    with intensity factored out when evaluating the sRGB model."""

    import numpy as np

    wavelengths = np.linspace(300, 800, mi.MI_WAVELENGTH_SAMPLES)
    wavelengths += (10 * np_rng.uniform(size=wavelengths.shape)).astype(int)
    d65 = mi.load_dict({ "type" : "d65" })

    ps = mi.PositionSample3f()
    d65_eval = d65.eval(mi.SurfaceInteraction3f(ps, wavelengths))

    for color in [[1, 1, 1], [0.1, 0.2, 0.3], [34, 0.1, 62],
                  [0.001, 0.02, 11.4]]:
        intensity  = (np.max(color) * 2.0) or 1.0
        normalized = np.array(color) / intensity

        d65 = mi.load_dict({
            "type" : "d65",
            "color" : mi.Color3f(color)

        })
        srgb = mi.load_dict({
            "type" : "srgb",
            "color" : mi.Color3f(normalized),
        })

        assert dr.allclose(d65.eval(mi.SurfaceInteraction3f(ps, wavelengths)),
                           d65_eval * intensity * srgb.eval(mi.SurfaceInteraction3f(ps, wavelengths)), atol=1e-5)


def test05_sample_rgb_spectrum(variant_scalar_spectral):
    """rgb_spectrum: Spot check the model in a few places, the chi^2 test will
    ensure that sampling works."""

    def spot_check(sample, expected_wav, expected_weight):
        wav, weight = mi.sample_rgb_spectrum(sample)
        pdf         = mi.pdf_rgb_spectrum(wav)
        assert dr.allclose(wav, expected_wav)
        assert dr.allclose(weight, expected_weight)
        assert dr.allclose(pdf, 1.0 / weight)

    if (mi.MI_CIE_MIN == 360 and mi.MI_CIE_MAX == 830):
        spot_check(0.1, 424.343, 465.291)
        spot_check(0.5, 545.903, 254.643)
        spot_check(0.8, 635.381, 400.432)
        # PDF is zero outside the range
        assert mi.pdf_rgb_spectrum(mi.MI_CIE_MIN - 10.0) == 0.0
        assert mi.pdf_rgb_spectrum(mi.MI_CIE_MAX + 0.5)  == 0.0


def test06_rgb2spec_fetch_eval_mean(variant_scalar_spectral):
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
    wavelengths = np.linspace(mi.MI_CIE_MIN, mi.MI_CIE_MAX, mi.MI_WAVELENGTH_SAMPLES)

    for i in range(rgb_values.shape[0]):
        rgb = rgb_values[i, :]
        assert np.all((rgb >= 0.0) & (rgb <= 1.0)), "Invalid RGB attempted"
        coeff = mi.srgb_model_fetch(rgb)
        mean  = mi.srgb_model_mean(coeff)
        value = mi.srgb_model_eval(coeff, wavelengths)

        assert not dr.any(dr.isnan(coeff)), "{} => coeff = {}".format(rgb, coeff)
        assert not dr.any(dr.isnan(mean)),  "{} => mean = {}".format(rgb, mean)
        assert not dr.any(dr.isnan(value)), "{} => value = {}".format(rgb, value)


def test07_cie1931_srgb_roundtrip(variants_vec_spectral):
    n = 50
    wavelengths = mi.Spectrum(
        dr.linspace(mi.Float, 100, 1000, n),
        dr.linspace(mi.Float, 400, 500, n),
        dr.linspace(mi.Float, 100, 500, n),
        dr.linspace(mi.Float, 500, 1000, n)
    )

    srgb = mi.spectrum_to_srgb(mi.Spectrum(1.0), wavelengths)
    xyz = mi.spectrum_to_xyz(mi.Spectrum(1.0), wavelengths)

    assert dr.allclose(mi.xyz_to_srgb(xyz), srgb)
    assert dr.allclose(mi.srgb_to_xyz(srgb), xyz, atol=1e-6)
