import pytest
import drjit as dr
import mitsuba as mi


def create_camera(o, d, fov=34, fov_axis="x", s_open=1.5, s_close=5, near_clip=1.0):
    t = [o[0] + d[0], o[1] + d[1], o[2] + d[2]]

    camera_dict = {
        "type": "perspective",
        "near_clip": near_clip,
        "far_clip": 35.0,
        "focus_distance": 15.0,
        "fov": fov,
        "fov_axis": fov_axis,
        "shutter_open": s_open,
        "shutter_close": s_close,
        "to_world": mi.ScalarTransform4f().look_at(
            origin=o,
            target=t,
            up=[0, 1, 0]
        ),
        "film": {
            "type": "hdrfilm",
            "width": 512,
            "height": 256,
        }
    }

    return mi.load_dict(camera_dict)


origins = [[1.0, 0.0, 1.5], [1.0, 4.0, 1.5]]
directions = [[0.0, 0.0, 1.0], [1.0, 0.0, 0.0]]


@pytest.mark.parametrize("origin", origins)
@pytest.mark.parametrize("direction", directions)
@pytest.mark.parametrize("s_open", [0.0, 1.5])
@pytest.mark.parametrize("s_time", [0.0, 3.0])
def test01_create(variant_scalar_rgb, origin, direction, s_open, s_time):
    camera = create_camera(origin, direction, s_open=s_open, s_close=s_open + s_time)

    assert dr.allclose(camera.near_clip(), 1)
    assert dr.allclose(camera.far_clip(), 35)
    assert dr.allclose(camera.focus_distance(), 15)
    assert dr.allclose(camera.shutter_open(), s_open)
    assert dr.allclose(camera.shutter_open_time(), s_time)
    assert not camera.needs_aperture_sample()
    assert camera.bbox() == mi.BoundingBox3f(origin, origin)
    assert dr.allclose(camera.world_transform().matrix,
                       mi.Transform4f().look_at(origin, mi.Vector3f(origin) + direction, [0, 1, 0]).matrix)


@pytest.mark.parametrize("origin", origins)
@pytest.mark.parametrize("direction", directions)
def test02_sample_ray(variants_vec_spectral, origin, direction):
    """Check the correctness of the sample_ray() method"""
    near_clip = 1.0
    camera = create_camera(origin, direction, near_clip=near_clip)

    time = 0.5
    wav_sample = [0.5, 0.33, 0.1]
    pos_sample = [[0.2, 0.1, 0.2], [0.6, 0.9, 0.2]]
    aperture_sample = 0 # Not being used

    ray, spec_weight = camera.sample_ray(time, wav_sample, pos_sample, aperture_sample)

    # Importance sample wavelength and weight
    wav, spec = mi.sample_rgb_spectrum(mi.sample_shifted(wav_sample))

    assert dr.allclose(ray.wavelengths, wav)
    assert dr.allclose(mi.unpolarized_spectrum(spec_weight), spec)
    assert dr.allclose(ray.time, time)

    inv_z = dr.rcp((camera.world_transform().inverse() @ ray.d).z)
    o = mi.Point3f(origin) + near_clip * inv_z * mi.Vector3f(ray.d)
    assert dr.allclose(ray.o, o, atol=1e-4)

    # Check that a [0.5, 0.5] position_sample generates a ray
    # that points in the camera direction
    ray, _ = camera.sample_ray(0, 0, [0.5, 0.5], 0)
    assert dr.allclose(ray.d, direction, atol=1e-7)



@pytest.mark.parametrize("origin", origins)
@pytest.mark.parametrize("direction", directions)
def test03_sample_ray(variants_vec_spectral, origin, direction):
    """Check the correctness of the sample_ray() method"""
    near_clip = 1.0
    camera = create_camera(origin, direction, near_clip=near_clip)

    time = 0.5
    wav_sample = [0.5, 0.33, 0.1]
    pos_sample = [[0.2, 0.1, 0.2], [0.6, 0.9, 0.2]]

    ray, spec_weight = camera.sample_ray(time, wav_sample, pos_sample, 0)

    # Importance sample wavelength and weight
    wav, spec = mi.sample_rgb_spectrum(mi.sample_shifted(wav_sample))

    assert dr.allclose(ray.wavelengths, wav)
    assert dr.allclose(mi.unpolarized_spectrum(spec_weight), spec)
    assert dr.allclose(ray.time, time)

    inv_z = dr.rcp((camera.world_transform().inverse() @ ray.d).z)
    o = mi.Point3f(origin) + near_clip * inv_z * mi.Vector3f(ray.d)
    assert dr.allclose(ray.o, o, atol=1e-4)

    # Check that a [0.5, 0.5] position_sample generates a ray
    # that points in the camera direction
    ray_center, _ = camera.sample_ray(0, 0, [0.5, 0.5], 0)
    assert dr.allclose(ray_center.d, direction, atol=1e-7)


