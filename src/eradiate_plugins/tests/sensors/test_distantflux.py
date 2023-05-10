import drjit as dr
import mitsuba as mi
import numpy as np
import pytest


def sensor_dict(target=None, film="1x1", to_world=None):
    result = {"type": "distantflux"}

    if film == "1x1":
        result.update(
            {
                "film": {
                    "type": "hdrfilm",
                    "width": 1,
                    "height": 1,
                    "rfilter": {"type": "box"},
                }
            }
        )

    elif film == "32x32":
        result.update(
            {
                "film": {
                    "type": "hdrfilm",
                    "width": 32,
                    "height": 32,
                    "rfilter": {"type": "box"},
                }
            }
        )

    if to_world is not None:
        result["to_world"] = to_world

    if target == "point":
        result.update({"target": [0, 0, 0]})

    elif target == "shape":
        result.update({"target": {"type": "rectangle"}})

    elif isinstance(target, dict):
        result.update({"target": target})

    return result


def test_construct(variant_scalar_rgb):
    # Construct without parameters
    sensor = mi.load_dict({"type": "distantflux"})
    assert sensor is not None
    assert not sensor.bbox().valid()  # Degenerate bounding box

    # Construct with transform
    sensor = mi.load_dict(
        sensor_dict(
            to_world=mi.ScalarTransform4f.look_at(
                origin=[0, 0, 0], target=[0, 0, 1], up=[1, 0, 0]
            )
        )
    )

    # Test different target values
    # -- No target,
    sensor = mi.load_dict(sensor_dict())
    assert sensor is not None

    # -- Point target
    sensor = mi.load_dict(sensor_dict(target="point"))
    assert sensor is not None

    # -- Shape target
    sensor = mi.load_dict(sensor_dict(target="shape"))
    assert sensor is not None

    # -- Random object target (we expect to raise)
    with pytest.raises(RuntimeError):
        mi.load_dict(sensor_dict(target={"type": "constant"}))


def bsphere(bbox):
    c = bbox.center()
    return c, dr.norm(c - bbox.max)


def test_sample_ray_direction(variant_scalar_rgb):
    sensor = mi.load_dict(sensor_dict())

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
        assert dr.allclose(ray.d, expected, atol=1e-7)


