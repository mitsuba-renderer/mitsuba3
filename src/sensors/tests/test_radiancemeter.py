import enoki as ek
import mitsuba
import pytest


def xml_sensor(params="", pixels="1"):
    xml = f"""
        <sensor version="2.0.0" type="radiancemeter">
            <film type="hdrfilm">
                <integer name="width" value="{pixels}"/>
                <integer name="height" value="{pixels}"/>
                <rfilter type="box"/>
            </film>
            {params}
        </sensor>
    """
    return xml


def xml_lookat(origin, target, up="0,0,1"):
    xml = f"""
        <transform name="to_world">
            <lookat origin="{origin}" target="{target}" up="{up}"/>
        </transform>
    """
    return xml


def xml_origin(value):
    return f"""<point name="origin" value="{value}"/>"""


def xml_direction(value):
    return f"""<vector name="direction" value="{value}"/>"""


def example_sensor(params="", pixels="1"):
    from mitsuba.core.xml import load_string
    xml = xml_sensor(params, pixels)
    return load_string(xml)


def test_construct(variant_scalar_rgb):
    from mitsuba.core.xml import load_string

    # Test construct from to_world
    only_to_world = xml_sensor(
        params=xml_lookat(origin="0,0,0", target="0,1,0")
    )
    sensor = load_string(only_to_world)
    assert not sensor.bbox().valid()  # Degenerate bounding box
    assert ek.allclose(
        sensor.world_transform().eval(0.).matrix,
        [[-1, 0, 0, 0],
         [0, 0, 1, 0],
         [0, 1, 0, 0],
         [0, 0, 0, 1]]
    )

    # Test construct from origin and direction
    origin_direction = xml_sensor(
        params=xml_origin("0,0,0") + xml_direction("0,1,0")
    )
    sensor = load_string(origin_direction)
    assert not sensor.bbox().valid()  # Degenerate bounding box
    assert ek.allclose(
        sensor.world_transform().eval(0.).matrix,
        [[0, 0, 1, 0],
         [1, 0, 0, 0],
         [0, 1, 0, 0],
         [0, 0, 0, 1]]
    )

    # Test to_world overriding direction + origin
    to_world_origin_direction = xml_sensor(
        params=xml_lookat(origin="0,0,0", target="0,1,0") +
        xml_origin("1,0,0") + xml_direction("4,1,0")
    )
    sensor = load_string(to_world_origin_direction)
    assert not sensor.bbox().valid()  # Degenerate bounding box
    assert ek.allclose(
        sensor.world_transform().eval(0.).matrix,
        [[-1, 0, 0, 0],
         [0, 0, 1, 0],
         [0, 1, 0, 0],
         [0, 0, 0, 1]]
    )

    # Test raise on missing direction or origin
    only_direction = xml_sensor(params=xml_direction("0,1,0"))
    with pytest.raises(RuntimeError):
        sensor = load_string(only_direction)

    only_origin = xml_sensor(params=xml_origin("0,1,0"))
    with pytest.raises(RuntimeError):
        sensor = load_string(only_origin)

    # Test raise on wrong film size
    with pytest.raises(RuntimeError):
        sensor = example_sensor(
            params=xml_lookat(origin="0,0,-2", target="0,0,0"),
            pixels=2
        )


@pytest.mark.parametrize("direction", [[0.0, 0.0, 1.0], [-1.0, -1.0, 0.0], [2.0, 0.0, 0.0]])
@pytest.mark.parametrize("origin", [[0.0, 0.0, 0.0], [-1.0, -1.0, 0.5], [4.0, 1.0, 0.0]])
def test_sample_ray(variant_scalar_rgb, direction, origin):
    sample1 = [0.32, 0.87]
    sample2 = [0.16, 0.44]
    direction_str = ",".join([str(x) for x in direction])
    origin_str = ",".join([str(x) for x in origin])
    sensor = example_sensor(
        params=xml_direction(direction_str) + xml_origin(origin_str)
    )

    # Test regular ray sampling
    ray = sensor.sample_ray(1., 1., sample1, sample2, True)
    assert ek.allclose(ray[0].o, origin)
    assert ek.allclose(ray[0].d, ek.normalize(direction))

    # Test ray differential sampling
    ray = sensor.sample_ray_differential(1., 1., sample2, sample1, True)
    assert ek.allclose(ray[0].o, origin)
    assert ek.allclose(ray[0].d, ek.normalize(direction))
    assert not ray[0].has_differentials


@pytest.mark.parametrize("radiance", [10**x for x in range(-3, 4)])
def test_render(variant_scalar_rgb, radiance):
    # Test render results with a simple scene
    from mitsuba.core.xml import load_string
    import numpy as np

    scene_xml = """
    <scene version="2.0.0">
        <default name="radiance" value="1.0"/>
        <default name="spp" value="1"/>

        <integrator type="path"/>

        <sensor type="radiancemeter">
            <film type="hdrfilm">
                <integer name="width" value="1"/>
                <integer name="height" value="1"/>
                <string name="pixel_format" value="rgb"/>
                <rfilter type="box"/>
            </film>

            <sampler type="independent">
                <integer name="sample_count" value="$spp"/>
            </sampler>
        </sensor>

        <emitter type="constant">
            <spectrum name="radiance" value="$radiance"/>
        </emitter>
    </scene>
    """

    scene = load_string(scene_xml, spp=1, radiance=radiance)
    sensor = scene.sensors()[0]
    scene.integrator().render(scene, sensor)
    img = sensor.film().bitmap()
    assert np.allclose(np.array(img), radiance)
