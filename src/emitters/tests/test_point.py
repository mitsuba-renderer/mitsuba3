import mitsuba
import pytest
import drjit as dr


spectrum_strings = {
    'd65':
    """<spectrum version='2.0.0' name="intensity" type='d65'/>""",
    'regular':
    """<spectrum version='2.0.0' name="intensity" type='regular'>
           <float name="lambda_min" value="500"/>
           <float name="lambda_max" value="600"/>
           <string name="values" value="1, 2"/>
       </spectrum>""",
}


def create_emitter_and_spectrum(pos, s_key='d65'):
    from mitsuba.core import load_string
    emitter = load_string("""<emitter version='2.0.0' type='point'>
                                <point name='position' x='{x}' y='{y}' z='{z}'/>
                                {s}
                             </emitter>""".format(x=pos[0], y=pos[1], z=pos[2],
                                                  s=spectrum_strings[s_key]))
    spectrum = load_string(spectrum_strings[s_key])
    expanded = spectrum.expand()
    if len(expanded) == 1:
        spectrum = expanded[0]

    return emitter, spectrum


@pytest.mark.parametrize("spectrum_key", spectrum_strings.keys())
def test01_point_sample_ray(variants_vec_spectral, spectrum_key):
    # Check the correctness of the sample_ray() method

    from mitsuba.core import Vector3f, warp, sample_shifted
    from mitsuba.render import SurfaceInteraction3f

    emitter_pos = [10, -1, 2]
    emitter, spectrum = create_emitter_and_spectrum(emitter_pos, spectrum_key)

    time = 0.5
    wavelength_sample = [0.5, 0.33, 0.1]
    dir_sample = [[0.4, 0.5, 0.3], [0.1, 0.4, 0.9]]
    pos_sample = dir_sample  # not being used anyway

    # Sample a ray (position, direction, wavelengths) on the emitter
    ray, res = emitter.sample_ray(time, wavelength_sample, pos_sample, dir_sample)

    # Sample wavelengths on the spectrum
    it = dr.zero(SurfaceInteraction3f, 3)
    wav, spec = spectrum.sample_spectrum(it, sample_shifted(wavelength_sample))

    assert dr.allclose(res, spec * 4 * dr.Pi)
    assert dr.allclose(ray.time, time)
    assert dr.allclose(ray.wavelengths, wav)
    assert dr.allclose(ray.d, warp.square_to_uniform_sphere(dir_sample))
    assert dr.allclose(ray.o, Vector3f(emitter_pos))


@pytest.mark.parametrize("spectrum_key", spectrum_strings.keys())
def test02_point_sample_direction(variant_scalar_spectral, spectrum_key):
    # Check the correctness of the sample_direction() method

    from mitsuba.render import SurfaceInteraction3f

    emitter_pos = [10, -1, 2]
    emitter, spectrum = create_emitter_and_spectrum(emitter_pos, spectrum_key)

    # Direction sampling
    it = dr.zero(SurfaceInteraction3f)
    it.p = [0.0, -2.0, 4.5]  # Some position
    it.time = 0.3

    # Direction from the position to the point emitter
    d = -it.p + emitter_pos
    dist = dr.norm(d)
    d /= dist

    # Sample a direction on the emitter
    sample = [0.1, 0.5]
    ds, res = emitter.sample_direction(it, sample)

    assert ds.time == it.time
    assert ds.pdf == 1.0
    assert ds.delta
    assert dr.allclose(ds.d, d)

    # Evaluate the spectrum
    spec = spectrum.eval(it) / (dist**2)
    assert dr.allclose(res, spec)


@pytest.mark.parametrize("spectrum_key", spectrum_strings.keys())
def test03_point_sample_direction_vec(variants_vec_spectral, spectrum_key):
    from mitsuba.render import SurfaceInteraction3f

    emitter_pos = [50, -1, 2]
    emitter, spectrum = create_emitter_and_spectrum(emitter_pos, spectrum_key)

    # Direction sampling
    it = dr.zero(SurfaceInteraction3f, 3)
    it.p = [[0.0, 0.0, 0.0], [-2.0, 0.0, -2.0],
            [4.5, 4.5, 0.0]]  # Some positions
    it.time = [0.3, 0.3, 0.3]

    # Direction from the position to the point emitter
    d = -it.p + emitter_pos
    dist = dr.norm(d)
    d /= dist

    # Sample direction on the emitter
    sample = [0.1, 0.5]
    ds, res = emitter.sample_direction(it, sample)

    assert dr.all(ds.time == it.time)
    assert dr.all(ds.pdf == 1.0)
    assert dr.all(ds.delta)
    assert dr.allclose(ds.d, d, atol=1e-3)
    assert emitter.pdf_direction(it, ds) == 0

    # Evaluate the spectrum
    spec = spectrum.eval(it) / (dist**2)
    assert dr.allclose(res, spec)
    assert dr.allclose(emitter.eval_direction(it, ds), spec)
