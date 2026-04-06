import pytest
import drjit as dr
import mitsuba as mi
import numpy as np

def test01_empty(variant_scalar_rgb):
    at = mi.AnimatedTransform4f()
    assert not at.is_animated()
    assert str(at) == "AnimatedTransform[]"

def test02_basics(variant_scalar_rgb):
    # Test construction from constant transform
    trafo = mi.ScalarAffineTransform4f.translate([1, 2, 3])
    at = mi.AnimatedTransform4f(trafo)
    assert not at.is_animated()
    assert dr.allclose(at.eval_scalar(0.5).matrix, trafo.matrix)

    # Test adding keyframes
    at = mi.AnimatedTransform4f({
        0.0: mi.ScalarAffineTransform4f.translate([0, 0, 0]),
        1.0: mi.ScalarAffineTransform4f.translate([1, 2, 3])
    })
    assert at.is_animated()

    # Test evaluation at keyframes
    assert dr.allclose(at.eval_scalar(0.0).matrix, dr.identity(mi.Matrix4f))
    assert dr.allclose(at.eval_scalar(1.0).matrix, mi.ScalarAffineTransform4f.translate([1, 2, 3]).matrix)

    # Test interpolation
    mid = at.eval_scalar(0.5)
    assert dr.allclose(mid.matrix, mi.ScalarAffineTransform4f.translate([0.5, 1, 1.5]).matrix)

def test03_rotation_interpolation(variant_scalar_rgb):
    at = mi.AnimatedTransform4f({
        0.0: mi.ScalarAffineTransform4f.rotate([0, 0, 1], 0),
        1.0: mi.ScalarAffineTransform4f.rotate([0, 0, 1], 90)
    })

    # Halfway should be 45 degrees
    mid = at.eval_scalar(0.5)
    expected = mi.ScalarAffineTransform4f.rotate([0, 0, 1], 45)
    assert dr.allclose(mid.matrix, expected.matrix)
    assert dr.allclose(mid.inverse().matrix, expected.inverse().matrix)

def test04_scaling_interpolation(variant_scalar_rgb):
    at = mi.AnimatedTransform4f({
        0.0: mi.ScalarAffineTransform4f.scale([1, 1, 1]),
        1.0: mi.ScalarAffineTransform4f.scale([2, 4, 8])
    })

    mid = at.eval_scalar(0.5)
    expected = mi.ScalarAffineTransform4f.scale([1.5, 2.5, 4.5])
    assert dr.allclose(mid.matrix, expected.matrix)
    assert dr.allclose(mid.inverse().matrix, expected.inverse().matrix)

def test05_complex_interpolation(variant_scalar_rgb):
    t0 = mi.ScalarAffineTransform4f.translate([1, 2, 3]).rotate([0, 1, 0], 30).scale([1, 2, 1])
    t1 = mi.ScalarAffineTransform4f.translate([4, 5, 6]).rotate([0, 1, 0], 60).scale([2, 1, 2])
    at = mi.AnimatedTransform4f({0.0: t0, 1.0: t1})

    # Keyframe evaluation
    assert dr.allclose(at.eval_scalar(0.0).matrix, t0.matrix)
    assert dr.allclose(at.eval_scalar(1.0).matrix, t1.matrix)

    # Midpoint manual composition
    expected_mid = mi.ScalarAffineTransform4f.translate([2.5, 3.5, 4.5]) @ \
                   mi.ScalarAffineTransform4f.rotate([0, 1, 0], 45) @ \
                   mi.ScalarAffineTransform4f.scale([1.5, 1.5, 1.5])

    assert dr.allclose(at.eval_scalar(0.5).matrix, expected_mid.matrix)
    assert dr.allclose(at.eval_scalar(0.5).inverse().matrix, expected_mid.inverse().matrix)

def test06_vectorized_eval(variants_vec_backends_once):
    at = mi.AnimatedTransform4f({
        0.0: mi.ScalarAffineTransform4f.translate([0, 0, 0]),
        1.0: mi.ScalarAffineTransform4f.translate([1, 1, 1])
    })

    times_list = [0.0, 0.25, 0.5, 0.75, 1.0]
    expected_translations = np.array([
        [0.0, 0.25, 0.5, 0.75, 1.0],
        [0.0, 0.25, 0.5, 0.75, 1.0],
        [0.0, 0.25, 0.5, 0.75, 1.0]
    ])


    times = mi.Float(times_list)
    trafos = at.eval(times)
    assert dr.allclose(trafos.translation(), expected_translations)
    assert dr.allclose(trafos.inverse().matrix, mi.AffineTransform4f.translate(expected_translations).inverse().matrix)

