import mitsuba
import pytest
import enoki as ek
from enoki.dynamic import Float32 as Float


spectrum_strings = {
    'd65':
    """<spectrum version='2.0.0' name="radiance" type='d65'/>""",
    'regular':
    """<spectrum version='2.0.0' name="radiance" type='regular'>
           <float name="lambda_min" value="500"/>
           <float name="lambda_max" value="600"/>
           <string name="values" value="1, 2"/>
       </spectrum>""",
}


def create_emitter_and_spectrum(s_key='d65'):
    from mitsuba.core.xml import load_string
    emitter = load_string("""<emitter version='2.0.0' type='constant'>
                                {s}
                             </emitter>""".format(s=spectrum_strings[s_key]))
    spectrum = load_string(spectrum_strings[s_key])
    expanded = spectrum.expand()
    if len(expanded) == 1:
        spectrum = expanded[0]

    return emitter, spectrum


@pytest.mark.parametrize("spectrum_key", spectrum_strings.keys())
def test01_eval(variant_packet_spectral, spectrum_key):
    # Check the correctness of the eval() method

    from mitsuba.core import warp
    from mitsuba.core.math import InvFourPi
    from mitsuba.render import SurfaceInteraction3f

    emitter, spectrum = create_emitter_and_spectrum(spectrum_key)

    it = SurfaceInteraction3f.zero()
    assert ek.allclose(emitter.eval(it), spectrum.eval(it))


@pytest.mark.parametrize("spectrum_key", spectrum_strings.keys())
def test02_sample_ray(variant_packet_spectral, spectrum_key):
    # Check the correctness of the sample_ray() method

    from mitsuba.core import Frame3f, warp, sample_shifted
    from mitsuba.render import SurfaceInteraction3f

    emitter, spectrum = create_emitter_and_spectrum(spectrum_key)

    time = 0.5
    wavelength_sample = [0.5, 0.33, 0.1]
    pos_sample = [[0.2, 0.1, 0.2], [0.6, 0.9, 0.2]]
    dir_sample = [[0.4, 0.5, 0.3], [0.1, 0.4, 0.9]]

    # Sample a ray (position, direction, wavelengths) on the emitter
    ray, res = emitter.sample_ray(time, wavelength_sample, pos_sample, dir_sample)

    # Sample wavelengths on the spectrum
    it = SurfaceInteraction3f.zero(3)
    wav, spec = spectrum.sample(it, sample_shifted(wavelength_sample))

    assert ek.allclose(res, spec * 4 * ek.pi * ek.pi)
    assert ek.allclose(ray.time, time)
    assert ek.allclose(ray.wavelengths, wav)
    assert ek.allclose(ray.o, warp.square_to_uniform_sphere(pos_sample))
    assert ek.allclose(
        ray.d, Frame3f(-ray.o).to_world(warp.square_to_cosine_hemisphere(dir_sample)))


def test03_sample_direction(variant_packet_spectral):
    # Check the correctness of the sample_direction() and pdf_direction() methods

    from mitsuba.core import warp
    from mitsuba.core.math import InvFourPi
    from mitsuba.render import SurfaceInteraction3f

    emitter, spectrum = create_emitter_and_spectrum()

    it = SurfaceInteraction3f.zero(3)
    # Some positions inside the unit sphere
    it.p = [[-0.5, 0.3, -0.1], [0.8, -0.3, -0.2], [-0.2, 0.6, -0.6]]
    it.time = 1.0

    # Sample direction on the emitter
    samples = [[0.4, 0.5, 0.3], [0.1, 0.4, 0.9]]
    ds, res = emitter.sample_direction(it, samples)

    assert ek.allclose(ds.pdf, InvFourPi)
    assert ek.allclose(ds.d, warp.square_to_uniform_sphere(samples))
    assert ek.allclose(emitter.pdf_direction(it, ds), InvFourPi)
    assert ek.allclose(ds.time, it.time)

    # Evaluate the spectrum (divide by the pdf)
    spec = spectrum.eval(it) / warp.square_to_uniform_sphere_pdf(ds.d)
    assert ek.allclose(res, spec)