@pytest.mark.parametrize("origin", [[1.0, 0.0, 1.5]])
@pytest.mark.parametrize("direction", [[0.0, 0.0, 1.0], [1.0, 0.0, 0.0]])
@pytest.mark.parametrize("fov", [34, 80])
def test04_fov_axis(variants_vec_spectral, origin, direction, fov):
    """
    Check that sampling position_sample at the extremities of the unit square
    along the fov_axis should generate a ray direction that make angle of fov/2
    with the camera direction.
    """

    def check_fov(camera, sample):
        ray, _ = camera.sample_ray(0, 0, sample, 0)
        assert dr.allclose(dr.acos(dr.dot(ray.d, direction)) * 180 / dr.pi, fov / 2)

    # In the configuration, aspect==1.5, so 'larger' should give the 'x'-axis
    for fov_axis in ['x', 'larger']:
        camera = create_camera(origin, direction, fov=fov, fov_axis=fov_axis)
        for sample in [[0.0, 0.5], [1.0, 0.5]]:
            check_fov(camera, sample)

    # In the configuration, aspect==1.5, so 'smaller' should give the 'y'-axis
    for fov_axis in ['y', 'smaller']:
        camera = create_camera(origin, direction, fov=fov, fov_axis=fov_axis)
        for sample in [[0.5, 0.0], [0.5, 1.0]]:
                check_fov(camera, sample)

    # Check the 4 corners for the `diagonal` case
    camera = create_camera(origin, direction, fov=fov, fov_axis='diagonal')
    for sample in [[0.0, 0.0], [0.0, 1.0], [1.0, 0.0], [1.0, 1.0]]:
            check_fov(camera, sample)


def test05_spectrum_sampling(variants_vec_spectral):
    # Check RGB wavelength sampling
    camera = mi.load_dict({
        'type': 'perspective',
    })
    wavelengths, _ = camera.sample_wavelengths(dr.zeros(mi.SurfaceInteraction3f), mi.Float([0.1, 0.4, 0.9]))
    assert (dr.all((wavelengths >= mi.MI_CIE_MIN) & (wavelengths <= mi.MI_CIE_MAX), axis=None))

    # Check custom SRF wavelength sampling
    camera = mi.load_dict({
        'type': 'perspective',
        'srf': {
            'type': 'spectrum',
            'value': [(1200,1.0), (1400,1.0)]
        }
    })
    wavelengths, _ =  camera.sample_wavelengths(dr.zeros(mi.SurfaceInteraction3f), mi.Float([0.1, 0.4, 0.9]))
    assert (dr.all((wavelengths >= 1200) & (wavelengths <= 1400), axis=None))

    # Check error if double SRF is defined
    with pytest.raises(RuntimeError, match=r'Sensor\(\)'):
        camera = mi.load_dict({
            'type': 'perspective',
            'srf': {
                'type': 'spectrum',
                'value': [(1200,1.0), (1400,1.0)]
            },
            'film': {
                'type': 'specfilm',
                'srf_test': {
                    'type': 'spectrum',
                    'value': [(34,1.0),(79,1.0),(120,1.0)]
                }
            }
        })


def test06_ray_cone(variants_all_backends_once):
    import math

    camera = mi.load_dict({
        'type': 'perspective', 'fov': 90, 'fov_axis': 'x', 'near_clip': 0.5,
        'film': {'type': 'hdrfilm', 'width': 100, 'height': 50}
    })

    # On the optical axis, the cone spreads by the angle one pixel subtends.
    # The ray starts on the near plane, where the cone already has the width
    # that it accumulated since the pinhole.
    ray, _ = camera.sample_ray(0, 0.5, [0.5, 0.5], [0.5, 0.5])
    spread = 2 * math.tan(math.radians(45)) / 100
    assert dr.allclose(ray.cone.spread, spread, rtol=1e-5)
    assert dr.allclose(ray.cone.width, 0.5 * spread, rtol=1e-5)
    assert dr.allclose(ray.cone.propagate(4 - 0.5).width, 4 * spread, rtol=1e-5)

    # At the edge of a 90 degree frame the pixel subtends cos(45)^1.5 less,
    # and the near plane lies 1/cos(45) farther along the ray
    ray, _ = camera.sample_ray(0, 0.5, [0.0, 0.5], [0.5, 0.5])
    cos_theta = math.cos(math.radians(45))
    assert dr.allclose(ray.cone.spread, spread * cos_theta ** 1.5, rtol=1e-4)
    assert dr.allclose(ray.cone.width, ray.cone.spread * 0.5 / cos_theta, rtol=1e-4)


