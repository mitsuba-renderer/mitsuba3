import pytest
import drjit as dr
import mitsuba as mi


spectrum_dicts = {
    'd65': { 'type' : 'd65' },
    'regular': {
        'type' : 'regular',
        'wavelength_min' : 500.0,
        'wavelength_max' : 600.0,
        'values' : "1, 2",
    }
}

lookat_transforms = [
    mi.scalar_rgb.ScalarTransform4f().look_at([0, 1, 0], [0, 0, 0], [1, 0, 0]),
    mi.scalar_rgb.ScalarTransform4f().look_at([0, 0, 1], [0, 0, 0], [0, -1, 0])
]


def create_emitter_and_spectrum(lookat, cutoff_angle, s_key='d65'):
    spectrum = mi.load_dict(spectrum_dicts[s_key])
    expanded = spectrum.expand()
    if len(expanded) == 1:
        spectrum = expanded[0]

    emitter = mi.load_dict({
        'type' : 'spot',
        'cutoff_angle' : cutoff_angle,
        'to_world' : lookat,
        'intensity' : spectrum
    })

    return emitter, spectrum


@pytest.mark.parametrize("spectrum_key", spectrum_dicts.keys())
@pytest.mark.parametrize("it_pos", [[2.0, 0.5, 0.0], [1.0, 0.5, -5.0]])
@pytest.mark.parametrize("wavelength_sample", [0.7])
@pytest.mark.parametrize("cutoff_angle", [20, 80])
@pytest.mark.parametrize("lookat", lookat_transforms)
def test_sample_direction(variant_scalar_spectral, spectrum_key, it_pos, wavelength_sample, cutoff_angle, lookat):
    """ Check the correctness of the sample_direction() method """

    cutoff_angle_rad = cutoff_angle / 180 * dr.pi
    beam_width_rad = cutoff_angle_rad * 0.75
    inv_transition_width = 1 / (cutoff_angle_rad - beam_width_rad)
    emitter, spectrum = create_emitter_and_spectrum(lookat, cutoff_angle, spectrum_key)
    eval_t = 0.3
    trafo = mi.Transform4f(emitter.world_transform())

    # Create a surface iteration
    it = mi.SurfaceInteraction3f()
    it.wavelengths = [0, 0, 0, 0]
    it.p = it_pos
    it.time = eval_t

    # Sample a wavelength from spectrum
    wav, spec = spectrum.sample_spectrum(it, mi.sample_shifted(wavelength_sample))
    it.wavelengths = wav

    # Direction from the position to the point emitter
    d = mi.Vector3f(-it.p + lookat.translation())
    dist = dr.norm(d)
    d /= dist

    # Calculate angle between lookat direction and ray direction
    angle = dr.acos((trafo.inverse() @ (-d))[2])
    angle = dr.select(dr.abs(angle - beam_width_rad)
                      < 1e-3, beam_width_rad, angle)
    angle = dr.select(dr.abs(angle - cutoff_angle_rad)
                      < 1e-3, cutoff_angle_rad, angle)

    # Sample a direction from the emitter
    ds, res = emitter.sample_direction(it, [0, 0])

    # Evaluate the spectrum
    spec = spectrum.eval(it)
    spec = dr.select(angle <= beam_width_rad, spec, spec *
                     ((cutoff_angle_rad - angle) * inv_transition_width))
    spec = dr.select(angle <= cutoff_angle_rad, spec, 0)

    assert ds.time == it.time
    assert ds.pdf == 1.0
    assert ds.delta
    assert dr.allclose(ds.d, d)
    assert dr.allclose(res, spec / (dist**2))


@pytest.mark.parametrize("spectrum_key", spectrum_dicts.keys())
@pytest.mark.parametrize("wavelength_sample", [0.7])
@pytest.mark.parametrize("pos_sample", [[0.4, 0.5], [0.1, 0.4]])
@pytest.mark.parametrize("cutoff_angle", [20, 80])
@pytest.mark.parametrize("lookat", lookat_transforms)
def test_sample_ray(variants_vec_spectral, spectrum_key, wavelength_sample, pos_sample, cutoff_angle, lookat):
    # Check the correctness of the sample_ray() method

    cutoff_angle_rad = cutoff_angle / 180 * dr.pi
    cos_cutoff_angle_rad = dr.cos(cutoff_angle_rad)
    beam_width_rad = cutoff_angle_rad * 0.75
    inv_transition_width = 1 / (cutoff_angle_rad - beam_width_rad)
    emitter, spectrum = create_emitter_and_spectrum(
        lookat, cutoff_angle, spectrum_key)
    eval_t = 0.3
    trafo = mi.Transform4f(emitter.world_transform())

    # Sample a local direction and calculate local angle
    dir_sample = pos_sample  # not being used anyway
    local_dir = mi.warp.square_to_uniform_cone(pos_sample, cos_cutoff_angle_rad)
    angle = dr.acos(local_dir[2])
    angle = dr.select(dr.abs(angle - beam_width_rad)
                      < 1e-3, beam_width_rad, angle)
    angle = dr.select(dr.abs(angle - cutoff_angle_rad)
                      < 1e-3, cutoff_angle_rad, angle)

    # Sample a ray (position, direction, wavelengths) from the emitter
    ray, res = emitter.sample_ray(
        eval_t, wavelength_sample, pos_sample, dir_sample)

    # Sample wavelengths on the spectrum
    it = dr.zeros(mi.SurfaceInteraction3f)
    wav, spec = spectrum.sample_spectrum(it, mi.sample_shifted(wavelength_sample))
    it.wavelengths = wav
    spec = spec * dr.select(angle <= beam_width_rad,
                            1,
                            ((cutoff_angle_rad - angle) * inv_transition_width))
    spec = dr.select(angle <= cutoff_angle_rad, spec, 0)

    assert dr.allclose(
        res, spec / mi.warp.square_to_uniform_cone_pdf(trafo.inverse() @ ray.d, cos_cutoff_angle_rad))
    assert dr.allclose(ray.time, eval_t)
    assert dr.all(local_dir.z >= cos_cutoff_angle_rad)
    assert dr.allclose(ray.wavelengths, wav)
    assert dr.allclose(ray.d, trafo @ local_dir)
    assert dr.allclose(ray.o, lookat.translation())


@pytest.mark.parametrize("spectrum_key", spectrum_dicts.keys())
@pytest.mark.parametrize("cutoff_angle", [20, 60])
@pytest.mark.parametrize("lookat", lookat_transforms)
def test_eval(variants_vec_spectral, spectrum_key, lookat, cutoff_angle):
    # Check the correctness of the eval() method

    emitter, spectrum = create_emitter_and_spectrum(
        lookat, cutoff_angle, spectrum_key)

    # Check that incident direction in the illuminated direction is zero (because hitting a delta light is impossible)
    it = dr.zeros(mi.SurfaceInteraction3f, 3)
    it.wi = [0, 1, 0]
    assert dr.allclose(emitter.eval(it), 0.)
