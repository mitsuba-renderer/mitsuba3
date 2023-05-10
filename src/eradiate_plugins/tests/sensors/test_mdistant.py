import drjit as dr
import mitsuba as mi
import numpy as np
import pytest


def sensor_dict(target=None, directions="0, 0, 1", pixel_count=1):
    result = {
        "type": "mdistant",
        "directions": directions,
        "film": {
            "type": "hdrfilm",
            "width": pixel_count,
            "height": 1,
            "rfilter": {"type": "box"},
        },
    }

    if target == "point":
        result.update({"target": [0, 0, 0]})

    elif target == "shape":
        result.update({"target": {"type": "rectangle"}})

    elif isinstance(target, dict):
        result.update({"target": target})

    return result


def test_construct(variant_scalar_rgb):
    directions = ",".join([str(x) for x in [0, 0, 1, 0, 0, -1]])
    # Constructing with properly build params succeeds
    sensor = mi.load_dict(
        {
            "type": "mdistant",
            "directions": directions,
            "film": {
                "type": "hdrfilm",
                "width": 2,
                "height": 1,
                "rfilter": {"type": "box"},
            },
        }
    )

    assert sensor is not None
    assert not sensor.bbox().valid()  # Degenerate bounding box

    # Constructing without parameters raises
    with pytest.raises(RuntimeError):
        mi.load_dict({"type": "mdistant"})

    # Constructing with inappropriate film size raises
    with pytest.raises(RuntimeError):
        mi.load_dict(
            {
                "type": "mdistant",
                "directions": directions,
                "film": {
                    "type": "hdrfilm",
                    "width": 2,
                    "height": 2,
                },
            }
        )

    # Test different combinations of target and origin values
    # -- No target
    sensor = mi.load_dict(sensor_dict(directions=directions, pixel_count=2))
    assert sensor is not None

    # -- Point target
    sensor = mi.load_dict(
        sensor_dict(target="point", directions=directions, pixel_count=2)
    )
    assert sensor is not None

    # -- Shape target
    sensor = mi.load_dict(
        sensor_dict(target="shape", directions=directions, pixel_count=2)
    )
    assert sensor is not None

    # -- Random object target (we expect to raise)
    with pytest.raises(RuntimeError):
        mi.load_dict(
            sensor_dict(
                target={"type": "constant"}, directions=directions, pixel_count=2
            )
        )