def test07_jitter(variants_all_backends_once):
    import numpy as np

    # Without jitter, every ray passes through the pixel center and the
    # sample count no longer matters. The scene has a depth edge in the
    # middle of every pixel column so that jittered renders differ.
    def render(jitter, spp):
        scene = mi.load_dict({
            'type': 'scene',
            'integrator': {'type': 'depth'},
            'sensor': {
                'type': 'perspective', 'fov': 90,
                'jitter': jitter,
                'film': {'type': 'hdrfilm', 'width': 8, 'height': 8,
                         'rfilter': {'type': 'box'}},
            },
            'near': {'type': 'rectangle',
                     'to_world': mi.ScalarAffineTransform4f().translate([0, 0, 2]).scale([0.25, 10, 1])},
            'far': {'type': 'rectangle',
                    'to_world': mi.ScalarAffineTransform4f().translate([0, 0, 5]).scale([10, 10, 1])},
        })
        return np.array(mi.render(scene, spp=spp, seed=0))

    assert np.allclose(render(False, 1), render(False, 16))
    assert not np.allclose(render(True, 1), render(True, 16))


def test08_animated_transform(variant_scalar_rgb):
    """Camera rays and world_transform() follow an animated to_world"""
    t = mi.AnimatedTransform4f({
        0.0: mi.Transform4f().translate([0, 0, 0]),
        1.0: mi.Transform4f().translate([0, 0, 1])
    })
    sensor = mi.load_dict({
        'type': 'perspective',
        'to_world': t,
    })

    ray, _ = sensor.sample_ray(0.5, 0, [0.5, 0.5], 0)
    assert dr.allclose(ray.o, mi.Point3f(0, 0, 0.5), atol=0.01)

    # world_transform() defaults to time = 0
    assert dr.allclose(sensor.world_transform().translation(), [0, 0, 0])
    # world_transform(time) evaluates at time
    assert dr.allclose(sensor.world_transform(0.5).translation(), [0, 0, 0.5])
    assert dr.allclose(sensor.world_transform_scalar(0.5).translation(), [0, 0, 0.5])
    # world_transform_anim() returns the AnimatedTransform object
    awt = sensor.world_transform_anim()
    assert isinstance(awt, mi.AnimatedTransform4f)


def test09_camera_shear_rejection(variant_scalar_rgb):
    """Sensors must reject scale factors and shear in to_world."""
    from mitsuba import ScalarTransform4f as T

    # Non-uniform scale in static transform
    with pytest.raises(RuntimeError, match="Scale factors in the camera-to-world transformation are not allowed!"):
        mi.load_dict({
            'type': 'perspective',
            'to_world': T().scale([1.0, 2.0, 1.0])
        })

    # Scale in animated transform
    with pytest.raises(RuntimeError, match="Scale factors in the camera-to-world transformation are not allowed!"):
        mi.load_dict({
            'type': 'perspective',
            'to_world': mi.AnimatedTransform4f({
                0.0: T().scale([1, 1, 1]),
                1.0: T().scale([1, 2, 1])
            })
        })

    # Sheared static transform is rejected by perspective sensor
    with pytest.raises(RuntimeError, match="Scale factors in the camera-to-world transformation are not allowed!"):
        sheared = T([[1, 1, 0, 0], [0, 1, 0, 0], [0, 0, 1, 0], [0, 0, 0, 1]])
        mi.load_dict({
            'type': 'perspective',
            'to_world': sheared
        })


def test10_sensor_to_world_transform_parameter_update(variant_scalar_rgb):
    """Updating sensor.to_world should take effect and reject illegal scale."""
    from mitsuba import ScalarTransform4f as T

    sensor = mi.load_dict({'type': 'perspective'})
    params = mi.traverse(sensor)
    assert 'to_world' in params

    # Update to valid translation
    params['to_world'] = T().translate([0, 0, 5])
    params.update()
    ray, _ = sensor.sample_ray(0.0, 0, [0.5, 0.5], 0)
    assert dr.allclose(ray.o, mi.Point3f(0, 0, 5), atol=0.01)

    # Update to invalid scaled transform via dotted key must be rejected
    params['to_world'] = T().scale([2, 1, 1])
    with pytest.raises(RuntimeError, match="Scale factors in the camera-to-world transformation are not allowed!"):
        params.update()


def test11_animation_xml(variant_scalar_rgb):
    """An <animation> in XML animates the sensor"""
    xml = """
    <scene version="3.0.0">
        <sensor type="perspective">
            <animation name="to_world">
                <transform time="0">
                    <translate x="0" y="0" z="0"/>
                </transform>
                <transform time="1">
                    <translate x="0" y="0" z="1"/>
                </transform>
            </animation>
        </sensor>
    </scene>
    """
    scene = mi.load_string(xml)
    sensor = scene.sensors()[0]
    ray, _ = sensor.sample_ray(0.5, 0, [0.5, 0.5], 0)
    assert dr.allclose(ray.o, mi.Point3f(0, 0, 0.5), atol=0.01)


