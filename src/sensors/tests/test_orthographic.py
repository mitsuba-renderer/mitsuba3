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
    assert dr.allclose(mi.unpolarized_spectrum(spec_weight), spec)
    assert dr.allclose(ray.time, time)

    # Check that ray origins are on the plane defined by the sensor
    assert dr.allclose(dr.dot(ray.o, direction), dr.dot(origin + mi.Vector3f(direction) * near_clip, direction))

    # Check that a [0.5, 0.5] position_sample generates a ray
    # that points in the camera direction
    ray, _ = camera.sample_ray(0, 0, [0.5, 0.5], 0)
    assert dr.allclose(ray.d, direction, atol=1e-7)


@pytest.mark.parametrize("origin", origins)
@pytest.mark.parametrize("direction", directions)
def test03_sample_ray(variants_vec_spectral, origin, direction):
    # Check the correctness of the sample_ray() method
    camera = create_camera(origin, direction)

    near_clip = 1.0
    time = 0.5
    wav_sample = [0.5, 0.33, 0.1]
    pos_sample = [[0.2, 0.1, 0.2], [0.6, 0.9, 0.2]]

    ray, spec_weight = camera.sample_ray(time, wav_sample, pos_sample, 0)

    # Importance sample wavelength and weight
    wav, spec = mi.sample_rgb_spectrum(mi.sample_shifted(wav_sample))

    assert dr.allclose(ray.wavelengths, wav)
    assert dr.allclose(mi.unpolarized_spectrum(spec_weight), spec)
    assert dr.allclose(ray.time, time)

    # Check that ray origins are on the plane defined by the sensor
    assert dr.allclose(dr.dot(ray.o, direction), dr.dot(origin + mi.Vector3f(direction) * near_clip, direction))

    # Check that a [0.5, 0.5] position_sample generates a ray
    # that points in the camera direction
    ray_center, _ = camera.sample_ray(0, 0, [0.5, 0.5], 0)
    assert dr.allclose(ray_center.d, direction)


def test04_ray_cone(variants_all_backends_once):
    # The camera space image plane spans [-1, 1] horizontally, so a pixel of
    # this 100 pixel wide film is 0.02 units wide before the 10x scaling
    camera = mi.load_dict({
        'type': 'orthographic',
        'to_world': mi.ScalarAffineTransform4f().scale([10, 10, 1]),
        'film': {'type': 'hdrfilm', 'width': 100, 'height': 50}
    })
    ray, _ = camera.sample_ray(0, 0.5, [0.5, 0.5], [0.5, 0.5])
    assert dr.allclose(ray.cone.width, 0.2, rtol=1e-5)
    assert dr.allclose(ray.cone.spread, 0)


def test05_ray_cone_animated(variants_all_backends_once):
    """The ray cone width follows the interpolated scale of an animated sensor"""
    # Scale doubles from [10, 10, 1] at t=0 to [20, 20, 1] at t=1
    camera = mi.load_dict({
        'type': 'orthographic',
        'to_world': mi.AnimatedTransform4f({
            0.0: mi.ScalarAffineTransform4f().scale([10, 10, 1]),
            1.0: mi.ScalarAffineTransform4f().scale([20, 20, 1])
        }),
        'film': {'type': 'hdrfilm', 'width': 100, 'height': 50}
    })
    ray0, _ = camera.sample_ray(0.0, 0.5, [0.5, 0.5], [0.5, 0.5])
    ray_mid, _ = camera.sample_ray(0.5, 0.5, [0.5, 0.5], [0.5, 0.5])
    ray1, _ = camera.sample_ray(1.0, 0.5, [0.5, 0.5], [0.5, 0.5])
    assert dr.allclose(ray0.cone.width, 0.2, rtol=1e-5)
    assert dr.allclose(ray_mid.cone.width, 0.3, rtol=1e-5)
    assert dr.allclose(ray1.cone.width, 0.4, rtol=1e-5)
    assert dr.allclose(ray0.cone.spread, 0)
    assert dr.allclose(ray_mid.cone.spread, 0)
    assert dr.allclose(ray1.cone.spread, 0)