def test07_scalar_eval(variant_scalar_rgb):
    at = mi.AnimatedTransform4f({
        0.0: mi.ScalarAffineTransform4f.translate([0, 0, 0]),
        1.0: mi.ScalarAffineTransform4f.translate([1, 1, 1])
    })
    times_list = [0.0, 0.25, 0.5, 0.75, 1.0]
    expected_translations = np.array([
        [0.0, 0.25, 0.5, 0.75, 1.0],
        [0.0, 0.25, 0.5, 0.75, 1.0],
        [0.0, 0.25, 0.5, 0.75, 1.0]
    ])
    for i, t in enumerate(times_list):
        trafo = at.eval_scalar(t)
        assert dr.allclose(trafo.translation(), expected_translations[:, i])

def test08_shear_error(variant_scalar_rgb):
    m = mi.Matrix4f(1)
    m[0, 1] = 1.0
    trafo = mi.ScalarAffineTransform4f(m)
    with pytest.raises(RuntimeError, match="Transformation contains shear"):
        mi.AnimatedTransform4f({0.0: trafo})

def test09_no_keyframes_error(variant_scalar_rgb):
    at = mi.AnimatedTransform4f()
    with pytest.raises(RuntimeError):
        at.eval_scalar(0.5)

    with pytest.raises(RuntimeError):
        at.eval(mi.Float(0.5))

def test10_properties(variant_scalar_rgb):
    at = mi.AnimatedTransform4f({
        0.0: mi.ScalarAffineTransform4f.translate([0, 0, 0]),
        1.0: mi.ScalarAffineTransform4f.translate([1, 2, 3])
    })

    props = mi.Properties()
    props["to_world"] = at

    # Check that it's retrieved correctly and has same content
    retrieved = props["to_world"]
    assert retrieved is not None
    assert props.type("to_world") == mi.Properties.Type.Object

def test11_xml_loading(variant_scalar_rgb):
    xml = """<scene version="3.0.0">
        <animation name="test_anim">
            <transform time="0">
                <translate x="0" y="0" z="0"/>
            </transform>
            <transform time="1">
                <translate x="1" y="2" z="3"/>
            </transform>
        </animation>
    </scene>"""

    obj = mi.load_string(xml)
    assert obj is not None
    assert str(obj) == """Scene[
  children = [
    AnimatedTransform[
      0: Keyframe[S=[1, 1, 1], Q=1, T=[0, 0, 0]],
      1: Keyframe[S=[1, 1, 1], Q=1, T=[1, 2, 3]],
    ]
  ]
]"""


def test12_translation_bounds(variant_scalar_rgb):
    at = mi.AnimatedTransform4f({
        0.0: mi.ScalarAffineTransform4f.translate([1, -2, 3]),
        1.0: mi.ScalarAffineTransform4f.translate([-1, 5, 0]),
        2.0: mi.ScalarAffineTransform4f.translate([0, 2, 8])
    })

    bbox = at.get_translation_bounds()
    assert dr.allclose(bbox.min, [-1, -2, 0])
    assert dr.allclose(bbox.max, [1, 5, 8])


def test13_has_scale(variant_scalar_rgb):
    at = mi.AnimatedTransform4f({
        0.0: mi.ScalarAffineTransform4f.translate([1, 2, 3]),
        1.0: mi.ScalarAffineTransform4f.translate([4, 5, 6])
    })
    assert not at.has_scale()

    at2 = mi.AnimatedTransform4f({
        0.0: mi.ScalarAffineTransform4f.translate([1, 2, 3]),
        1.0: mi.ScalarAffineTransform4f.scale([2, 1, 1])
    })
    assert at2.has_scale()

def test14_time_bounds(variant_scalar_rgb):
    at = mi.AnimatedTransform4f({
        0.5: mi.ScalarAffineTransform4f.translate([1, 2, 3]),
        1.5: mi.ScalarAffineTransform4f.translate([4, 5, 6])
    })

    bbox = at.get_time_bounds()
    assert bbox.min == 0.5
    assert bbox.max == 1.5

