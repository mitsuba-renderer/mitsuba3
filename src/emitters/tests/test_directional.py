import pytest
import drjit as dr
import mitsuba as mi


spectrum_dicts = {
    'd65': {
        "type": "d65",
    },
    'regular': {
        "type": "regular",
        "wavelength_min": 500,
        "wavelength_max": 600,
        "values": "1, 2"
    }
}


def make_spectrum(spectrum_key="d65"):
    spectrum = mi.load_dict(spectrum_dicts[spectrum_key])
    expanded = spectrum.expand()

    if len(expanded) == 1:
        spectrum = expanded[0]

    return spectrum


def make_emitter(direction=None, spectrum_key="d65"):
    emitter_dict = {
        "type" : "directional",
        "irradiance" : spectrum_dicts[spectrum_key]
    }

    if direction is not None:
        emitter_dict["direction"] = direction

    return mi.load_dict(emitter_dict)


def test_construct(variant_scalar_rgb):
    # Test if the emitter can be constructed as intended
    emitter = make_emitter()
    assert not emitter.bbox().valid()  # Degenerate bounding box
    assert dr.allclose(
        emitter.world_transform().matrix,
        [[1, 0, 0, 0],
         [0, 1, 0, 0],
         [0, 0, 1, 0],
         [0, 0, 0, 1]]
    )  # Identity transform matrix by default

    # Check transform setup correctness
    emitter = make_emitter(direction=[0, 0, -1])
    assert dr.allclose(
        emitter.world_transform().matrix,
        [[0, 1, 0, 0],
         [1, 0, 0, 0],
         [0, 0, -1, 0],
         [0, 0, 0, 1]]
    )


@pytest.mark.parametrize("spectrum_key", spectrum_dicts.keys())
def test_eval(variant_scalar_spectral, spectrum_key):
    # Check correctness of the eval() method

    direction = mi.Vector3f([0, 0, -1])
    emitter = make_emitter(direction, spectrum_key)

    # Incident direction in the illuminated direction
    wi = [0, 0, 1]
    it = mi.SurfaceInteraction3f()
    it.p = [0, 0, 0]
    it.wi = wi
    assert dr.allclose(emitter.eval(it), 0.)

    # Incident direction off the illuminated direction
    wi = [0, 0, 1.1]
    it = mi.SurfaceInteraction3f()
    it.p = [0, 0, 0]
    it.wi = wi
    assert dr.allclose(emitter.eval(it), 0.)


@pytest.mark.parametrize("spectrum_key", spectrum_dicts.keys())
@pytest.mark.parametrize("direction", [[0, 0, -1], [1, 1, 1], [0, 0, 1]])
def test_sample_direction(variant_scalar_spectral, spectrum_key, direction):
    # Check correctness of sample_direction() and pdf_direction() methods

    direction = mi.Vector3f(direction)
    emitter = make_emitter(direction, spectrum_key)
    spectrum = make_spectrum(spectrum_key)

    it = mi.SurfaceInteraction3f()
    it.wavelengths = [0, 0, 0, 0]

    # Some position inside the unit sphere (i.e. within the emitter's default bounding sphere)
    it.p = [-0.5, 0.3, -0.1]
    it.time = 1.0

    # Sample direction
    samples = [0.85, 0.13]
    ds, res = emitter.sample_direction(it, samples)

    # Direction should point *towards* the illuminated direction
    assert dr.allclose(ds.d, -direction / dr.norm(direction))
    assert dr.allclose(ds.pdf, 1.)
    assert dr.allclose(emitter.pdf_direction(it, ds), 0.)
    assert dr.allclose(ds.time, it.time)

    # Check spectrum (no attenuation vs distance)
    spec = spectrum.eval(it)
    assert dr.allclose(res, spec)

    assert dr.allclose(emitter.eval_direction(it, ds), spec)

@pytest.mark.parametrize("direction", [[0, 0, -1], [1, 1, 1], [0, 0, 1]])
def test_sample_ray(variant_scalar_spectral, direction):
    emitter = make_emitter(direction=direction)
    direction = mi.Vector3f(direction)
    direction = direction / dr.norm(direction)

    time = 1.0
    wavelength_sample = 0.3
    directional_sample = [0.3, 0.2]

    for spatial_sample in [
            [0.85, 0.13],
            [0.16, 0.50],
            [0.00, 1.00],
            [0.32, 0.87],
            [0.16, 0.44],
            [0.17, 0.44],
            [0.22, 0.81],
            [0.12, 0.82],
            [0.99, 0.42],
            [0.72, 0.40],
            [0.01, 0.61],
        ]:
        ray, _ = emitter.sample_ray(
            time, wavelength_sample, spatial_sample, directional_sample)

        # Check that ray direction is what is expected
        assert dr.allclose(ray.d, direction)

        # Check that ray origin is outside of bounding sphere
        # Bounding sphere is centered at world origin and has radius 1 without scene
        assert dr.norm(ray.o) >= 1.