@pytest.mark.slow
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
def test_sample_ray_target(variant_scalar_rgb, sensor_setup):
    # This test checks if targeting works as intended by rendering a basic scene
    w_e = [0, 0, -1]
    normal = [0, 0, 1]

    # Basic illumination parameters
    l_e = 1.0  # Emitted radiance
    w_e = list(w_e / np.linalg.norm(w_e))  # Emitter direction
    cos_theta_e = abs(np.dot(w_e, normal))

    # Reflecting surface specification
    surface_scale = 1.0
    rho = 1.0  # Surface reflectance

    # Sensor definitions
    sensors = {
        "default": {  # No target, origin projected to bounding sphere
            "type": "distantflux",
            "sampler": {
                "type": "independent",
                "sample_count": 10000,
            },
            "film": {
                "type": "hdrfilm",
                "height": 16,
                "width": 16,
                "rfilter": {"type": "box"},
            },
        },
        "target_square": {  # Targeting square
            "type": "distantflux",
            "target": {
                "type": "rectangle",
                "to_world": mi.ScalarTransform4f.scale(surface_scale),
            },
            "sampler": {
                "type": "independent",
                "sample_count": 100,
            },
            "film": {
                "type": "hdrfilm",
                "height": 16,
                "width": 16,
                "rfilter": {"type": "box"},
            },
        },
        "target_square_small": {  # Targeting small square
            "type": "distantflux",
            "target": {
                "type": "rectangle",
                "to_world": mi.ScalarTransform4f.scale(0.5 * surface_scale),
            },
            "sampler": {
                "type": "independent",
                "sample_count": 100,
            },
            "film": {
                "type": "hdrfilm",
                "height": 16,
                "width": 16,
                "rfilter": {"type": "box"},
            },
        },
        "target_square_large": {  # Targeting large square
            "type": "distantflux",
            "target": {
                "type": "rectangle",
                "to_world": mi.ScalarTransform4f.scale(2.0 * surface_scale),
            },
            "sampler": {
                "type": "independent",
                "sample_count": 10000,
            },
            "film": {
                "type": "hdrfilm",
                "height": 16,
                "width": 16,
                "rfilter": {"type": "box"},
            },
        },
        "target_point": {  # Targeting point
            "type": "distantflux",
            "target": [0, 0, 0],
            "sampler": {
                "type": "independent",
                "sample_count": 100,
            },
            "film": {
                "type": "hdrfilm",
                "height": 16,
                "width": 16,
                "rfilter": {"type": "box"},
            },
        },
        "target_disk": {  # Targeting disk
            "type": "distantflux",
            "target": {
                "type": "disk",
                "to_world": mi.ScalarTransform4f.scale(surface_scale),
            },
            "sampler": {
                "type": "independent",
                "sample_count": 100,
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
            "to_world": mi.ScalarTransform4f.scale(surface_scale),
            "bsdf": {
                "type": "diffuse",
                "reflectance": rho,
            },
        },
        "emitter": {"type": "directional", "direction": w_e, "irradiance": l_e},
        "integrator": {"type": "path"},
    }

    scene = mi.load_dict({**scene_dict, "sensor": sensors[sensor_setup]})

    # Run simulation
    sensor = scene.sensors()[0]
    scene.integrator().render(scene, sensor)

    # Check result
    result = (
        np.array(
            sensor.film()
            .bitmap()
            .convert(mi.Bitmap.PixelFormat.Y, mi.Struct.Type.Float32, False)
        )
        .squeeze()
        .sum()
    )

    irradiance = (
        l_e * cos_theta_e
    )  # Irradiance accounting for slanting factor of targeted surface
    expected = {  # Special expected values for some cases
        "default": irradiance * (2.0 / np.pi),
        "target_square_large": irradiance * 0.25,
    }
    expected_value = expected.get(
        sensor_setup, irradiance
    )  # We expect that no energy is lost

    rtol = {  # Special tolerance values for some cases
        "default": 5e-3,
    }
    rtol_value = rtol.get(sensor_setup, 1e-3)

    assert np.allclose(result, expected_value, rtol=rtol_value)


def test_checkerboard(variant_scalar_rgb):
    """
    Very basic render test with checkboard texture and square target.
    """
    irradiance = 1.0
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
                    "to_uv": mi.ScalarTransform4f.scale(2),
                },
            },
        },
        "emitter": {
            "type": "directional",
            "direction": [0, 0, -1],
            "irradiance": irradiance,
        },
        "sensor0": {
            "type": "distantflux",
            "target": {"type": "rectangle"},
            "sampler": {
                "type": "independent",
                "sample_count": 100,
            },
            "film": {
                "type": "hdrfilm",
                "height": 16,
                "width": 16,
                "pixel_format": "luminance",
                "component_format": "float32",
                "rfilter": {"type": "box"},
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
        "integrator": {"type": "path"},
    }

    scene = mi.load_dict(scene_dict)

    sensor = scene.sensors()[0]
    scene.integrator().render(scene, sensor)

    data = np.array(sensor.film().bitmap()).squeeze().sum()

    expected = irradiance * 0.5 * (rho0 + rho1)
    assert np.allclose(data, expected, atol=1e-3)


@pytest.mark.slow
@pytest.mark.parametrize(
    "bsdf",
    [
        "diffuse",
        "roughconductor",
    ],
)
@pytest.mark.parametrize(
    "w_e",
    [[0, 0, -1], [0, 1, -1]],
)
def test_lobe(variant_scalar_rgb, bsdf, w_e):
    # Check if surfaces with a reflecting lobe are also handled correctly
    normal = [0, 0, 1]

    # Basic illumination parameters
    l_e = 1.0  # Emitted radiance
    w_e = list(w_e / np.linalg.norm(w_e))  # Emitter direction
    cos_theta_e = abs(np.dot(w_e, normal))

    # Reflecting surface specification
    surface_scale = 1.0
    bsdf_dict = {
        "diffuse": {"type": "diffuse", "reflectance": 1.0},
        "roughconductor": {"type": "roughconductor"},
    }[bsdf]

    # Sensor definitions
    # Targeting square
    sensor_dict = {
        "type": "distantflux",
        "target": {
            "type": "rectangle",
            "to_world": mi.ScalarTransform4f.scale(surface_scale),
        },
        "sampler": {
            "type": "independent",
            "sample_count": 10000,
        },
        "film": {
            "type": "hdrfilm",
            "height": 32,
            "width": 32,
            "rfilter": {"type": "box"},
        },
    }

    # Scene setup
    scene_dict = {
        "type": "scene",
        "shape": {
            "type": "rectangle",
            "to_world": mi.ScalarTransform4f.scale(surface_scale),
            "bsdf": bsdf_dict,
        },
        "emitter": {"type": "directional", "direction": w_e, "irradiance": l_e},
        "integrator": {"type": "path"},
    }

    scene = mi.load_dict({**scene_dict, "sensor": sensor_dict})

    # Run simulation
    sensor = scene.sensors()[0]
    scene.integrator().render(scene, sensor)

    # Check result
    result = (
        np.array(
            sensor.film()
            .bitmap()
            .convert(mi.Bitmap.PixelFormat.Y, mi.Struct.Type.Float32, False)
        )
        .squeeze()
        .sum()
    )

    # Irradiance accounting for slanting factor of targeted surface
    irradiance = l_e * cos_theta_e
    # We expect that no energy is lost
    expected_value = irradiance

    assert np.allclose(result, expected_value, rtol=1e-3)