def test15_spatial_bounds(variant_scalar_rgb):
    at = mi.AnimatedTransform4f({
        0.0: mi.ScalarAffineTransform4f.translate([0, 0, 0]),
        1.0: mi.ScalarAffineTransform4f.translate([10, 0, 0])
    })

    bbox = mi.ScalarBoundingBox3f([0, 0, 0], [1, 1, 1])
    spatial_bounds = at.get_spatial_bounds(bbox)

    assert dr.allclose(spatial_bounds.min, [0, 0, 0])
    assert dr.allclose(spatial_bounds.max, [11, 1, 1])


def test16_parameters_changed(variants_vec_backends_once):
    at = mi.AnimatedTransform4f({
        0.0: mi.ScalarAffineTransform4f.translate([0, 0, 0]),
        1.0: mi.ScalarAffineTransform4f.translate([1, 0, 0])
    })
    sensor = mi.load_dict({'type': 'perspective', 'to_world': at})
    params = mi.traverse(sensor)
    translations = params['to_world.translations']
    translations[3] = 2.5
    at.parameters_changed(["translations"])
    assert dr.allclose(at.eval_scalar(1.0).translation(), [2.5, 0, 0])


def test17_change_frame_number(variants_vec_backends_once):
    # Initialize with 1 keyframe
    at = mi.AnimatedTransform4f(mi.ScalarAffineTransform4f.translate([1.0, 0.0, 0.0]))
    assert dr.allclose(at.eval_scalar(0.0).translation(), [1.0, 0.0, 0.0])

    # Modify to 2 keyframes
    params = mi.traverse(at)
    params['times'] = mi.Float([0.0, 1.0])
    params['translations'] = mi.Float([1.0, 0.0, 0.0, 3.0, 0.0, 0.0])
    params['scales'] = mi.Float([1.0, 1.0, 1.0, 1.0, 1.0, 1.0])
    params['rotations'] = mi.Float([1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0])
    at.parameters_changed(["times", "translations", "scales", "rotations"])
    assert dr.allclose(at.eval_scalar(0.0).translation(), [1.0, 0.0, 0.0])
    assert dr.allclose(at.eval_scalar(0.5).translation(), [2.0, 0.0, 0.0])
    assert dr.allclose(at.eval_scalar(1.0).translation(), [3.0, 0.0, 0.0])

    # Shrink back to 1 keyframe
    params['times'] = mi.Float([0.5])
    params['translations'] = mi.Float([5.0, 0.0, 0.0])
    params['scales'] = mi.Float([1.0, 1.0, 1.0])
    params['rotations'] = mi.Float([1.0, 0.0, 0.0, 0.0])
    at.parameters_changed(["times", "translations", "scales", "rotations"])
    assert dr.allclose(at.eval_scalar(0.0).translation(), [5.0, 0.0, 0.0])
    assert dr.allclose(at.eval_scalar(1.0).translation(), [5.0, 0.0, 0.0])

def test18_ensure_uniform_keyframes(variant_scalar_rgb):
    # Uniform keyframes
    at = mi.AnimatedTransform4f({
        0.0: mi.ScalarAffineTransform4f.translate([0, 0, 0]),
        0.5: mi.ScalarAffineTransform4f.translate([1, 0, 0]),
        1.0: mi.ScalarAffineTransform4f.translate([2, 0, 0])
    })
    at.ensure_uniform_keyframes()  # Should not raise

    # 1 or 2 keyframes should always pass
    at2 = mi.AnimatedTransform4f(mi.ScalarAffineTransform4f.translate([0, 0, 0]))
    at2.ensure_uniform_keyframes()

    at3 = mi.AnimatedTransform4f({
        0.0: mi.ScalarAffineTransform4f.translate([0, 0, 0]),
        1.0: mi.ScalarAffineTransform4f.translate([1, 0, 0])
    })
    at3.ensure_uniform_keyframes()

    # Non-uniform keyframes
    at4 = mi.AnimatedTransform4f({
        0.0: mi.ScalarAffineTransform4f.translate([0, 0, 0]),
        0.3: mi.ScalarAffineTransform4f.translate([1, 0, 0]),
        1.0: mi.ScalarAffineTransform4f.translate([2, 0, 0])
    })
    with pytest.raises(RuntimeError):
        at4.ensure_uniform_keyframes()
