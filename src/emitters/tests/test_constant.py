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


def create_emitter_and_spectrum(s_key='d65'):
    emitter = mi.load_dict({
        "type" : "constant",
        "radiance" : spectrum_dicts[s_key]
    })
    spectrum = mi.load_dict(spectrum_dicts[s_key])
    expanded = spectrum.expand()
    if len(expanded) == 1:
        spectrum = expanded[0]

    return emitter, spectrum


@pytest.mark.parametrize("spectrum_key", spectrum_dicts.keys())
def test01_eval(variants_vec_spectral, spectrum_key):
    # Check the correctness of the eval() method

    emitter, spectrum = create_emitter_and_spectrum(spectrum_key)

    it = dr.zeros(mi.SurfaceInteraction3f)
    assert dr.allclose(emitter.eval(it), spectrum.eval(it))


@pytest.mark.parametrize("spectrum_key", spectrum_dicts.keys())
def test02_sample_ray(variants_vec_spectral, spectrum_key):
    # Check the correctness of the sample_ray() method

    emitter, spectrum = create_emitter_and_spectrum(spectrum_key)

    time = 0.5
    wavelength_sample = [0.5, 0.33, 0.1]
    pos_sample = [[0.2, 0.1, 0.2], [0.6, 0.9, 0.2]]
    dir_sample = [[0.4, 0.5, 0.3], [0.1, 0.4, 0.9]]

    # Sample a ray (position, direction, wavelengths) on the emitter
    ray, res = emitter.sample_ray(time, wavelength_sample, pos_sample, dir_sample)

    # Sample wavelengths on the spectrum
    it = dr.zeros(mi.SurfaceInteraction3f, 3)
    wav, spec = spectrum.sample_spectrum(it, mi.sample_shifted(wavelength_sample))

    assert dr.allclose(res, spec * 4 * dr.pi * dr.pi)
    assert dr.allclose(ray.time, time)
    assert dr.allclose(ray.wavelengths, wav)
    assert dr.allclose(ray.o, mi.warp.square_to_uniform_sphere(pos_sample))
    assert dr.allclose(
        ray.d, mi.Frame3f(-ray.o).to_world(mi.warp.square_to_cosine_hemisphere(dir_sample)))


def test03_sample_direction(variants_vec_spectral):
    # Check the correctness of the sample_direction() and pdf_direction() methods

    emitter, spectrum = create_emitter_and_spectrum()

    it = dr.zeros(mi.SurfaceInteraction3f, 3)
    # Some positions inside the unit sphere
    it.p = [[-0.5, 0.3, -0.1], [0.8, -0.3, -0.2], [-0.2, 0.6, -0.6]]
    it.time = 1.0

    # Sample direction on the emitter
    samples = [[0.4, 0.5, 0.3], [0.1, 0.4, 0.9]]
    ds, res = emitter.sample_direction(it, samples)

    assert dr.allclose(ds.pdf, dr.inv_four_pi)
    assert dr.allclose(ds.d, mi.warp.square_to_uniform_sphere(samples))
    assert dr.allclose(emitter.pdf_direction(it, ds), dr.inv_four_pi)
    assert dr.allclose(ds.time, it.time)

    # Evaluate the spectrum (divide by the pdf)
    spec = spectrum.eval(it) / mi.warp.square_to_uniform_sphere_pdf(ds.d)
    assert dr.allclose(res, spec)
