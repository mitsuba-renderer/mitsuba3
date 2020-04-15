import numpy as np
import pytest

import enoki as ek
import mitsuba


def example_shape(radius, center):
    from mitsuba.core.xml import load_string

    xml = f"""
    <shape version='0.1.0' type="sphere">
        <float name="radius" value="{radius}"/>
        <transform name="to_world">
            <translate x="{center.x}" y="{center.y}" z="{center.z}"/>
        </transform>
        <sensor type="irradiancemeter">
            <film type="hdrfilm">
                <integer name="width" value="1"/>
                <integer name="height" value="1"/>
            </film>
        </sensor>
    </shape>
    """
    return load_string(xml)

def test_construct(variant_scalar_rgb):
    """
    We construct an irradiance meter attached to a sphere and assert that the
    following parameters get set correctly:
    - associated shape
    - film
    """
    from mitsuba.core import Vector3f
    center_v = Vector3f(0.0)
    radius = 1.0
    sphere = example_shape(radius, center_v)
    sensor = sphere.sensor()

    assert sensor.shape() == sphere
    assert ek.allclose(sensor.film().size(), [1, 1])


@pytest.mark.parametrize(("center", "radius"), [([2.0, 5.0, 8.3], 2.0), ([0.0, 0.0, 0.0], 1.0), ([1.0, 4.0, 0.0], 5.0)])
def test_sampling(variant_scalar_rgb, center, radius):
    """
    We construct an irradiance meter attached to a sphere and assert that sampled
    rays originate at the sphere's surface
    """
    from mitsuba.core import Vector3f

    center_v = Vector3f(center)
    sphere = example_shape(radius, center_v)
    sensor = sphere.sensor()
    num_samples = 100

    wav_samples = np.random.rand(num_samples)
    pos_samples = np.random.rand(num_samples, 2)
    dir_samples = np.random.rand(num_samples, 2)

    for i in range(100):
        ray = sensor.sample_ray_differential(0.0, wav_samples[i], pos_samples[i], dir_samples[i])[0]

        # assert that the ray starts at the sphere surface
        assert ek.allclose(ek.norm(center_v - ray.o), radius)
        # assert that all rays point away from the sphere center
        assert ek.dot(ek.normalize(ray.o - center_v), ray.d) > 0.0

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
    from mitsuba.core import Spectrum
    from mitsuba.core.xml import load_string

    sensor_xml = f"""
    <shape version='0.1.0' type="sphere">
        <float name="radius" value="1"/>
        <transform name="to_world">
            <translate x="0" y="0" z="0"/>
        </transform>
        <sensor type="irradiancemeter">
            <film type="hdrfilm">
                <integer name="width" value="1"/>
                <integer name="height" value="1"/>
            </film>
        </sensor>
    </shape>
    """

    emitter_xml = f"""
    <emitter type="constant">
        <spectrum name="radiance" type='uniform'>
        	<float name="value" value="{radiance}"/>
    	</spectrum>
    </emitter>
    """

    scene_xml = f"""
        <scene version="0.1.0">
            {sensor_xml}
            {emitter_xml}
        </scene>
    """
    scene = load_string(scene_xml)
    sensor = scene.sensors()[0]

    power_density_cum = 0.0
    num_samples = 100

    wav_samples = np.random.rand(num_samples)
    pos_samples = np.random.rand(num_samples, 2)
    dir_samples = np.random.rand(num_samples, 2)

    for i in range(100):
        ray, weight = sensor.sample_ray_differential(0.0, wav_samples[i], pos_samples[i], dir_samples[i])

        intersection = scene.ray_intersect(ray)
        power_density_cum += weight * intersection.emitter(scene).eval(intersection)
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

    from mitsuba.core import Spectrum, Bitmap, Struct
    from mitsuba.core.xml import load_string

    sensor_xml = f"""
    <shape version='0.1.0' type="sphere">
        <float name="radius" value="1"/>
        <transform name="to_world">
            <translate x="0" y="0" z="0"/>
        </transform>
        <sensor type="irradiancemeter">
            <film type="hdrfilm">
                <integer name="width" value="1"/>
                <integer name="height" value="1"/>
            </film>
        </sensor>
    </shape>
    """

    emitter_xml = f"""
    <emitter type="constant">
        <spectrum name="radiance" type='uniform'>
        	<float name="value" value="{radiance}"/>
    	</spectrum>
    </emitter>
    """

    integrator_xml = f"""
    <integrator type="path">

        <integer name="max_depth" value="-1"/>
    </integrator>
    """

    sampler_xml = f"""
     <sampler type="independent">
          <integer name="sample_count" value="100"/>
     </sampler>
    """
    scene_xml = f"""
        <scene version="0.1.0">
            {integrator_xml}
            {sensor_xml}
            {emitter_xml}
            {sampler_xml}
        </scene>
    """
    scene = load_string(scene_xml)
    sensor = scene.sensors()[0]

    scene.integrator().render(scene, sensor)
    film = sensor.film()

    img = film.bitmap(raw=True).convert(Bitmap.PixelFormat.Y, Struct.Type.Float32, srgb_gamma=False)
    image_np = np.array(img)

    ek.allclose(image_np, (radiance*ek.pi))
