import pytest
import numpy as np
import pandas as pd
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

# Set up some basic photon data
photon_detected = pd.DataFrame([[1,-0.086816,102.75,1028.0,0.047872,0.11631,0.99156,412.48667183165185],
                                [1,-0.086816,102.75,1028.0,0.029383,0.14085,0.98779,412.48667183165185]])
x_position, y_position, z_position = photon_detected.values[:, 1:4].T
x_momentum, y_momentum, z_momentum = photon_detected.values[:, 4:7].T
print(x_position, z_position)
# calculate the target coordinates of the photons
x_target = x_position + x_momentum
y_target = y_position + y_momentum
z_target = z_position + z_momentum

emitter_data = np.column_stack((x_position, z_position, y_position, x_target, z_target, y_target)).flatten()
emitter_data = np.insert(emitter_data, 0, len(x_position))
photon_data = np.zeros((1, 1, len(emitter_data)), dtype=np.float32)
photon_data[0, 0, :] = emitter_data


def create_emitter_and_spectrum(lookat, s_key='d65'):
    spectrum = mi.load_dict(spectrum_dicts[s_key])
    expanded = spectrum.expand()
    if len(expanded) == 1:
        spectrum = expanded[0]

    photon_list = mi.VolumeGrid(photon_data)
    emitter = mi.load_dict({
        'type' : 'photon',
        'photon_list' : photon_list,
        # 'cutoff_angle' : cutoff_angle,
        'to_world' : lookat,
        'intensity' : spectrum
    })

    return emitter, spectrum


@pytest.mark.parametrize("spectrum_key", spectrum_dicts.keys())
@pytest.mark.parametrize("it_pos", [[2.0, 0.5, 0.0], [1.0, 0.5, -5.0]])
@pytest.mark.parametrize("wavelength_sample", [0.7])
# @pytest.mark.parametrize("cutoff_angle", [20, 80])
@pytest.mark.parametrize("lookat", lookat_transforms)
def test_sample_direction(variant_scalar_spectral, spectrum_key, it_pos, wavelength_sample, lookat):
    """ Check the correctness of the sample_direction() method """

    # Test a fixed cutoff angle?
    cutoff_angle = 20
    cutoff_angle_rad = cutoff_angle / 180 * dr.pi
    beam_width_rad = cutoff_angle_rad * 0.75
    inv_transition_width = 1 / (cutoff_angle_rad - beam_width_rad)
    emitter, spectrum = create_emitter_and_spectrum(lookat, spectrum_key)
    eval_t = 0.3
    # TODO: work out how to test the transforms used in the photon emitter here
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
    # assert dr.allclose(ds.d, d)
    # assert dr.allclose(res, spec / (dist**2))


@pytest.mark.parametrize("spectrum_key", spectrum_dicts.keys())
@pytest.mark.parametrize("wavelength_sample", [0.7])
@pytest.mark.parametrize("pos_sample", [[0.4, 0.5], [0.1, 0.4]])
# @pytest.mark.parametrize("cutoff_angle", [20, 80])
@pytest.mark.parametrize("lookat", lookat_transforms)
def test_sample_ray(variants_vec_spectral, spectrum_key, wavelength_sample, pos_sample, lookat):
    # Check the correctness of the sample_ray() method

    # Fixed cutoff angle
    cutoff_angle = 20
    cutoff_angle_rad = cutoff_angle / 180 * dr.pi
    cos_cutoff_angle_rad = dr.cos(cutoff_angle_rad)
    beam_width_rad = cutoff_angle_rad * 0.75
    inv_transition_width = 1 / (cutoff_angle_rad - beam_width_rad)
    emitter, spectrum = create_emitter_and_spectrum(
        lookat, spectrum_key)
    eval_t = 0.3
    # TODO: work out how to test the transforms used in the photon emitter here
    trafo = mi.Transform4f(emitter.world_transform())

    # Sample a local direction and calculate local angle
    dir_sample = pos_sample  # not being used anyway
    # local_dir = mi.warp.square_to_uniform_cone(pos_sample, cos_cutoff_angle_rad)
    local_dir = mi.ScalarVector3f(0., 0., 1.)     
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

    # assert dr.allclose(
    #     res, spec / mi.warp.square_to_uniform_cone_pdf(trafo.inverse() @ ray.d, cos_cutoff_angle_rad))
    pdf_dir = 445029
    assert dr.allclose(res, spec / pdf_dir)
    assert dr.allclose(ray.time, eval_t)
    assert dr.all(local_dir.z >= cos_cutoff_angle_rad)
    assert dr.allclose(ray.wavelengths, wav)
    # TODO: work out how to test the transforms used in the photon emitter here
    # assert dr.allclose(ray.d, trafo @ local_dir)
    # assert dr.allclose(ray.o, lookat.translation())


