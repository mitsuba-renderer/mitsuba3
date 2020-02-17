import mitsuba
import pytest
import enoki as ek
from enoki.dynamic import Float32 as Float


def create_emitter_and_spectrum():
    from mitsuba.core.xml import load_string
    emitter = load_string("""<emitter version="2.0.0" type="constant"/>""")
    spectrum = load_string("<spectrum version='2.0.0' type='d65'/>").expand()[0]
    return emitter, spectrum


def test01_sample_ray(variant_packet_spectral):
    from mitsuba.core import Frame3f, warp, sample_shifted
    from mitsuba.render import SurfaceInteraction3f

    emitter, spectrum = create_emitter_and_spectrum()

    time = 0.5
    wavelength_sample = [0.5, 0.33, 0.1]
    pos_sample = [[0.2, 0.1, 0.2], [0.6, 0.9, 0.2]]
    dir_sample = [[0.4, 0.5, 0.3], [0.1, 0.4, 0.9]]

    ray, spec = emitter.sample_ray(time, wavelength_sample, pos_sample, dir_sample)

    it = SurfaceInteraction3f.zero(3)
    wav, res = spectrum.sample(it, sample_shifted(wavelength_sample))

    assert ek.allclose(spec, res * 4 * ek.pi * ek.pi)
    assert ek.allclose(ray.time, time)
    assert ek.allclose(ray.wavelengths, wav)
    assert ek.allclose(ray.o, warp.square_to_uniform_sphere(pos_sample))
    assert ek.allclose(
        ray.d, Frame3f(-ray.o).to_world(warp.square_to_cosine_hemisphere(dir_sample)))


def test02_sample_direction(variant_packet_rgb):
    from mitsuba.core import warp
    from mitsuba.core.math import InvFourPi
    from mitsuba.render import SurfaceInteraction3f

    it = SurfaceInteraction3f.zero()
    it.p = [-0.5, 0.3, -0.1]  # Some position inside the unit sphere
    it.time = 1.0

    samples = [[0.4, 0.5, 0.3], [0.1, 0.4, 0.9]]

    emitter, _ = create_emitter_and_spectrum()
    ds, spectrum = emitter.sample_direction(it, samples)

    assert ek.allclose(ds.pdf, InvFourPi)
    assert ek.allclose(ds.d, warp.square_to_uniform_sphere(samples))
    assert ek.allclose(emitter.pdf_direction(it, ds), InvFourPi)
    assert ek.allclose(ds.time, it.time)
