import math
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


def test_angle(variants_vec_backends_once_rgb):
    # A positive angle spreads the light samples uniformly over the cone
    # while the emitter remains a delta light with pdf one
    import numpy as np

    angle = 10.0
    cos_cutoff = math.cos(math.radians(angle) / 2)
    emitter = mi.load_dict({
        'type': 'directional',
        'direction': [0, 0, -1],
        'irradiance': 2.0,
        'angle': angle,
    })
    assert emitter.flags() == int(mi.EmitterFlags.Infinite
                                  | mi.EmitterFlags.DeltaDirection)

    n = 100000
    sampler = mi.load_dict({'type': 'independent'})
    sampler.seed(0, n)
    it = dr.zeros(mi.Interaction3f, n)
    ds, spec = emitter.sample_direction(it, sampler.next_2d())

    cos = np.array(ds.d.z)
    assert cos.min() >= cos_cutoff - 1e-6
    assert cos.min() < cos_cutoff + 1e-4
    # A uniform cone has mean cosine (1 + cos_cutoff) / 2
    assert cos.mean() == pytest.approx((1 + cos_cutoff) / 2, abs=1e-4)
    assert dr.all(ds.pdf == 1.0)
    assert dr.all(ds.delta)
    assert dr.allclose(spec, 2.0)
    assert dr.allclose(emitter.pdf_direction(it, ds), 0.0)

    # Light tracing rays leave the bounding sphere along cone directions
    ray, _ = emitter.sample_ray(0.0, 0.5, sampler.next_2d(), sampler.next_2d())
    assert dr.all(-ray.d.z >= cos_cutoff - 1e-6)
    assert dr.all(dr.norm(ray.o) >= 1.0 - 1e-6)

    # The angle is exposed and updates the cone
    params = mi.traverse(emitter)
    params['angle'] = 60.0
    params.update()
    ds, _ = emitter.sample_direction(it, sampler.next_2d())
    cos = np.array(ds.d.z)
    assert cos.min() >= math.cos(math.radians(30.0)) - 1e-6
    assert cos.min() < cos_cutoff


def test_invalid_angle(variant_scalar_rgb):
    for angle in (-1.0, 180.0):
        with pytest.raises(RuntimeError):
            mi.load_dict({'type': 'directional', 'angle': angle})
