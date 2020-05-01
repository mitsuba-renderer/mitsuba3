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
       </spectrum>"""
}

lookat_transforms = [
    {
        "pos": [0, 1, 0],
        "target": [0, 0, 0],
        "up": [1, 0, 0]
    },
    {
        "pos": [0, 0, 0],
        "target": [0, 0, 1],
        "up": [0, 1, 0]
    },
    {
        "pos": [0, 0, 1],
        "target": [0, 0, 0],
        "up": [0, -1, 0]
    }
]


def create_emitter_and_spectrum(lookat, cutoff_angle, s_key='d65'):
    from mitsuba.core.xml import load_string
    emitter = load_string("""<emitter version='2.0.0' type='spot'>
                                <float name='cutoff_angle' value='{ca}'/>
                                <transform name='to_world'>
                                    <lookat origin='{px}, {py}, {pz}' target='{tx}, {ty}, {tz}' up='{ux}, {uy}, {uz}'/>
                                </transform>
                                {s}
                             </emitter>""".format(ca=cutoff_angle,
                                                  px=lookat["pos"][0], py=lookat["pos"][1], pz=lookat["pos"][2],
                                                  tx=lookat["target"][0], ty=lookat["target"][1], tz=lookat["target"][2],
                                                  ux=lookat["up"][0], uy=lookat["up"][1], uz=lookat["up"][2],
                                                  s=spectrum_strings[s_key]))
    spectrum = load_string(spectrum_strings[s_key])
    expanded = spectrum.expand()
    if len(expanded) == 1:
        spectrum = expanded[0]

    return emitter, spectrum


@pytest.mark.parametrize("spectrum_key", spectrum_strings.keys())
@pytest.mark.parametrize("it_pos", [[2.0, 0.5, 0.0], [1.0, 0.5, -5.0], [0.0, 0.5, 0.0]])
@pytest.mark.parametrize("wavelength_sample", [0.1, 0.3, 0.5, 0.7])
@pytest.mark.parametrize("cutoff_angle", [20, 40, 60, 80])
@pytest.mark.parametrize("lookat", lookat_transforms)
def test_sample_direction(variant_scalar_spectral, spectrum_key, it_pos, wavelength_sample, cutoff_angle, lookat):
    # Check the correctness of the sample_direction() method

    import math
    from mitsuba.core import sample_shifted, Transform4f
    from mitsuba.render import SurfaceInteraction3f

    cutoff_angle_rad = math.radians(cutoff_angle)
    beam_width_rad = cutoff_angle_rad * 0.75
    inv_transition_width = 1 / (cutoff_angle_rad - beam_width_rad)
    emitter, spectrum = create_emitter_and_spectrum(
        lookat, cutoff_angle, spectrum_key)
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
    d = -it.p + lookat["pos"]
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
@pytest.mark.parametrize("wavelength_sample", [0.1, 0.3, 0.5, 0.7])
@pytest.mark.parametrize("pos_sample", [[0.4, 0.5], [0.1, 0.4]])
@pytest.mark.parametrize("cutoff_angle", [20, 40, 60, 80])
@pytest.mark.parametrize("lookat", lookat_transforms)
def test_sample_ray(variant_packet_spectral, spectrum_key, wavelength_sample, pos_sample, cutoff_angle, lookat):
    # Check the correctness of the sample_ray() method

    import math
    from mitsuba.core import warp, sample_shifted, Transform4f
    from mitsuba.render import SurfaceInteraction3f

    cutoff_angle_rad = math.radians(cutoff_angle)
    cos_cutoff_angle_rad = math.cos(cutoff_angle_rad)
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
    assert ek.allclose(ray.o, lookat["pos"])


@pytest.mark.parametrize("spectrum_key", spectrum_strings.keys())
@pytest.mark.parametrize("cutoff_angle", [20, 40, 60])
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