def test12_pixel_aspect_ratio(variants_vec_rgb):
    """Non-square pixel_aspect_ratio scales vertical FOV."""
    sensor = mi.load_dict({
        'type': 'perspective',
        'fov': 90.0,
        'fov_axis': 'x',
        'pixel_aspect_ratio': 2.0,
        'near_clip': 0.5,
        'far_clip': 100.0,
        'film': {'type': 'hdrfilm', 'width': 100, 'height': 100},
    })
    params = mi.traverse(sensor)
    assert 'pixel_aspect_ratio' in params
    assert dr.allclose(params['pixel_aspect_ratio'], 2.0)
    assert dr.allclose(sensor.pixel_aspect_ratio(), 2.0)

    # At x=0 or x=1, y=0.5, horizontal tangent is +/- tan(45 deg) = +/- 1.0
    ray_x, _ = sensor.sample_ray(0.0, 0.5, [0.0, 0.5], [0.5, 0.5])
    assert dr.allclose(ray_x.d.x / ray_x.d.z, 1.0, atol=1e-5)

    # At x=0.5, y=0.0, vertical tangent is +tan(45 deg) / 2.0 = 0.5
    ray_y, _ = sensor.sample_ray(0.0, 0.5, [0.5, 0.0], [0.5, 0.5])
    assert dr.allclose(ray_y.d.y / ray_y.d.z, 0.5, atol=1e-5)

    # Roundtrip sample_ray -> sample_direction and projection_transform
    pos = mi.Point2f([0.2, 0.5, 0.8], [0.1, 0.5, 0.9])
    ray, _ = sensor.sample_ray(0.0, 0.5, pos, [0.5, 0.5])
    it = dr.zeros(mi.Interaction3f)
    it.p = ray(3.0)
    ds, _ = sensor.sample_direction(it, [0.5, 0.5])
    assert dr.allclose(ds.uv, pos * mi.ScalarVector2f(100, 100), atol=1e-4)

    proj = sensor.projection_transform() @ it.p
    assert dr.allclose(mi.Point2f(proj.x, proj.y), pos, atol=1e-4)

    # On-axis ray cone uses geometric mean of horizontal (0.02) and vertical (0.01) pixel sizes
    ray_axis, _ = sensor.sample_ray(0.0, 0.5, [0.5, 0.5], [0.5, 0.5])
    assert dr.allclose(ray_axis.cone.spread, (0.02 * 0.01) ** 0.5, rtol=1e-5)

    # Importance weight scales with 1 / image_rect_area (doubles when fy / fx = 2.0 vs 1.0)
    sensor_sq = mi.load_dict({
        'type': 'perspective', 'fov': 90.0, 'fov_axis': 'x', 'pixel_aspect_ratio': 1.0,
        'film': {'type': 'hdrfilm', 'width': 100, 'height': 100},
    })
    it_axis = dr.zeros(mi.Interaction3f)
    it_axis.p = mi.Point3f(0.0, 0.0, 2.0)
    _, weight_nsq = sensor.sample_direction(it_axis, [0.5, 0.5])
    _, weight_sq = sensor_sq.sample_direction(it_axis, [0.5, 0.5])
    assert dr.allclose(weight_nsq, weight_sq * 2.0, rtol=1e-5)

    # Updating pixel_aspect_ratio via mi.traverse recomputes camera transforms
    params['pixel_aspect_ratio'] = 1.0
    params.update()
    ray_updated, _ = sensor.sample_ray(0.0, 0.5, [0.5, 0.0], [0.5, 0.5])
    assert dr.allclose(ray_updated.d.y / ray_updated.d.z, 1.0, atol=1e-5)

    # parse_fov() uses effective aspect when fov_axis != 'x' (aspect = 2.0 > 1 -> 'smaller' maps to 'y')
    sensor_y = mi.load_dict({
        'type': 'perspective', 'fov': 90.0, 'fov_axis': 'smaller', 'pixel_aspect_ratio': 2.0,
        'film': {'type': 'hdrfilm', 'width': 100, 'height': 100},
    })
    ray_top, _ = sensor_y.sample_ray(0.0, 0.5, [0.5, 0.0], [0.5, 0.5])
    ray_left, _ = sensor_y.sample_ray(0.0, 0.5, [0.0, 0.5], [0.5, 0.5])
    assert dr.allclose(ray_top.d.y / ray_top.d.z, 1.0, atol=1e-5)
    assert dr.allclose(ray_left.d.x / ray_left.d.z, 2.0, atol=1e-5)

    with pytest.raises(
        RuntimeError, match="The 'pixel_aspect_ratio' parameter must be > 0!"
    ):
        mi.load_dict({
            'type': 'perspective',
            'pixel_aspect_ratio': 0.0,
        })