@pytest.mark.parametrize("spectrum_key", spectrum_dicts.keys())
# @pytest.mark.parametrize("cutoff_angle", [20, 60])
@pytest.mark.parametrize("lookat", lookat_transforms)
def test_eval(variants_vec_spectral, spectrum_key, lookat):
    # Check the correctness of the eval() method

    emitter, spectrum = create_emitter_and_spectrum(
        lookat, spectrum_key)

    # Check that incident direction in the illuminated direction is zero (because hitting a delta light is impossible)
    it = dr.zeros(mi.SurfaceInteraction3f, 3)
    it.wi = [0, 1, 0]
    assert dr.allclose(emitter.eval(it), 0.)


def test_load_methods():
    # Check that the same emitter is created for each type of loading method
    photon_list = mi.VolumeGrid(photon_data)
    intensity = 1000.0
    volume_grid_emitter = mi.load_dict({
        'type' : 'photon',
        'photon_list' : photon_list,
        # 'cutoff_angle' : cutoff_angle,
        'intensity' : intensity
    })

    # Create a binary file from the photon data
    import struct

    with open("photon_geometry.bin", "wb") as f:
        f.write(struct.pack("<Q", len(x_position)))  # Use 'Q' format for 64-bit unsigned integer (size_t)
        for x1, y1, z1, x2, y2, z2 in zip(x_position, y_position, z_position, x_target, y_target, z_target):
            f.write(struct.pack("<f", x1))
            f.write(struct.pack("<f", z1))
            f.write(struct.pack("<f", y1))
            f.write(struct.pack("<f", x2))
            f.write(struct.pack("<f", z2))
            f.write(struct.pack("<f", y2))

    binary_file_emitter = mi.load_dict({
        'type' : 'photon',
        'filename' : 'photon_geometry.bin',
        'intensity' : intensity
    })

    # We now have two emitters so compare the two using the same sampling parameters
    cutoff_angle = 20
    cutoff_angle_rad = cutoff_angle / 180 * dr.pi
    beam_width_rad = cutoff_angle_rad * 0.75
    eval_t = 0.3
    # TODO: work out how to test the transforms used in the photon emitter here
    vol_trafo = mi.Transform4f(volume_grid_emitter.world_transform())
    bin_trafo = mi.Transform4f(binary_file_emitter.world_transform())

    # Sample a local direction and calculate local angle
    pos_sample = [0.4, 0.5]
    dir_sample = pos_sample
    local_dir = mi.ScalarVector3f(0., 0., 1.)     
    angle = dr.acos(local_dir[2])
    angle = dr.select(dr.abs(angle - beam_width_rad)
                      < 1e-3, beam_width_rad, angle)
    angle = dr.select(dr.abs(angle - cutoff_angle_rad)
                      < 1e-3, cutoff_angle_rad, angle)
    
    wavelength_sample = 0.7

    # Sample a ray (position, direction, wavelengths) from the emitters
    vol_ray, vol_res = volume_grid_emitter.sample_ray(
        eval_t, wavelength_sample, pos_sample, dir_sample)

    bin_ray, bin_res = binary_file_emitter.sample_ray(
        eval_t, wavelength_sample, pos_sample, dir_sample)

    assert(vol_ray.o[0] == bin_ray.o[0])
    assert(vol_ray.d[0] == bin_ray.d[0])
    assert(vol_ray.wavelengths[0] == bin_ray.wavelengths[0])
    assert(vol_res[0] == bin_res[0])

    # Delete the photon_geometry binary file
    import os
    os.remove('photon_geometry.bin')
    