import pytest
import drjit as dr
import mitsuba as mi


def create_perspective_camera(fov=45):
    camera_dict = {
        "type": "perspective",
        "near_clip": 0.1,
        "far_clip": 100.0,
        "fov": fov,
        "to_world": mi.ScalarTransform4f().look_at(
            origin=[0, 0, 0],
            target=[0, 0, 1],
            up=[0, 1, 0]
        ),
        "film": {
            "type": "hdrfilm",
            "width": 640,
            "height": 480,
        }
    }
    return mi.load_dict(camera_dict)


def create_orthographic_camera():
    camera_dict = {
        "type": "orthographic",
        "near_clip": 0.1,
        "far_clip": 100.0,
        "to_world": mi.ScalarTransform4f().look_at(
            origin=[0, 0, 0],
            target=[0, 0, 1],
            up=[0, 1, 0]
        ),
        "film": {
            "type": "hdrfilm",
            "width": 640,
            "height": 480,
        }
    }
    return mi.load_dict(camera_dict)


def test_perspective_matches_manual_calculation(variant_scalar_rgb):
    """Test that perspective camera projection matrix matches mi.perspective_projection()"""
    sensor = create_perspective_camera(fov=45)

    # Get projection matrix from the sensor
    proj_matrix = sensor.projection_transform()

    # Calculate manually using mi.perspective_projection
    film = sensor.film()
    x_fov = mi.traverse(sensor)['x_fov']
    manual_proj = mi.perspective_projection(
        film.size(), film.crop_size(), film.crop_offset(),
        x_fov, sensor.near_clip(), sensor.far_clip()
    )

    # They should be identical
    assert dr.allclose(proj_matrix.matrix, manual_proj.matrix)


def test_orthographic_matches_manual_calculation(variant_scalar_rgb):
    """Test that orthographic camera projection matrix matches mi.orthographic_projection()"""
    sensor = create_orthographic_camera()

    # Get projection matrix from the sensor
    proj_matrix = sensor.projection_transform()

    # Calculate manually using mi.orthographic_projection
    film = sensor.film()
    manual_proj = mi.orthographic_projection(
        film.size(), film.crop_size(), film.crop_offset(),
        sensor.near_clip(), sensor.far_clip()
    )

    # They should be identical
    assert dr.allclose(proj_matrix.matrix, manual_proj.matrix)
