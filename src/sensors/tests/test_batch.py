import pytest
import drjit as dr
import mitsuba as mi


def create_perspective(o, d, fov=34, fov_axis="x", s_open=1.5, s_close=5, near_clip=1.0):
    t = [o[0] + d[0], o[1] + d[1], o[2] + d[2]]

    camera_dict = {
        "type": "perspective",
        "near_clip": near_clip,
        "far_clip": 35.0,
        "focus_distance": 15.0,
        "fov": fov,
        "fov_axis": fov_axis,
        "shutter_open": s_open,
        "shutter_close": s_close,
        "to_world": mi.ScalarTransform4f().look_at(
            origin=o,
            target=t,
            up=[0, 1, 0]
        ),
        "film": {
            "type": "hdrfilm",
            "width": 256,
            "height": 256,
        }
    }

    return mi.load_dict(camera_dict)


def create_batch(sensors, width=512, height=256, s_open=1.5, s_close=5):
    camera_dict = {
        "type": "batch",
        "shutter_open": s_open,
        "shutter_close": s_close,
        "film": {
            "type": "hdrfilm",
            "width": width,
            "height": width,
        }
    }

    for i, sensor in enumerate(sensors):
        camera_dict[f'sensor_{i}'] = sensor

    return mi.load_dict(camera_dict)


origins = [[1.0, 0.0, 1.5], [1.0, 4.0, 1.5]]
directions = [[0.0, 0.0, 1.0], [1.0, 0.0, 0.0]]


@pytest.mark.parametrize("s_open", [0.0, 1.5])
@pytest.mark.parametrize("s_time", [0.0, 3.0])
def test01_create(variant_scalar_rgb, s_open, s_time):
    camera1 = create_perspective(origins[0], directions[0], s_open=s_open, s_close=s_open + s_time)
    camera2 = create_perspective(origins[1], directions[1], s_open=s_open, s_close=s_open + s_time)
    camera = create_batch([camera1, camera2], 512, 256, s_open, s_open + s_time)

    assert dr.allclose(camera.shutter_open(), s_open)
    assert dr.allclose(camera.shutter_open_time(), s_time)
    assert not camera.needs_aperture_sample()
    assert camera.bbox() == mi.BoundingBox3f(*origins)


@pytest.mark.parametrize("s_open", [0.0, 1.5])
@pytest.mark.parametrize("s_time", [0.0, 3.0])
def test02_sample_ray(variants_vec_spectral, s_open, s_time):
    """Check the correctness of the sample_ray() method"""
    near_clip = 1.0
    camera0 = create_perspective(origins[0], directions[0], s_open=s_open, s_close=s_open + s_time)
    camera1 = create_perspective(origins[1], directions[1], s_open=s_open, s_close=s_open + s_time)
    camera = create_batch([camera0, camera1], 512, 256, s_open, s_open + s_time)

    time = 0.5
    wav_sample = [0.5, 0.33, 0.1]
    pos_sample = [[0.2, 0.1, 0.6], [0.6, 0.9, 0.2]]
    aperture_sample = 0 # Not being used

    ray, spec_weight = camera.sample_ray(time, wav_sample, pos_sample, aperture_sample)

    # Importance sample wavelength and weight
    wav, spec = mi.sample_rgb_spectrum(mi.sample_shifted(wav_sample))

    assert dr.allclose(ray.wavelengths, wav)
    assert dr.allclose(spec_weight, spec)
    assert dr.allclose(ray.time, time)

    inv_z0 = dr.rcp((camera0.world_transform().inverse() @ ray.d).z)
    inv_z1 = dr.rcp((camera1.world_transform().inverse() @ ray.d).z)
    o0 = mi.Vector3f(origins[0]) + near_clip * inv_z0 * mi.Vector3f(ray.d)
    o1 = mi.Vector3f(origins[1]) + near_clip * inv_z1 * mi.Vector3f(ray.d)
    o = o0
    dr.scatter(o, dr.gather(mi.Vector3f, o1, 2), 2)
    assert dr.allclose(ray.o, o, atol=1e-4)

    # Check that a [(2*i + 1)/(2 * N), 0.5] for i = 0, 1, ..., #cameras
    # `position_sample` generates a ray that points in the camera direction
    position_sample = mi.Point2f([0.25, 0.25, 0.75], [0.5, 0.5, 0.5])
    ray, _ = camera.sample_ray(0, 0, position_sample, 0)
    direction = dr.zeros(mi.Vector3f, 3)
    dr.scatter(direction, mi.Vector3f(directions[0]), mi.UInt32([0, 1]))
    dr.scatter(direction, mi.Vector3f(directions[1]), mi.UInt32([2]))
    print(f"{ray.d=}")
    print(f"{direction=}")
    assert dr.allclose(ray.d, direction, atol=1e-7)
