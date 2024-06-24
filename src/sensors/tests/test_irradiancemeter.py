import pytest
import drjit as dr
import mitsuba as mi


def sensor_shape_dict(radius, center):
    return {
        'type': 'sphere',
        'radius': radius,
        'to_world': mi.ScalarTransform4f().translate(center),
        'sensor': {
            'type': 'irradiancemeter',
            'film': {
                'type': 'hdrfilm',
                'width': 1,
                'height': 1,
                'rfilter': {'type': 'box'}
            },
        }
    }


def test_construct(variant_scalar_rgb):
    """
    We construct an irradiance meter attached to a sphere and assert that the
    following parameters get set correctly:
    - associated shape
    - film
    """

    center_v = mi.ScalarVector3f(0.0)
    radius = 1.0
    sphere = mi.load_dict(sensor_shape_dict(radius, center_v))
    sensor = sphere.sensor()

    assert sensor.get_shape() == sphere
    assert dr.allclose(sensor.film().size(), [1, 1])


@pytest.mark.parametrize(
    ("center", "radius"),
    [([2.0, 5.0, 8.3], 2.0), ([0.0, 0.0, 0.0], 1.0), ([1.0, 4.0, 0.0], 5.0)]
)
def test_sampling(variant_scalar_rgb, center, radius, np_rng):
    """
    We construct an irradiance meter attached to a sphere and assert that
    sampled rays originate at the sphere's surface
    """

    center_v = mi.ScalarVector3f(center)
    sphere = mi.load_dict(sensor_shape_dict(radius, center_v))
    sensor = sphere.sensor()
    num_samples = 100

    wav_samples = np_rng.random((num_samples,))
    pos_samples = np_rng.random((num_samples, 2))
    dir_samples = np_rng.random((num_samples, 2))

    for i in range(num_samples):
        ray = sensor.sample_ray_differential(
            0.0, wav_samples[i], pos_samples[i], dir_samples[i])[0]

        # assert that the ray starts at the sphere surface
        assert dr.allclose(dr.norm(center_v - ray.o), radius, atol=1e-4)
        # assert that all rays point away from the sphere center
        assert dr.dot(dr.normalize(ray.o - center_v), ray.d) > 0.0


def constant_emitter_dict(radiance):
    return {
        "type": "constant",
        "radiance": {"type": "uniform", "value": radiance}
    }


@pytest.mark.parametrize("radiance", [2.04, 1.0, 0.0])
def test_incoming_flux(variant_scalar_rgb, radiance, np_rng):
    """
    We test the recorded power density of the irradiance meter, by creating a simple scene:
    The irradiance meter is attached to a sphere with unit radius at the coordinate origin
    surrounded by a constant environment emitter.
    We sample a number of rays and average their contribution to the cumulative power
    density.
    We expect the average value to be \\pi * L with L the radiance of the constant
    emitter.
    """

    scene_dict = {
        'type': 'scene',
        'sensor': sensor_shape_dict(1, mi.ScalarVector3f(0, 0, 0)),
        'emitter': constant_emitter_dict(radiance)
    }

    scene = mi.load_dict(scene_dict)
    sensor = scene.sensors()[0]

    power_density_cum = 0.0
    num_samples = 100

    wav_samples = np_rng.random((num_samples,))
    pos_samples = np_rng.random((num_samples, 2))
    dir_samples = np_rng.random((num_samples, 2))

    for i in range(num_samples):
        ray, weight = sensor.sample_ray_differential(
            0.0, wav_samples[i], pos_samples[i], dir_samples[i])

        intersection = scene.ray_intersect(ray)
        power_density_cum += weight * \
            intersection.emitter(scene).eval(intersection)
    power_density_avg = power_density_cum / float(num_samples)

    assert dr.allclose(power_density_avg, mi.Spectrum(dr.pi * radiance))


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

    scene_dict = {
        'type': 'scene',
        'sensor': sensor_shape_dict(1, mi.ScalarVector3f(0, 0, 0)),
        'emitter': constant_emitter_dict(radiance),
        'integrator': {'type': 'path'}
    }

    scene = mi.load_dict(scene_dict)
    scene.integrator().render(scene, seed=0)
    film = scene.sensors()[0].film()

    img = film.bitmap(raw=True).convert(mi.Bitmap.PixelFormat.Y,
                                        mi.Struct.Type.Float32, srgb_gamma=False)

    assert dr.allclose(mi.TensorXf(img), (radiance * dr.pi))


def test_shape_accessors(variants_vec_rgb):
    center_v = mi.ScalarVector3f(0.0)
    radius = 1.0
    shape = mi.load_dict(sensor_shape_dict(radius, center_v))
    shape_ptr = mi.ShapePtr(shape)

    assert type(shape.sensor()) == mi.Sensor
    assert type(shape_ptr.sensor()) == mi.SensorPtr

    sensor = shape.sensor()
    sensor_ptr = mi.SensorPtr(sensor)

    assert type(sensor.get_shape()) == mi.Shape
    assert type(sensor_ptr.get_shape()) == mi.ShapePtr
