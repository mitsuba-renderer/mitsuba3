import random

import drjit as dr
import mitsuba as mi
import numpy as np
import pytest


def dict_sensor(orig, dirs, pixels):
    dict = {
        "type": "mradiancemeter",
        "origins": orig,
        "directions": dirs,
        "film": {
            "type": "hdrfilm",
            "width": pixels,
            "height": 1,
            "rfilter": {"type": "box"},
        },
    }
    return dict


def dict_scene(origins, directions, width, spp, radiance):
    scene_dict = {
        "type": "scene",
        "integrator": {"type": "path"},
        "sensor": {
            "type": "mradiancemeter",
            "origins": origins,
            "directions": directions,
            "film": {
                "type": "hdrfilm",
                "width": width,
                "height": 1,
                "pixel_format": "rgb",
                "rfilter": {"type": "box"},
            },
            "sampler": {
                "type": "independent",
                "sample_count": spp,
            },
        },
        "emitter": {
            "type": "constant",
            "radiance": {"type": "uniform", "value": radiance},
        },
    }
    return scene_dict


def test_construct(variant_scalar_rgb):
    # Instantiate with 1 or 2 direction
    for origins, directions in [
        [["0, 0, 0"], ["1, 0, 0"]],
        [["0, 0, 0"] * 2, ["1, 0, 0", "-1, 0, 0"]],
    ]:

        sensor = mi.load_dict(
            dict_sensor(
                ", ".join(origins),
                ", ".join(directions),
                len(origins),
            )
        )
        assert sensor is not None
        assert not sensor.bbox().valid()  # Degenerate bounding box

    # Instantiation is impossible with ill-formed specification
    with pytest.raises(RuntimeError):  # Wrong film size
        mi.load_dict(dict_sensor("0, 0, 0", "1, 0, 0", 2))
    with pytest.raises(RuntimeError):  # Wrong vector size
        mi.load_dict(dict_sensor("0, 0", "1, 0", 1))
    with pytest.raises(RuntimeError):  # Vector size mismatch
        mi.load_dict(dict_sensor("0, 0, 0", "1, 0", 1))


def test_sample_ray_compare(variant_scalar_rgb):
    """
    Verify that the single-sensor setup yields results identical with those
    produced by the radiancemeter plugin.
    """
    for origin, direction in [
        ([-1, 0, 0], [-2, 1, -10]),
        ([0, 0, 0], [0, 1, 0]),
        ([0, 1, 1], [1, 3, 2]),
    ]:
        mradiancemeter = mi.load_dict(
            {
                "type": "mradiancemeter",
                "origins": ",".join([str(x) for x in origin]),
                "directions": ",".join([str(x) for x in direction]),
                "film": {
                    "type": "hdrfilm",
                    "width": 1,
                    "height": 1,
                    "rfilter": {"type": "box"},
                },
            }
        )
        radiancemeter = mi.load_dict(
            {
                "type": "radiancemeter",
                "origin": origin,
                "direction": direction,
                "film": {
                    "type": "hdrfilm",
                    "width": 1,
                    "height": 1,
                    "rfilter": {"type": "box"},
                },
            }
        )

        for film_sample in [
            [0.2, 0.8],
            [0.3, 0.2],
            [0.9, 0.3],
            [0.1, 0.6],
        ]:
            wavelength_sample = 0.0
            time = 0.0
            aperture_sample = (0.0, 0.0)

            ray_rad, _ = radiancemeter.sample_ray(
                time, wavelength_sample, film_sample, aperture_sample
            )
            ray_mrad, _ = mradiancemeter.sample_ray(
                time, wavelength_sample, film_sample, aperture_sample
            )

            assert dr.allclose(ray_mrad.o, ray_rad.o, atol=1e-4)
            assert dr.allclose(ray_mrad.d, ray_rad.d)


def test_sample_ray_multi(variant_scalar_rgb):
    """
    Test ray sampling by instantiating a sensor with two components. We sample
    rays with different values for the position sample and assert, that the
    correct component is picked.
    """
    sensor = mi.load_dict(dict_sensor("0, 0, 0, 1, 0, 1", "1, 0, 0, 1, 1, 1", 2))

    random.seed(42)
    for _ in range(10):
        wavelength_sample = random.random()
        position_sample = (random.random(), random.random())
        ray = sensor.sample_ray_differential(
            0, wavelength_sample, position_sample, (0, 0), True
        )[0]

        if position_sample[0] < 0.5:
            assert dr.allclose(ray.o, [0, 0, 0])
            assert dr.allclose(ray.d, [1, 0, 0])
        else:
            assert dr.allclose(ray.o, [1, 0, 1])
            assert dr.allclose(ray.d, dr.normalize(mi.ScalarVector3f(1, 1, 1)))


@pytest.mark.parametrize("radiance", [10**x for x in range(-3, 4)])
def test_render(variant_scalar_rgb, radiance):
    """
    Test render results with a simple scene.
    """
    scene_dict = dict_scene(
        "1, 0, 0, 0, 1, 0, 0, 0, 1", "1, 0, 0, 0, -1, 0, 0, 0, -1", 3, 1, radiance
    )

    scene = mi.load_dict(scene_dict)
    img = mi.render(scene)
    assert np.allclose(np.array(img), radiance)


def test_render_complex(variant_scalar_rgb):
    """
    Test render results for a more complex scene.
    The mradiancemeter has three components, pointing at three
    surfaces with different reflectances.
    """
    scene_dict = {
        "type": "scene",
        "integrator": {"type": "path"},
        "sensor": {
            "type": "mradiancemeter",
            "origins": "-2, 0, 1, 0, 0, 1, 2, 0, 1",
            "directions": "0, 0, -1, 0, 0, -1 0, 0, -1",
            "film": {
                "type": "hdrfilm",
                "width": 3,
                "height": 1,
                "pixel_format": "luminance",
                "rfilter": {"type": "box"},
            },
            "sampler": {
                "type": "independent",
                "sample_count": 1,
            },
        },
        "emitter": {
            "type": "directional",
            "direction": (0, 0, -1),
            "irradiance": {"type": "uniform", "value": 1},
        },
        "light_rectangle": {
            "type": "rectangle",
            "to_world": mi.ScalarTransform4f.translate((-2, 0, 0)),
            "bsdf": {
                "type": "diffuse",
                "reflectance": {"type": "uniform", "value": 1.0},
            },
        },
        "medium_rectangle": {
            "type": "rectangle",
            "to_world": mi.ScalarTransform4f.translate((0, 0, 0)),
            "bsdf": {
                "type": "diffuse",
                "reflectance": {"type": "uniform", "value": 0.5},
            },
        },
        "dark_rectangle": {
            "type": "rectangle",
            "to_world": mi.ScalarTransform4f.translate((2, 0, 0)),
            "bsdf": {
                "type": "diffuse",
                "reflectance": {"type": "uniform", "value": 0.0},
            },
        },
    }

    scene = mi.load_dict(scene_dict)
    img = mi.render(scene)
    data = np.squeeze(np.array(img))

    assert np.isclose(data[0] / data[1], 2, atol=1e-3)
    assert data[2] == 0
