import enoki as ek
import numpy as np
import pytest


def sensor_dict(target=None, to_world=None):
    result = {"type": "hdistant"}

    if to_world is not None:
        result["to_world"] = to_world

    if target == "point":
        result.update({"target": [0, 0, 0]})

    elif target == "shape":
        result.update({"target": {"type": "rectangle"}})

    elif isinstance(target, dict):
        result.update({"target": target})

    return result


def make_sensor(d):
    from mitsuba.core.xml import load_dict

    return load_dict(d).expand()[0]


def test_construct(variant_scalar_rgb):
    from mitsuba.core import ScalarTransform4f

    # Construct without parameters
    sensor = make_sensor({"type": "hdistant"})
    assert sensor is not None
    assert not sensor.bbox().valid()  # Degenerate bounding box

    # Construct with transform
    sensor = make_sensor(
        sensor_dict(to_world=ScalarTransform4f.look_at(
            origin=[0, 0, 0], target=[0, 0, 1], up=[1, 0, 0])))

    # Test different target values
    # -- No target,
    sensor = make_sensor(sensor_dict())
    assert sensor is not None

    # -- Point target
    sensor = make_sensor(sensor_dict(target="point"))
    assert sensor is not None

    # -- Shape target
    sensor = make_sensor(sensor_dict(target="shape"))
    assert sensor is not None

    # -- Random object target (we expect to raise)
    with pytest.raises(RuntimeError):
        make_sensor(sensor_dict(target={"type": "constant"}))


def test_sample_ray_direction(variant_scalar_rgb):
    sensor = make_sensor(sensor_dict())

    # Check that directions are appropriately set
    for (sample1, sample2, expected) in [
        [[0.5, 0.5], [0.16, 0.44], [0, 0, -1]],
        [[0.0, 0.0], [0.23, 0.40], [0.707107, 0.707107, 0]],
        [[1.0, 0.0], [0.22, 0.81], [-0.707107, 0.707107, 0]],
        [[0.0, 1.0], [0.99, 0.42], [0.707107, -0.707107, 0]],
        [[1.0, 1.0], [0.52, 0.31], [-0.707107, -0.707107, 0]],
    ]:
        ray, _ = sensor.sample_ray(1.0, 1.0, sample1, sample2, True)

        # Check that ray direction is what is expected
        assert ek.allclose(ray.d, expected, atol=1e-7)


