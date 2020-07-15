import numpy as np
import pytest

import enoki as ek
import mitsuba


def sensor_shape_dict(radius, center):
    from mitsuba.core import ScalarTransform4f

    d = {
        "type": "sphere",
        "radius": radius,
        "to_world": ScalarTransform4f.translate(center),
        "sensor": {
            "type": "irradiancemeter",
            "film": {
                "type": "hdrfilm",
                "width": 1,
                "height": 1,
                "rfilter": {"type": "box"}
            },
        }
    }

    return d


def test_construct(variant_scalar_rgb):
    """We construct an irradiance meter attached to a sphere and assert that the
    following parameters get set correctly:
    - associated shape
    - film
    """
    from mitsuba.core import ScalarVector3f
    from mitsuba.core.xml import load_dict

    center_v = ScalarVector3f(0.0)
    radius = 1.0
    sphere = load_dict(sensor_shape_dict(radius, center_v))
    sensor = sphere.sensor()

    assert sensor.shape() == sphere
    assert ek.allclose(sensor.film().size(), [1, 1])


@pytest.mark.parametrize(
    ("center", "radius"),
    [([2.0, 5.0, 8.3], 2.0), ([0.0, 0.0, 0.0], 1.0), ([1.0, 4.0, 0.0], 5.0)]
)
def test_sampling(variant_scalar_rgb, center, radius):
    """We construct an irradiance meter attached to a sphere and assert that 
    sampled rays originate at the sphere's surface
    """
    from mitsuba.core import ScalarVector3f
    from mitsuba.core.xml import load_dict

    center_v = ScalarVector3f(center)
    sphere = load_dict(sensor_shape_dict(radius, center_v))
    sensor = sphere.sensor()
    num_samples = 100

    wav_samples = np.random.rand(num_samples)
    pos_samples = np.random.rand(num_samples, 2)
    dir_samples = np.random.rand(num_samples, 2)

    for i in range(100):
        ray = sensor.sample_ray_differential(
            0.0, wav_samples[i], pos_samples[i], dir_samples[i])[0]

        # assert that the ray starts at the sphere surface
        assert ek.allclose(ek.norm(center_v - ray.o), radius)
        # assert that all rays point away from the sphere center
        assert ek.dot(ek.normalize(ray.o - center_v), ray.d) > 0.0


def constant_emitter_dict(radiance):
    return {
        "type": "constant",
        "radiance": {"type": "uniform", "value": radiance}
    }


@pytest.mark.parametrize("radiance", [2.04, 1.0, 0.0])
def test_incoming_flux(variant_scalar_rgb, radiance):
    """
    We test the recorded power density of the irradiance meter, by creating a simple scene:
    The irradiance meter is attached to a sphere with unit radius at the coordinate origin
    surrounded by a constant environment emitter.
    We sample a number of rays and average their contribution to the cumulative power
    density.
    We expect the average value to be \\pi * L with L the radiance of the constant
    emitter.
    """
    from mitsuba.core import Spectrum, ScalarVector3f
    from mitsuba.core.xml import load_dict

    scene_dict = {
        "type": "scene",
        "sensor": sensor_shape_dict(1, ScalarVector3f(0, 0, 0)),
        "emitter": constant_emitter_dict(radiance)
    }

    scene = load_dict(scene_dict)
    sensor = scene.sensors()[0]

    power_density_cum = 0.0
    num_samples = 100

    wav_samples = np.random.rand(num_samples)
    pos_samples = np.random.rand(num_samples, 2)
    dir_samples = np.random.rand(num_samples, 2)

    for i in range(100):
        ray, weight = sensor.sample_ray_differential(
            0.0, wav_samples[i], pos_samples[i], dir_samples[i])

        intersection = scene.ray_intersect(ray)
        power_density_cum += weight * \
            intersection.emitter(scene).eval(intersection)
    power_density_avg = power_density_cum / float(num_samples)

    assert ek.allclose(power_density_avg, Spectrum(ek.pi * radiance))


@pytest.mark.parametrize("radiance", [2.04, 1.0, 0.0])
def test_incoming_flux_integrator(variant_scalar_rgb, radiance):
    """
    We test the recorded power density of the irradiance meter, by creating a simple scene:
    The irradiance meter is attached to a sphere with unit radius at the coordinate origin
    surrounded by a constant environment emitter.
    We render the scene with the path tracer integrator and compare the recorded  power
    density with our theoretical expectation.
    We expect the average value to be \\pi * L with L the radiance of the constant
    emitter.
    """

    from mitsuba.core import Spectrum, Bitmap, Struct, ScalarVector3f
    from mitsuba.core.xml import load_dict

    scene_dict = {
        "type": "scene",
        "sensor": sensor_shape_dict(1, ScalarVector3f(0, 0, 0)),
        "emitter": constant_emitter_dict(radiance),
        "integrator": {"type": "path"}
    }

    scene = load_dict(scene_dict)
    sensor = scene.sensors()[0]

    scene.integrator().render(scene, sensor)
    film = sensor.film()

    img = film.bitmap(raw=True).convert(Bitmap.PixelFormat.Y,
                                        Struct.Type.Float32, srgb_gamma=False)
    image_np = np.array(img)

    ek.allclose(image_np, (radiance * ek.pi))