def test_sample_ray_direction(variant_scalar_rgb):
    directions = "0, 0, -1, -1, -1, 0, -2, 0, 0"
    sensor = mi.load_dict(
        sensor_dict(directions=directions, pixel_count=3, target="point")
    )

    # Check that directions are appropriately set
    for (sample1, sample2, expected) in [
        [[0.32, 0.87], [0.16, 0.44], [0, 0, -1]],
        [[0.17, 0.44], [0.22, 0.81], [0, 0, -1]],
        [[0.51, 0.82], [0.99, 0.42], [-1, -1, 0]],
        [[0.72, 0.40], [0.01, 0.61], [-2, 0, 0]],
    ]:
        ray, _ = sensor.sample_ray(1.0, 1.0, sample1, sample2, True)

        # Check that ray direction is what is expected
        assert dr.allclose(ray.d, dr.normalize(mi.ScalarVector3f(expected)))


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
@pytest.mark.parametrize("w_o", [[0, 0, -1], [0, -1, -1]])
@pytest.mark.slow
def test_sample_target(variant_scalar_rgb, sensor_setup, w_e, w_o):
    """
    This test checks if targeting works as intended by rendering a basic scene
    """
    # Basic illumination and sensing parameters
    l_e = 1.0  # Emitted radiance
    w_e = list(w_e / np.linalg.norm(w_e))  # Emitter direction
    w_o = list(w_o / np.linalg.norm(w_o))  # Sensor direction
    cos_theta_e = abs(np.dot(w_e, [0, 0, 1]))
    cos_theta_o = abs(np.dot(w_o, [0, 0, 1]))

    # Reflecting surface specification
    surface_scale = 1.0
    rho = 1.0  # Surface reflectance

    # Sensor definitions
    directions = ",".join(map(str, w_o))
    sensors = {
        "default": {  # No target
            "type": "mdistant",
            "directions": directions,
            "sampler": {
                "type": "independent",
                "sample_count": 100000,
            },
            "film": {
                "type": "hdrfilm",
                "height": 1,
                "width": 1,
                "rfilter": {"type": "box"},
            },
        },
        "target_square": {  # Targeting square
            "type": "mdistant",
            "directions": directions,
            "target": {
                "type": "rectangle",
                "to_world": mi.ScalarTransform4f.scale(surface_scale),
            },
            "sampler": {
                "type": "independent",
                "sample_count": 100000,
            },
            "film": {
                "type": "hdrfilm",
                "height": 1,
                "width": 1,
                "rfilter": {"type": "box"},
            },
        },
        "target_square_small": {  # Targeting small square
            "type": "mdistant",
            "directions": directions,
            "target": {
                "type": "rectangle",
                "to_world": mi.ScalarTransform4f.scale(0.5 * surface_scale),
            },
            "sampler": {
                "type": "independent",
                "sample_count": 100000,
            },
            "film": {
                "type": "hdrfilm",
                "height": 1,
                "width": 1,
                "rfilter": {"type": "box"},
            },
        },
        "target_square_large": {  # Targeting large square
            "type": "mdistant",
            "directions": directions,
            "target": {
                "type": "rectangle",
                "to_world": mi.ScalarTransform4f.scale(2.0 * surface_scale),
            },
            "sampler": {
                "type": "independent",
                "sample_count": 100000,
            },
            "film": {
                "type": "hdrfilm",
                "height": 1,
                "width": 1,
                "rfilter": {"type": "box"},
            },
        },
        "target_point": {  # Targeting point
            "type": "mdistant",
            "directions": directions,
            "target": [0, 0, 0],
            "sampler": {
                "type": "independent",
                "sample_count": 100000,
            },
            "film": {
                "type": "hdrfilm",
                "height": 1,
                "width": 1,
                "rfilter": {"type": "box"},
            },
        },
        "target_disk": {  # Targeting disk
            "type": "mdistant",
            "directions": directions,
            "target": {
                "type": "disk",
                "to_world": mi.ScalarTransform4f.scale(surface_scale),
            },
            "sampler": {
                "type": "independent",
                "sample_count": 100000,
            },
            "film": {
                "type": "hdrfilm",
                "height": 1,
                "width": 1,
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

    # Run simulation
    scene = mi.load_dict({**scene_dict, "sensor": sensors[sensor_setup]})
    sensor = scene.sensors()[0]
    scene.integrator().render(scene, sensor)

    # Check result
    result = np.array(
        sensor.film()
        .bitmap()
        .convert(mi.Bitmap.PixelFormat.RGB, mi.Struct.Type.Float32, False)
    ).squeeze()

    l_o = l_e * cos_theta_e * rho / np.pi  # Outgoing radiance
    expected = {  # Special expected values for some cases (when rays are "lost")
        "default": l_o * (2.0 / np.pi) * cos_theta_o,
        "target_square_large": l_o * 0.25,
    }
    expected_value = expected.get(sensor_setup, l_o)

    rtol = {  # Special tolerance values for some cases
        "target_square_large": 1e-2,
    }
    rtol_value = rtol.get(sensor_setup, 5e-3)

    assert np.allclose(result, expected_value, rtol=rtol_value)


def test_checkerboard(variant_scalar_rgb):
    """
    Very basic render test with checkerboard texture and square target.
    """
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
                    "to_uv": mi.ScalarTransform4f.scale(2),
                },
            },
        },
        "emitter": {
            "type": "directional",
            "direction": [0, 0, -1],
            "irradiance": 1.0,
        },
        "sensor0": {
            "type": "mdistant",
            "directions": ",".join(map(str, [0, 0, -1, 0, 1, -1, 1, 1, -1])),
            "target": {"type": "rectangle"},
            "sampler": {
                "type": "independent",
                "sample_count": 10000,
            },
            "film": {
                "type": "hdrfilm",
                "width": 3,
                "height": 1,
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
        #         "rfilter": {"type": "box"},
        #     },
        # },
        "integrator": {"type": "path"},
    }

    scene = mi.load_dict(scene_dict)
    sensor = scene.sensors()[0]
    scene.integrator().render(scene, sensor)
    result = np.array(
        sensor.film()
        .bitmap()
        .convert(mi.Bitmap.PixelFormat.RGB, mi.Struct.Type.Float32, False)
    ).squeeze()

    expected = l_o * 0.5 * (rho0 + rho1) / np.pi
    assert np.allclose(result, expected, atol=1e-3)


@pytest.mark.parametrize("offset", [0.9, 0.5, 0.1])
@pytest.mark.parametrize("sigma", [0.1, 0.5, 1.0])
def test_ray_offset(variant_scalar_rgb, offset, sigma):
    """
    We consider the very simple case of a uniform and absorbing medium with
    absorbing coefficient σ under directional illumination above a diffuse
    surface with reflectance ρ.
    The reflected radiance is equal to ρ/π * exp(-σL) where L is the geometric
    distance through which light travels until it reaches the sensor.
    This test checks that the mdistant plugin can compute this correctly.
    """

    rho = 1.0  # diffuse surface reflectance
    l = 1.0  # thickness of the medium
    scene = mi.load_dict(
        {
            "type": "scene",
            "medium": {
                "type": "homogeneous",
                "sigma_t": sigma,
                "albedo": 0.0,
                "phase": {"type": "isotropic"},
            },
            "cube": {
                "type": "cube",
                "bsdf": {"type": "null"},
                "interior": {"type": "ref", "id": "medium"},
                "to_world": mi.ScalarTransform4f.scale([1, 1, l]),
            },
            "rectangle": {
                "type": "rectangle",
                "bsdf": {"type": "diffuse", "reflectance": rho},
            },
            "illumination": {
                "type": "directional",
                "direction": [0.0, 0.0, -1.0],
                "irradiance": 1.0,
            },
            "integrator": {"type": "volpath"},
            "measure": {
                "type": "mdistant",
                "id": "measure",
                "directions": "0,0,-1",
                "sampler": {"type": "independent"},
                "film": {
                    "type": "hdrfilm",
                    "width": 1,
                    "height": 1,
                    "pixel_format": "luminance",
                    "component_format": "float32",
                    "rfilter": {"type": "box"},
                },
                "ray_offset": offset,
                "target": [0, 0, 0],
                "medium": {"type": "ref", "id": "medium"},
            },
        }
    )
    result = np.squeeze(mi.render(scene, spp=4**8))
    expected = (
        rho * np.exp(-sigma * (l + offset)) / np.pi
    )  # geometric distance is l + offset
    # Fairly loose criterion, but a stricter one requires too many samples
    assert np.isclose(result, expected, rtol=1e-2), f"{result = }, {expected = }"
