import pytest
import drjit as dr
import mitsuba as mi
import numpy as np


spectrum_dicts = {
    "d65": {
        "type": "d65",
    },
    "regular": {
        "type": "regular",
        "wavelength_min": 500,
        "wavelength_max": 600,
        "values": "1, 2",
    },
}


def make_spectrum(spectrum_key="d65"):
    spectrum = mi.load_dict(spectrum_dicts[spectrum_key])
    expanded = spectrum.expand()

    if len(expanded) == 1:
        spectrum = expanded[0]

    return spectrum


def make_emitter(direction=None, spectrum_key="d65", angular_diameter=None):
    emitter_dict = {
        "type": "astroobject",
        "irradiance": spectrum_dicts[spectrum_key],
    }

    if direction is not None:
        emitter_dict["direction"] = direction

    if angular_diameter is not None:
        emitter_dict["angular_diameter"] = angular_diameter

    return mi.load_dict(emitter_dict)


def test_construct(variant_scalar_rgb):
    # Test if the emitter can be constructed as intended
    emitter = make_emitter()
    assert not emitter.bbox().valid()  # Degenerate bounding box
    assert dr.allclose(
        emitter.world_transform().matrix,
        [
            [1, 0, 0, 0],
            [0, 1, 0, 0],
            [0, 0, 1, 0],
            [0, 0, 0, 1],
        ],
    )  # Identity transform matrix by default

    # Check transform setup correctness
    emitter = make_emitter(direction=[0, 0, -1])
    assert dr.allclose(
        emitter.world_transform().matrix,
        [
            [0, 1, 0, 0],
            [1, 0, 0, 0],
            [0, 0, -1, 0],
            [0, 0, 0, 1],
        ],
    )

    # Check angular diameter setup
    emitter = make_emitter(angular_diameter=15.0)
    with pytest.raises(RuntimeError):
        emitter = make_emitter(angular_diameter=0.0)
    with pytest.raises(RuntimeError):
        emitter = make_emitter(angular_diameter=180.0)


@pytest.mark.parametrize("spectrum_key", spectrum_dicts.keys())
def test_eval(variant_scalar_spectral, spectrum_key):
    # Check correctness of the eval() method

    direction = mi.ScalarVector3f([0, 0, -1])
    emitter = make_emitter(direction, spectrum_key, angular_diameter=1.0)

    it = dr.zeros(mi.SurfaceInteraction3f)
    it.wavelengths = [500, 525, 575, 600]
    it.p = [0, 0, 0]

    # Incident direction in the illuminated direction
    it.wi = [0, 0, 1]
    assert not dr.allclose(emitter.eval(it), 0.0)

    # Incident direction in the illuminated cone
    it.wi = dr.normalize(mi.ScalarVector3f([0.001, 0, 1]))
    assert not dr.allclose(emitter.eval(it), 0.0)

    # Incident direction outside the illuminated cone
    it.wi = dr.normalize(mi.ScalarVector3f([0.01, 0, 1]))
    assert dr.allclose(emitter.eval(it), 0.0)


@pytest.mark.parametrize("spectrum_key", spectrum_dicts.keys())
@pytest.mark.parametrize("direction", [[0, 0, -1], [1, 1, 1], [0, 0, 1]])
@pytest.mark.parametrize("angular_diameter", np.arange(0.1, 51, 10.0))
def test_sample_direction(
    variant_scalar_spectral, spectrum_key, direction, angular_diameter
):
    # Check correctness of sample_direction() and pdf_direction() methods

    direction = mi.Vector3f(direction)
    emitter = make_emitter(direction, spectrum_key, angular_diameter)
    spectrum = make_spectrum(spectrum_key)

    it = dr.zeros(mi.SurfaceInteraction3f)
    # Some position inside the unit sphere (i.e. within the emitter's default bounding sphere)
    it.p = [-0.5, 0.3, -0.1]
    it.time = 1.0

    d = -direction / dr.norm(direction)

    # Sample direction
    samples = [0.85, 0.13]
    ds, res = emitter.sample_direction(it, samples)

    # Direction should point *towards* the illuminated direction

    assert dr.dot(ds.d, d) > dr.cos(dr.deg2rad(angular_diameter / 2))
    assert dr.allclose(emitter.pdf_direction(it, ds), ds.pdf)
    assert dr.allclose(ds.time, it.time)

    # Check spectrum (no attenuation vs distance)
    spec = spectrum.eval(it)
    assert dr.allclose(res, spec)
