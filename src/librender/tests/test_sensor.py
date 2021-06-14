import numpy as np
import enoki as ek
import mitsuba

def test01_parse_fov(variant_scalar_rgb):
    from mitsuba.core import Properties
    from mitsuba.render import parse_fov
    
    # Focal length re-calculation tests:
    props = Properties()
    props["focal_length"] = "50mm"
    assert ek.allclose(parse_fov(props, aspect=0.5), 21.90213966369629)
    assert ek.allclose(parse_fov(props, aspect=1.0), 34.02204132080078)
    assert ek.allclose(parse_fov(props, aspect=1.5), 39.597713470458984)
    props["focal_length"] = "25mm"
    assert ek.allclose(parse_fov(props, aspect=1.0), 62.923526763916016)
    props["fov_axis"] = "y" # should have no effect
    assert ek.allclose(parse_fov(props, aspect=1.0), 62.923526763916016)

    # Direct FOV input tests with aspect == 1.0:
    props = Properties()
    props["fov"] = 35.0
    assert ek.allclose(parse_fov(props, aspect=1.0), 35.0)
    for axis in ["x", "y", "smaller", "larger"]:
        # Should have no effect:
        props["fov_axis"] = axis
        assert ek.allclose(parse_fov(props, aspect=1.0), 35.0)
    props["fov_axis"] = "diagonal"
    assert ek.allclose(parse_fov(props, aspect=1.0), 25.137083053588867)

    # Direct FOV input tests with aspect != 1.0:
    props = Properties()
    props["fov"] = 35.0
    for axis, result in [
        ("x", 35.0), ("smaller", 35.0),
        ("y", 17.917831420898438), ("larger", 17.917831420898438),
        ("diagonal", 16.052263259887695)
    ]:
        props["fov_axis"] = axis
        assert ek.allclose(parse_fov(props, aspect=0.5), result)
    for axis, result in [
        ("x", 35.0), ("larger", 35.0),
        ("y", 50.6234245300293), ("smaller", 50.6234245300293),
        ("diagonal", 29.399948120117188)
    ]:
        props["fov_axis"] = axis
        assert ek.allclose(parse_fov(props, aspect=1.5), result)

def test02_perspective_projection(variant_scalar_rgb):
    from mitsuba.core import Matrix4f
    from mitsuba.render import perspective_projection

    assert ek.allclose(perspective_projection(
        film_size=[128,32],
        crop_size=[16,8],
        crop_offset=[8,4],
        fov_x = 35.0,
        near_clip = 10.0,
        far_clip = 1000.0).matrix,
        Matrix4f([[-12.686378479003906, 0.0, 3.5, 0.0],
                  [0.0, -25.372756958007812, 1.5, 0.0],
                  [0.0, 0.0, 1.010101079940796, -10.1010103225708],
                  [0, 0, 1, 0]]))
