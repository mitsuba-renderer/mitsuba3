import pytest
import drjit as dr
import mitsuba as mi


def make_sensor(origin=None, direction=None, to_world=None, pixels=1):
    d = {
        "type": "radiancemeter",
        "film": {
            "type": "hdrfilm",
            "width": pixels,
            "height": pixels,
            "rfilter": {"type": "box"}
        }
    }

    if origin is not None:
        d["origin"] = origin
    if direction is not None:
        d["direction"] = direction
    if to_world is not None:
        d["to_world"] = to_world

    return mi.load_dict(d)


def test_construct(variant_scalar_rgb):
    # Test construct from to_world
    sensor = make_sensor(to_world=mi.ScalarTransform4f().look_at(
        origin=[0, 0, 0],
        target=[0, 1, 0],
        up=[0, 0, 1]
    ))
    assert not sensor.bbox().valid()  # Degenerate bounding box
    assert dr.allclose(
        sensor.world_transform().matrix,
        [[-1, 0, 0, 0],
         [0, 0, 1, 0],
         [0, 1, 0, 0],
         [0, 0, 0, 1]]
    )

    # Test construct from origin and direction
    sensor = make_sensor(origin=[0, 0, 0], direction=[0, 1, 0])
    assert not sensor.bbox().valid()  # Degenerate bounding box
    assert dr.allclose(
        sensor.world_transform().matrix,
        [[0, 1, 0, 0],
         [0, 0, 1, 0],
         [1, 0, 0, 0],
         [0, 0, 0, 1]]
    )

    # Test to_world overriding direction + origin
    sensor = make_sensor(
        to_world=mi.ScalarTransform4f().look_at(
            origin=[0, 0, 0],
            target=[0, 1, 0],
            up=[0, 0, 1]
        ),
        origin=[1, 0, 0],
        direction=[4, 1, 0]
    )
    assert not sensor.bbox().valid()  # Degenerate bounding box
    assert dr.allclose(
        sensor.world_transform().matrix,
        [[-1, 0, 0, 0],
         [0, 0, 1, 0],
         [0, 1, 0, 0],
         [0, 0, 0, 1]]
    )

    # Test raise on missing direction or origin
    with pytest.raises(RuntimeError):
        sensor = make_sensor(direction=[0, 1, 0])

    with pytest.raises(RuntimeError):
        sensor = make_sensor(origin=[0, 1, 0])

    # Test raise on wrong film size
    with pytest.raises(RuntimeError):
        sensor = make_sensor(pixels=2)


@pytest.mark.parametrize("direction", [[0.0, 0.0, 1.0], [-1.0, -1.0, 0.0], [2.0, 0.0, 0.0]])
@pytest.mark.parametrize("origin", [[0.0, 0.0, 0.0], [-1.0, -1.0, 0.5], [4.0, 1.0, 0.0]])
def test_sample_ray(variant_scalar_rgb, direction, origin):
    origin    = mi.Vector3f(origin)
    direction = mi.Vector3f(direction)
    sample1 = [0.32, 0.87]
    sample2 = [0.16, 0.44]

    sensor = make_sensor(direction=direction, origin=origin)

    # Test regular ray sampling
    ray = sensor.sample_ray(1., 1., sample1, sample2, True)
    assert dr.allclose(ray[0].o, origin, atol=1e-4)
    assert dr.allclose(ray[0].d, dr.normalize(direction))

    # Test ray differential sampling
    ray = sensor.sample_ray_differential(1., 1., sample2, sample1, True)
    assert dr.allclose(ray[0].o, origin, atol=1e-4)
    assert dr.allclose(ray[0].d, dr.normalize(direction))
    assert not ray[0].has_differentials


@pytest.mark.parametrize("radiance", [10**x for x in range(-3, 4)])
def test_render(variant_scalar_rgb, radiance):
    """Test render results with a simple scene"""
    spp = 1

    scene_dict = {
        'type': 'scene',
        'integrator': {
            'type': 'path'
        },
        'sensor': {
            'type': 'radiancemeter',
            'film': {
                'type': 'hdrfilm',
                'width': 1,
                'height': 1,
                'pixel_format': 'rgb',
                'rfilter': {
                    'type': 'box'
                }
            },
            'sampler': {
                'type': 'independent',
                'sample_count': spp
            }
        },
        'emitter': {
            'type': 'constant',
            'radiance': {
                'type': 'uniform',
                'value': radiance
            }
        }
    }

    scene = mi.load_dict(scene_dict)
    img = mi.render(scene)
    assert dr.allclose(img, radiance)
