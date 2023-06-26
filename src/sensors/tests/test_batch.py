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
        "to_world": mi.ScalarTransform4f.look_at(
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