@pytest.mark.parametrize(
    "sensor_setup",
    [
        "default",
        "target_square",
        "target_square_small",
        "target_square_large",
        "target_disk",
        "target_point",
    ],
)
@pytest.mark.parametrize("w_e", [[0, 0, -1], [0, 1, -1]])
@pytest.mark.parametrize("w_o", [[0, 0, 1], [0, 1, 1]])
def test_sample_target(variant_scalar_rgb, sensor_setup, w_e, w_o):
    # This test checks if targeting works as intended by rendering a basic scene
    from mitsuba.core import Bitmap, ScalarTransform4f, Struct
    from mitsuba.core.xml import load_dict

    # Basic illumination and sensing parameters
    l_e = 1.0  # Emitted radiance
    w_e = list(w_e / np.linalg.norm(w_e))  # Emitter direction
    w_o = list(w_o / np.linalg.norm(w_o))  # Sensor direction
    cos_theta_e = abs(np.dot(w_e, [0, 0, 1]))

    # Reflecting surface specification
    surface_scale = 1.0
    rho = 1.0  # Surface reflectance

    # Sensor definitions
    sensors = {
        "default": {  # No target, origin projected to bounding sphere
            "type": "hdistant",
            "sampler": {
                "type": "independent",
                "sample_count": 100000,
            },
            "film": {
                "type": "hdrfilm",
                "height": 4,
                "width": 4,
                "rfilter": {"type": "box"},
            },
        },
        "target_square": {  # Targeting square, origin projected to bounding sphere
            "type": "hdistant",
            "target": {
                "type": "rectangle",
                "to_world": ScalarTransform4f.scale(surface_scale),
            },
            "sampler": {
                "type": "independent",
                "sample_count": 1000,
            },
            "film": {
                "type": "hdrfilm",
                "height": 16,
                "width": 16,
                "rfilter": {"type": "box"},
            },
        },
        "target_square_small": {  # Targeting small square, origin projected to bounding sphere
            "type": "hdistant",
            "target": {
                "type": "rectangle",
                "to_world": ScalarTransform4f.scale(0.5 * surface_scale),
            },
            "sampler": {
                "type": "independent",
                "sample_count": 1000,
            },
            "film": {
                "type": "hdrfilm",
                "height": 16,
                "width": 16,
                "rfilter": {"type": "box"},
            },
        },
        "target_square_large": {  # Targeting large square, origin projected to bounding sphere
            "type": "hdistant",
            "target": {
                "type": "rectangle",
                "to_world": ScalarTransform4f.scale(2.0 * surface_scale),
            },
            "sampler": {
                "type": "independent",
                "sample_count": 1000000,
            },
            "film": {
                "type": "hdrfilm",
                "height": 4,
                "width": 4,
                "rfilter": {"type": "box"},
            },
        },
        "target_point": {  # Targeting point, origin projected to bounding sphere
            "type": "hdistant",
            "target": [0, 0, 0],
            "sampler": {
                "type": "independent",
                "sample_count": 1000,
            },
            "film": {
                "type": "hdrfilm",
                "height": 16,
                "width": 16,
                "rfilter": {"type": "box"},
            },
        },
        "target_disk": {  # Targeting disk, origin projected to bounding sphere
            "type": "hdistant",
            "target": {
                "type": "disk",
                "to_world": ScalarTransform4f.scale(surface_scale),
            },
            "sampler": {
                "type": "independent",
                "sample_count": 1000,
            },
            "film": {
                "type": "hdrfilm",
                "height": 16,
                "width": 16,
                "rfilter": {"type": "box"},
            },
        },
    }

    # Scene setup
    scene_dict = {
        "type": "scene",
        "shape": {
            "type": "rectangle",
            "to_world": ScalarTransform4f.scale(surface_scale),
            "bsdf": {
                "type": "diffuse",
                "reflectance": rho,
            },
        },
        "emitter": {"type": "directional", "direction": w_e, "irradiance": l_e},
        "integrator": {"type": "path"},
    }

    scene = load_dict({**scene_dict, "sensor": sensors[sensor_setup]})

    # Run simulation
    sensor = scene.sensors()[0]
    scene.integrator().render(scene, sensor)

    # Check result
    result = np.array(
        sensor.film()
        .bitmap()
        .convert(Bitmap.PixelFormat.RGB, Struct.Type.Float32, False)
    ).squeeze()

    surface_area = 4.0 * surface_scale ** 2  # Area of square surface
    l_o = l_e * cos_theta_e * rho / np.pi  # * cos_theta_o
    expected = {  # Special expected values for some cases
        "default": l_o * 2.0 / ek.Pi,
        "target_square_large": l_o * 0.25,
    }
    expected_value = expected.get(sensor_setup, l_o)

    rtol = {  # Special tolerance values for some cases
        "default": 1e-2,
        "target_square_large": 1e-2,
    }
    rtol_value = rtol.get(sensor_setup, 5e-3)

    assert np.allclose(result, expected_value, rtol=rtol_value)


def test_checkerboard(variants_all_rgb):
    """
    Very basic render test with checkerboard texture and square target.
    """
    from mitsuba.core import ScalarTransform4f
    from mitsuba.core.xml import load_dict

    l_o = 1.0
    rho0 = 0.5
    rho1 = 1.0

    # Scene setup
    scene_dict = {
        "type": "scene",
        "shape": {
            "type": "rectangle",
            "bsdf": {
                "type": "diffuse",
                "reflectance": {
                    "type": "checkerboard",
                    "color0": rho0,
                    "color1": rho1,
                    "to_uv": ScalarTransform4f.scale(2),
                },
            },
        },
        "emitter": {
            "type": "directional",
            "direction": [0, 0, -1],
            "irradiance": 1.0
        },
        "sensor0": {
            "type": "hdistant",
            "target": {
                "type": "rectangle"
            },
            "sampler": {
                "type": "independent",
                "sample_count": 50000,
            },
            "film": {
                "type": "hdrfilm",
                "height": 4,
                "width": 4,
                "pixel_format": "luminance",
                "component_format": "float32",
                "rfilter": {
                    "type": "box"
                },
            },
        },
        # "sensor1": {  # In case one would like to check what the scene looks like
        #     "type": "perspective",
        #     "to_world": ScalarTransform4f.look_at(origin=[0, 0, 5], target=[0, 0, 0], up=[0, 1, 0]),
        #     "sampler": {
        #         "type": "independent",
        #         "sample_count": 10000,
        #     },
        #     "film": {
        #         "type": "hdrfilm",
        #         "height": 256,
        #         "width": 256,
        #         "pixel_format": "luminance",
        #         "component_format": "float32",
        #         "rfilter": {"type": "box"},
        #     },
        # },
        "integrator": {
            "type": "path"
        },
    }

    scene = load_dict(scene_dict)

    sensor = scene.sensors()[0]
    scene.integrator().render(scene, sensor)

    data = np.array(sensor.film().bitmap())

    expected = l_o * 0.5 * (rho0 + rho1) / ek.Pi
    assert np.allclose(data, expected, atol=1e-3)
