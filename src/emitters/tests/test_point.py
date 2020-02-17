import mitsuba
import pytest
import enoki as ek


def create_emitter_and_spectrum(pos):
    from mitsuba.core.xml import load_string
    emitter = load_string("""<emitter version='2.0.0' type='point'>
                                 <point name='position' x='{x}' y='{y}' z='{z}'/>
                             </emitter>""".format(x=pos[0], y=pos[1], z=pos[2]))
    spectrum = load_string("<spectrum version='2.0.0' type='d65'/>").expand()[0]
    return emitter, spectrum


def test01_point_sample_direction(variant_scalar_spectral):
    from mitsuba.render import SurfaceInteraction3f

    e_pos = [10, -1, 2]
    emitter, spectrum = create_emitter_and_spectrum(e_pos)

    # Direction sampling
    it = SurfaceInteraction3f.zero()
    it.p = [0.0, -2.0, 4.5]  # Some position
    it.time = 0.3

    # direction from the position to the point emitter
    d = -it.p + e_pos
    dist = ek.norm(d)
    d /= dist

    # sample a direction
    sample = [0.1, 0.5]
    (d_rec, res) = emitter.sample_direction(it, sample)

    assert d_rec.time == it.time
    assert d_rec.pdf == 1.0
    assert d_rec.delta
    assert ek.allclose(d_rec.d, d)

    spec = spectrum.eval(it) / (dist**2)
    assert ek.allclose(res, spec)


def test02_point_sample_direction_vec(variant_packet_spectral):
    from mitsuba.render import SurfaceInteraction3f

    e_pos = [10, -1, 2]
    emitter, spectrum = create_emitter_and_spectrum(e_pos)

    # Direction sampling
    it = SurfaceInteraction3f.zero(3)
    it.p = [[0.0, 0.0, 0.0], [-2.0, 0.0, -2.0],
            [4.5, 4.5, 0.0]]  # Some positions
    it.time = [0.3, 0.3, 0.3]

    # direction from the position to the point emitter
    d = -it.p + e_pos
    dist = ek.norm(d)
    d /= dist

    sample = [0.1, 0.5]
    (d_rec, res) = emitter.sample_direction(it, sample)

    assert ek.all(d_rec.time == it.time)
    assert ek.all(d_rec.pdf == 1.0)
    assert ek.all(d_rec.delta)
    assert ek.allclose(d_rec.d, d / ek.norm(d))

    spec = spectrum.eval(it) / (dist**2)
    assert ek.allclose(res, spec)


def test03_point_sample_ray(variant_packet_spectral):
    from mitsuba.core import Vector3f, warp, sample_shifted
    from mitsuba.render import SurfaceInteraction3f

    e_pos = [10, -1, 2]
    emitter, spectrum = create_emitter_and_spectrum(e_pos)

    time = 0.5
    wavelength_sample = [0.5, 0.33, 0.1]
    dir_sample = [[0.4, 0.5, 0.3], [0.1, 0.4, 0.9]]
    pos_sample = dir_sample  # not being used anyway

    ray, spec = emitter.sample_ray(time, wavelength_sample, pos_sample, dir_sample)

    it = SurfaceInteraction3f.zero(3)
    wav, res = spectrum.sample(it, sample_shifted(wavelength_sample))

    assert ek.allclose(spec, res * 4 * ek.pi)
    assert ek.allclose(ray.time, time)
    assert ek.allclose(ray.wavelengths, wav)
    assert ek.allclose(ray.d, warp.square_to_uniform_sphere(dir_sample))
    assert ek.allclose(ray.o, Vector3f(e_pos))
