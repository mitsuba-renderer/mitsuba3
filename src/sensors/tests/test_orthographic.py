import pytest

import mitsuba as mi
import drjit as dr


def create_camera(o, d, s_open=1.5, s_close=5):
    o = mi.scalar_rgb.Vector3f(o)
    d = mi.scalar_rgb.Vector3f(d)

    return mi.load_dict({
        'type': 'orthographic',
        'near_clip': 1.0,
        'far_clip': 35.0,
        'shutter_open': s_open,
        'shutter_close': s_close,
        'to_world': mi.scalar_rgb.Transform4f().look_at(
            origin=o,
            target=o + d,
            up=[0, 1, 0]
        ),
        'film': {
            'type': 'hdrfilm',
            'width': 512,
            'height': 256,
        },
    })


origins    = [[1.0, 0.0, 1.5], [1.0, 4.0, 1.5]]
directions = [[0.0, 0.0, 1.0], [1.0, 0.0, 0.0]]


@pytest.mark.parametrize("origin", origins)
@pytest.mark.parametrize("direction", directions)
@pytest.mark.parametrize("s_open", [0.0, 1.5])
@pytest.mark.parametrize("s_time", [0.0, 3.0])
def test01_create(variant_scalar_rgb, origin, direction, s_open, s_time):
    camera = create_camera(origin, direction, s_open=s_open, s_close=s_open + s_time)

    assert dr.allclose(camera.near_clip(), 1)
    assert dr.allclose(camera.far_clip(), 35)
    assert dr.allclose(camera.shutter_open(), s_open)
    assert dr.allclose(camera.shutter_open_time(), s_time)
    assert not camera.needs_aperture_sample()
    assert camera.bbox() == mi.BoundingBox3f(origin, origin)
    assert dr.allclose(camera.world_transform().matrix,
                       mi.Transform4f().look_at(origin, mi.Vector3f(origin) + direction, [0, 1, 0]).matrix)


@pytest.mark.parametrize("origin", origins)
@pytest.mark.parametrize("direction", directions)
def test02_sample_ray(variants_vec_spectral, origin, direction):
    # Check the correctness of the sample_ray() method
    camera = create_camera(origin, direction)
    near_clip = 1.0
    time = 0.5
    wav_sample = [0.5, 0.33, 0.1]
    pos_sample = [[0.2, 0.1, 0.2], [0.6, 0.9, 0.2]]
    aperture_sample = 0 # Not being used

    ray, spec_weight = camera.sample_ray(time, wav_sample, pos_sample, aperture_sample)

    # Importance sample wavelength and weight
    wav, spec = mi.sample_rgb_spectrum(mi.sample_shifted(wav_sample))

    assert dr.allclose(ray.wavelengths, wav)
    assert dr.allclose(spec_weight, spec)
    assert dr.allclose(ray.time, time)

    # Check that ray origins are on the plane defined by the sensor
    assert dr.allclose(dr.dot(ray.o, direction), dr.dot(origin + mi.Vector3f(direction) * near_clip, direction))

    # Check that a [0.5, 0.5] position_sample generates a ray
    # that points in the camera direction
    ray, _ = camera.sample_ray(0, 0, [0.5, 0.5], 0)
    assert dr.allclose(ray.d, direction, atol=1e-7)


@pytest.mark.parametrize("origin", origins)
@pytest.mark.parametrize("direction", directions)
def test03_sample_ray_differential(variants_vec_spectral, origin, direction):
    # Check the correctness of the sample_ray_differential() method
    camera = create_camera(origin, direction)

    near_clip = 1.0
    time = 0.5
    wav_sample = [0.5, 0.33, 0.1]
    pos_sample = [[0.2, 0.1, 0.2], [0.6, 0.9, 0.2]]

    ray, spec_weight = camera.sample_ray_differential(time, wav_sample, pos_sample, 0)

    # Importance sample wavelength and weight
    wav, spec = mi.sample_rgb_spectrum(mi.sample_shifted(wav_sample))

    assert dr.allclose(ray.wavelengths, wav)
    assert dr.allclose(spec_weight, spec)
    assert dr.allclose(ray.time, time)

    # Check that ray origins are on the plane defined by the sensor
    assert dr.allclose(dr.dot(ray.o, direction), dr.dot(origin + mi.Vector3f(direction) * near_clip, direction))

    # Check that the derivatives are orthogonal

    # Check that a [0.5, 0.5] position_sample generates a ray
    # that points in the camera direction
    ray_center, _ = camera.sample_ray_differential(0, 0, [0.5, 0.5], 0)

    assert dr.allclose(ray_center.d,   direction)
    assert dr.allclose(ray_center.d_x, direction)
    assert dr.allclose(ray_center.d_y, direction)

    # Check correctness of the ray derivatives

    # Deltas in screen space
    dx = 1.0 / camera.film().crop_size().x
    dy = 1.0 / camera.film().crop_size().y

    # # Sample the rays by offsetting the position_sample with the deltas
    ray_dx, _ = camera.sample_ray_differential(0, 0, [0.5 + dx, 0.5], 0)
    ray_dy, _ = camera.sample_ray_differential(0, 0, [0.5, 0.5 + dy], 0)

    assert dr.allclose(ray_dx.o, ray_center.o_x)
    assert dr.allclose(ray_dy.o, ray_center.o_y)
