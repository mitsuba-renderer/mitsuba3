import pytest
import drjit as dr
import mitsuba as mi

from mitsuba.scalar_rgb.test.util import fresolver_append_path


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


@fresolver_append_path
def create_emitter_and_spectrum(s_key='d65'):
    emitter = mi.load_dict({
        "type": "ply",
        "filename": "resources/data/tests/ply/triangle.ply",
        "emitter" : { "type": "directionalarea", "radiance" : spectrum_dicts[s_key] },
        "to_world" : mi.ScalarTransform4f().translate([10, -1, 2])
    })
    spectrum = mi.load_dict(spectrum_dicts[s_key])
    expanded = spectrum.expand()
    if len(expanded) == 1:
        spectrum = expanded[0]

    return emitter, spectrum


def test01_constructor(variant_scalar_rgb):
    # Check that the shape is properly bound to the emitter
    shape, spectrum = create_emitter_and_spectrum()
    assert shape.emitter().bbox() == shape.bbox()

    # Check that we are not allowed to specify a to_world transform directly in the emitter.
    with pytest.raises(RuntimeError):
        e = mi.load_dict({
            "type" : "directionalarea",
            "to_world" : mi.ScalarTransform4f().translate([5, 0, 0])
        })


@pytest.mark.parametrize("spectrum_key", spectrum_dicts.keys())
def test02_eval(variants_vec_spectral, spectrum_key):
    # Check that eval() return the same values as the 'radiance' spectrum

    shape, spectrum = create_emitter_and_spectrum(spectrum_key)
    emitter = shape.emitter()

    it = dr.zeros(mi.SurfaceInteraction3f, 3)
    assert dr.allclose(emitter.eval(it), spectrum.eval(it))

    # Check that eval returns 0.0 when the direction points into the shape

    it.wi = dr.normalize(mi.ScalarVector3f(0.2, 0.2, -0.5))
    assert dr.allclose(emitter.eval(it), 0.0)


@pytest.mark.parametrize("spectrum_key", spectrum_dicts.keys())
def test03_sample_ray(variants_vec_spectral, spectrum_key):
    # Check the correctness of the sample_ray() method

    shape, spectrum = create_emitter_and_spectrum(spectrum_key)
    emitter = shape.emitter()

    time = 0.5
    wavelength_sample = [0.5, 0.33, 0.1]
    pos_sample = [[0.2, 0.1, 0.2], [0.6, 0.9, 0.2]]
    dir_sample = [[0.4, 0.5, 0.3], [0.1, 0.4, 0.9]]

    # Sample a ray (position, direction, wavelengths) on the emitter
    ray, res = emitter.sample_ray(
        time, wavelength_sample, pos_sample, dir_sample)

    # Sample wavelengths on the spectrum
    it = dr.zeros(mi.SurfaceInteraction3f, 3)
    wav, spec = spectrum.sample_spectrum(it, mi.sample_shifted(wavelength_sample))

    # Sample a position on the shape
    ps = shape.sample_position(time, pos_sample)

    assert dr.allclose(res, spec * shape.surface_area())
    assert dr.allclose(ray.time, time)
    assert dr.allclose(ray.wavelengths, wav)
    assert dr.allclose(ray.o, ps.p, atol=1e-3)
    assert dr.allclose(ray.d, ps.n)


@pytest.mark.parametrize("spectrum_key", spectrum_dicts.keys())
def test04_sample_direction(variants_vec_spectral, spectrum_key):
    # Check the correctness of the sample_direction(), pdf_direction(), and eval_direction() methods

    shape, spectrum = create_emitter_and_spectrum(spectrum_key)
    emitter = shape.emitter()

    # Direction sampling is conditioned on a sampled position
    it = dr.zeros(mi.SurfaceInteraction3f, 3)
    it.p = [[0.2, 0.1, 0.2], [0.6, -0.9, 0.2],
            [0.4, 0.9, -0.2]]  # Some positions
    it.time = 1.0

    # Sample direction on the emitter
    samples = [[0.4, 0.5, 0.3], [0.1, 0.4, 0.9]]
    ds, res = emitter.sample_direction(it, samples)

    assert dr.allclose(ds.pdf, 0)
    assert dr.allclose(ds.pdf, emitter.pdf_direction(it, ds))
    assert dr.allclose(ds.d, 0)
    assert dr.allclose(ds.time, 0)
    assert dr.allclose(res, 0)
