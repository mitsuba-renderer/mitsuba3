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
    at = mi.AnimatedTransform4f()
    at.add_keyframe(0.0, mi.ScalarAffineTransform4f.translate([0, 0, 0]))
    at.add_keyframe(1.0, mi.ScalarAffineTransform4f.translate([1, 2, 3]))
    assert at.is_animated()

    # Test evaluation at keyframes
    assert dr.allclose(at.eval_scalar(0.0).matrix, dr.identity(mi.Matrix4f))
    assert dr.allclose(at.eval_scalar(1.0).matrix, mi.ScalarAffineTransform4f.translate([1, 2, 3]).matrix)

    # Test interpolation
    mid = at.eval_scalar(0.5)
    assert dr.allclose(mid.matrix, mi.ScalarAffineTransform4f.translate([0.5, 1, 1.5]).matrix)

def test03_rotation_interpolation(variant_scalar_rgb):
    at = mi.AnimatedTransform4f()
    at.add_keyframe(0.0, mi.ScalarAffineTransform4f.rotate([0, 0, 1], 0))
    at.add_keyframe(1.0, mi.ScalarAffineTransform4f.rotate([0, 0, 1], 90))

    # Halfway should be 45 degrees
    mid = at.eval_scalar(0.5)
    expected = mi.ScalarAffineTransform4f.rotate([0, 0, 1], 45)
    assert dr.allclose(mid.matrix, expected.matrix)

def test04_scaling_interpolation(variant_scalar_rgb):
    at = mi.AnimatedTransform4f()
    at.add_keyframe(0.0, mi.ScalarAffineTransform4f.scale([1, 1, 1]))
    at.add_keyframe(1.0, mi.ScalarAffineTransform4f.scale([2, 4, 8]))

    mid = at.eval_scalar(0.5)
    expected = mi.ScalarAffineTransform4f.scale([1.5, 2.5, 4.5])
    assert dr.allclose(mid.matrix, expected.matrix)

def test05_complex_interpolation(variant_scalar_rgb):
    at = mi.AnimatedTransform4f()
    t0 = mi.ScalarAffineTransform4f.translate([1, 2, 3]).rotate([0, 1, 0], 30).scale([1, 2, 1])
    t1 = mi.ScalarAffineTransform4f.translate([4, 5, 6]).rotate([0, 1, 0], 60).scale([2, 1, 2])
    at.add_keyframe(0.0, t0)
    at.add_keyframe(1.0, t1)

    # Keyframe evaluation
    assert dr.allclose(at.eval_scalar(0.0).matrix, t0.matrix, atol=1e-6)
    assert dr.allclose(at.eval_scalar(1.0).matrix, t1.matrix, atol=1e-6)

    # Midpoint manual composition
    expected_mid = mi.ScalarAffineTransform4f.translate([2.5, 3.5, 4.5]) @ \
                   mi.ScalarAffineTransform4f.rotate([0, 1, 0], 45) @ \
                   mi.ScalarAffineTransform4f.scale([1.5, 1.5, 1.5])

    assert dr.allclose(at.eval_scalar(0.5).matrix, expected_mid.matrix, atol=1e-6)

def test06_vectorized_eval(variants_vec_backends_once):
    at = mi.AnimatedTransform4f()
    at.add_keyframe(0.0, mi.ScalarAffineTransform4f.translate([0, 0, 0]))
    at.add_keyframe(1.0, mi.ScalarAffineTransform4f.translate([1, 1, 1]))

    times_list = [0.0, 0.25, 0.5, 0.75, 1.0]
    expected_translations = np.array([
        [0.0, 0.25, 0.5, 0.75, 1.0],
        [0.0, 0.25, 0.5, 0.75, 1.0],
        [0.0, 0.25, 0.5, 0.75, 1.0]
    ])


    times = mi.Float(times_list)
    trafos = at.eval(times)
    assert dr.allclose(trafos.translation(), expected_translations, atol=1e-6)

def test07_scalar_eval(variant_scalar_rgb):
    at = mi.AnimatedTransform4f()
    at.add_keyframe(0.0, mi.ScalarAffineTransform4f.translate([0, 0, 0]))
    at.add_keyframe(1.0, mi.ScalarAffineTransform4f.translate([1, 1, 1]))
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
    at = mi.AnimatedTransform4f()
    m = mi.Matrix4f(1)
    m[0, 1] = 1.0
    trafo = mi.ScalarAffineTransform4f(m)

    with pytest.raises(RuntimeError, match="Transformation contains shear"):
        at.add_keyframe(0.0, trafo)

def test09_no_keyframes_error(variant_scalar_rgb):
    at = mi.AnimatedTransform4f()
    with pytest.raises(RuntimeError):
        at.eval_scalar(0.5)

    with pytest.raises(RuntimeError):
        at.eval(mi.Float(0.5))

@pytest.mark.skip
def test10_dict_loading(variant_scalar_rgb):
    at = mi.AnimatedTransform4f()
    at.add_keyframe(0.0, mi.ScalarAffineTransform4f.translate([0, 0, 0]))
    at.add_keyframe(1.0, mi.ScalarAffineTransform4f.translate([1, 2, 3]))

    props = mi.Properties()
    props["to_world"] = at

    # Check that it's retrieved correctly and has same content
    retrieved = props["to_world"]
    assert retrieved is not None
    assert props.type("to_world") == mi.Properties.Type.AnimatedTransform

# TODO: Enable this once supported
@pytest.mark.skip
def test11_xml_loading(variant_scalar_rgb):
    # To test XML loading without triggering "failed to instantiate" errors in existing plugins,
    # we can use the parser to create a ParserState and inspect its properties.
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

    # We use parse_string directly to get the state and check the root node's properties
    config = mi.parser.ParserConfig()
    state = mi.parser.parse_string(config, xml)

    assert state is not None
    props = state.root.props
    assert "test_anim" in props
    assert props.type("test_anim") == mi.Properties.Type.AnimatedTransform

    anim = props["test_anim"]
    assert anim.is_animated()
    assert len(anim.keyframes()) == 2
