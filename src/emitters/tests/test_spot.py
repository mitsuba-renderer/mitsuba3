import mitsuba
import pytest
import enoki as ek

mitsuba.set_variant("scalar_rgb")

spectrum_strings = {
    'd65': { 'type' : 'd65' },
    'regular': {
        'type' : 'regular',
        'lambda_min' : 500.0,
        'lambda_max' : 600.0,
        'values' : "1, 2",
    }
}

lookat_transforms = [
    mitsuba.core.ScalarTransform4f.look_at([0, 1, 0], [0, 0, 0], [1, 0, 0]),
    mitsuba.core.ScalarTransform4f.look_at([0, 0, 1], [0, 0, 0], [0, -1, 0])
]

def create_emitter_and_spectrum(lookat, cutoff_angle, s_key='d65'):
    from mitsuba.core.xml import load_dict

    spectrum = load_dict(spectrum_strings[s_key])
    expanded = spectrum.expand()
    if len(expanded) == 1:
        spectrum = expanded[0]

    emitter = load_dict({
        'type' : 'spot',
        'cutoff_angle' : cutoff_angle,
        'to_world' : lookat,
        'intensity' : spectrum
    })

    return emitter, spectrum


@pytest.mark.parametrize("spectrum_key", spectrum_strings.keys())
@pytest.mark.parametrize("it_pos", [[2.0, 0.5, 0.0], [1.0, 0.5, -5.0]])
@pytest.mark.parametrize("wavelength_sample", [0.1, 0.7])
@pytest.mark.parametrize("cutoff_angle", [20, 80])
@pytest.mark.parametrize("lookat", lookat_transforms)
def test_sample_direction(variant_scalar_spectral, spectrum_key, it_pos, wavelength_sample, cutoff_angle, lookat):
    """ Check the correctness of the sample_direction() method """

    from mitsuba.core import sample_shifted, Transform4f
    from mitsuba.render import SurfaceInteraction3f

    cutoff_angle_rad = cutoff_angle / 180 * ek.pi
    beam_width_rad = cutoff_angle_rad * 0.75
    inv_transition_width = 1 / (cutoff_angle_rad - beam_width_rad)
    emitter, spectrum = create_emitter_and_spectrum(lookat, cutoff_angle, spectrum_key)
    eval_t = 0.3
    trafo = Transform4f(emitter.world_transform().eval(eval_t))

    # Create a surface iteration
    it = SurfaceInteraction3f.zero()
    it.p = it_pos
    it.time = eval_t

    # Sample a wavelength from spectrum
    wav, spec = spectrum.sample(it, sample_shifted(wavelength_sample))
    it.wavelengths = wav

    # Direction from the position to the point emitter
    d = -it.p + lookat.transform_point(0)
    dist = ek.norm(d)
    d /= dist

    # Calculate angle between lookat direction and ray direction
    angle = ek.acos((trafo.inverse().transform_vector(-d))[2])
    angle = ek.select(ek.abs(angle - beam_width_rad)
                      < 1e-3, beam_width_rad, angle)
    angle = ek.select(ek.abs(angle - cutoff_angle_rad)
                      < 1e-3, cutoff_angle_rad, angle)

    # Sample a direction from the emitter
    ds, res = emitter.sample_direction(it, [0, 0])

    # Evalutate the spectrum
    spec = spectrum.eval(it)
    spec = ek.select(angle <= beam_width_rad, spec, spec *
                     ((cutoff_angle_rad - angle) * inv_transition_width))
    spec = ek.select(angle <= cutoff_angle_rad, spec, 0)

    assert ds.time == it.time
    assert ds.pdf == 1.0
    assert ds.delta
    assert ek.allclose(ds.d, d)
    assert ek.allclose(res, spec / (dist**2))


@pytest.mark.parametrize("spectrum_key", spectrum_strings.keys())
@pytest.mark.parametrize("wavelength_sample", [0.1, 0.7])
@pytest.mark.parametrize("pos_sample", [[0.4, 0.5], [0.1, 0.4]])
@pytest.mark.parametrize("cutoff_angle", [20, 80])
@pytest.mark.parametrize("lookat", lookat_transforms)
def test_sample_ray(variant_packet_spectral, spectrum_key, wavelength_sample, pos_sample, cutoff_angle, lookat):
    # Check the correctness of the sample_ray() method

    from mitsuba.core import warp, sample_shifted, Transform4f
    from mitsuba.render import SurfaceInteraction3f

    cutoff_angle_rad = cutoff_angle / 180 * ek.pi
    cos_cutoff_angle_rad = ek.cos(cutoff_angle_rad)
    beam_width_rad = cutoff_angle_rad * 0.75
    inv_transition_width = 1 / (cutoff_angle_rad - beam_width_rad)
    emitter, spectrum = create_emitter_and_spectrum(
        lookat, cutoff_angle, spectrum_key)
    eval_t = 0.3
    trafo = Transform4f(emitter.world_transform().eval(eval_t))

    # Sample a local direction and calculate local angle
    dir_sample = pos_sample  # not being used anyway
    local_dir = warp.square_to_uniform_cone(pos_sample, cos_cutoff_angle_rad)
    angle = ek.acos(local_dir[2])
    angle = ek.select(ek.abs(angle - beam_width_rad)
                      < 1e-3, beam_width_rad, angle)
    angle = ek.select(ek.abs(angle - cutoff_angle_rad)
                      < 1e-3, cutoff_angle_rad, angle)

    # Sample a ray (position, direction, wavelengths) from the emitter
    ray, res = emitter.sample_ray(
        eval_t, wavelength_sample, pos_sample, dir_sample)

    # Sample wavelengths on the spectrum
    it = SurfaceInteraction3f.zero()
    wav, spec = spectrum.sample(it, sample_shifted(wavelength_sample))
    it.wavelengths = wav
    spec = spectrum.eval(it)
    spec = ek.select(angle <= beam_width_rad, spec, spec *
                     ((cutoff_angle_rad - angle) * inv_transition_width))
    spec = ek.select(angle <= cutoff_angle_rad, spec, 0)

    assert ek.allclose(
        res, spec / warp.square_to_uniform_cone_pdf(trafo.inverse().transform_vector(ray.d), cos_cutoff_angle_rad))
    assert ek.allclose(ray.time, eval_t)
    assert ek.all(local_dir.z >= cos_cutoff_angle_rad)
    assert ek.allclose(ray.wavelengths, wav)
    assert ek.allclose(ray.d, trafo.transform_vector(local_dir))
    assert ek.allclose(ray.o, lookat.transform_point(0))


@pytest.mark.parametrize("spectrum_key", spectrum_strings.keys())
@pytest.mark.parametrize("cutoff_angle", [20, 60])
@pytest.mark.parametrize("lookat", lookat_transforms)
def test_eval(variant_packet_spectral, spectrum_key, lookat, cutoff_angle):
    # Check the correctness of the eval() method

    from mitsuba.render import SurfaceInteraction3f
    emitter, spectrum = create_emitter_and_spectrum(
        lookat, cutoff_angle, spectrum_key)

    # Check that incident direction in the illuminated direction is zero (because hitting a delta light is impossible)
    it = SurfaceInteraction3f.zero(3)
    it.wi = [0, 1, 0]
    assert ek.allclose(emitter.eval(it), 0.)
