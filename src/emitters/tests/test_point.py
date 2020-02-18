import mitsuba
import pytest
import enoki as ek


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
    from mitsuba.core.xml import load_string
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
def test01_point_sample_ray(variant_packet_spectral, spectrum_key):
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
    it = SurfaceInteraction3f.zero(3)
    wav, spec = spectrum.sample(it, sample_shifted(wavelength_sample))

    assert ek.allclose(res, spec * 4 * ek.pi)
    assert ek.allclose(ray.time, time)
    assert ek.allclose(ray.wavelengths, wav)
    assert ek.allclose(ray.d, warp.square_to_uniform_sphere(dir_sample))
    assert ek.allclose(ray.o, Vector3f(emitter_pos))


@pytest.mark.parametrize("spectrum_key", spectrum_strings.keys())
def test02_point_sample_direction(variant_scalar_spectral, spectrum_key):
    # Check the correctness of the sample_direction() method

    from mitsuba.render import SurfaceInteraction3f

    emitter_pos = [10, -1, 2]
    emitter, spectrum = create_emitter_and_spectrum(emitter_pos, spectrum_key)

    # Direction sampling
    it = SurfaceInteraction3f.zero()
    it.p = [0.0, -2.0, 4.5]  # Some position
    it.time = 0.3

    # Direction from the position to the point emitter
    d = -it.p + emitter_pos
    dist = ek.norm(d)
    d /= dist

    # Sample a direction on the emitter
    sample = [0.1, 0.5]
    ds, res = emitter.sample_direction(it, sample)

    assert ds.time == it.time
    assert ds.pdf == 1.0
    assert ds.delta
    assert ek.allclose(ds.d, d)

    # Evalutate the spectrum
    spec = spectrum.eval(it) / (dist**2)
    assert ek.allclose(res, spec)


@pytest.mark.parametrize("spectrum_key", spectrum_strings.keys())
def test03_point_sample_direction_vec(variant_packet_spectral, spectrum_key):
    from mitsuba.render import SurfaceInteraction3f

    emitter_pos = [10, -1, 2]
    emitter, spectrum = create_emitter_and_spectrum(emitter_pos, spectrum_key)

    # Direction sampling
    it = SurfaceInteraction3f.zero(3)
    it.p = [[0.0, 0.0, 0.0], [-2.0, 0.0, -2.0],
            [4.5, 4.5, 0.0]]  # Some positions
    it.time = [0.3, 0.3, 0.3]

    # Direction from the position to the point emitter
    d = -it.p + emitter_pos
    dist = ek.norm(d)
    d /= dist

    # Sample direction on the emitter
    sample = [0.1, 0.5]
    ds, res = emitter.sample_direction(it, sample)

    assert ek.all(ds.time == it.time)
    assert ek.all(ds.pdf == 1.0)
    assert ek.all(ds.delta)
    assert ek.allclose(ds.d, d / ek.norm(d))

    # Evalutate the spectrum
    spec = spectrum.eval(it) / (dist**2)
    assert ek.allclose(res, spec)
