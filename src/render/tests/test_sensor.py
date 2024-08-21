import pytest
import drjit as dr
import mitsuba as mi

def test01_parse_fov(variant_scalar_rgb):
    # Focal length re-calculation tests:
    props = mi.Properties()
    props["focal_length"] = "50mm"
    assert dr.allclose(mi.parse_fov(props, aspect=0.5), 21.90213966369629)
    assert dr.allclose(mi.parse_fov(props, aspect=1.0), 34.02204132080078)
    assert dr.allclose(mi.parse_fov(props, aspect=1.5), 39.597713470458984)
    props["focal_length"] = "25mm"
    assert dr.allclose(mi.parse_fov(props, aspect=1.0), 62.923526763916016)
    props["fov_axis"] = "y" # should have no effect
    assert dr.allclose(mi.parse_fov(props, aspect=1.0), 62.923526763916016)

    # Direct FOV input tests with aspect == 1.0:
    props = mi.Properties()
    props["fov"] = 35.0
    assert dr.allclose(mi.parse_fov(props, aspect=1.0), 35.0)
    for axis in ["x", "y", "smaller", "larger"]:
        # Should have no effect:
        props["fov_axis"] = axis
        assert dr.allclose(mi.parse_fov(props, aspect=1.0), 35.0)
    props["fov_axis"] = "diagonal"
    assert dr.allclose(mi.parse_fov(props, aspect=1.0), 25.137083053588867)

    # Direct FOV input tests with aspect != 1.0:
    props = mi.Properties()
    props["fov"] = 35.0
    for axis, result in [
        ("x", 35.0), ("smaller", 35.0),
        ("y", 17.917831420898438), ("larger", 17.917831420898438),
        ("diagonal", 16.052263259887695)
    ]:
        props["fov_axis"] = axis
        assert dr.allclose(mi.parse_fov(props, aspect=0.5), result)
    for axis, result in [
        ("x", 35.0), ("larger", 35.0),
        ("y", 50.6234245300293), ("smaller", 50.6234245300293),
        ("diagonal", 29.399948120117188)
    ]:
        props["fov_axis"] = axis
        assert dr.allclose(mi.parse_fov(props, aspect=1.5), result)

def test02_perspective_projection(variant_scalar_rgb):
    assert dr.allclose(mi.perspective_projection(
        film_size=[128,32],
        crop_size=[16,8],
        crop_offset=[8,4],
        fov_x = 35.0,
        near_clip = 10.0,
        far_clip = 1000.0).matrix,
        mi.Matrix4f([[-12.686378479003906, 0.0, 3.5, 0.0],
                     [0.0, -25.372756958007812, 1.5, 0.0],
                     [0.0, 0.0, 1.010101079940796, -10.1010103225708],
                     [0, 0, 1, 0]]))


def test03_trampoline(variants_vec_backends_once_rgb):
    class DummySensor(mi.Sensor):
        def __init__(self, props):
            mi.Sensor.__init__(self, props)
            self.m_needs_sample_2 = True

        def to_string(self):
            return f"DummySensor ({self.m_needs_sample_2})"

    mi.register_sensor('dummy_sensor', DummySensor)
    sensor = mi.load_dict({
        'type': 'dummy_sensor'
    })

    assert str(sensor) == "DummySensor (True)"
