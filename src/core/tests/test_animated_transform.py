import pytest
import drjit as dr
import mitsuba as mi
import numpy as np


def make_translation_anim(times, offsets):
    return mi.AnimatedTransform4f({
        t: mi.ScalarAffineTransform4f.translate(o)
        for t, o in zip(times, offsets)
    })


def test01_basics(variant_scalar_rgb):
    """Construction from keyframes"""
    trafo = mi.ScalarAffineTransform4f.translate([1, 2, 3])
    at = make_translation_anim([0.0, 1.0], [[0, 0, 0], [1, 2, 3]])
    assert dr.allclose(at.eval_scalar(0.0).matrix, dr.identity(mi.Matrix4f))
    assert dr.allclose(at.eval_scalar(1.0).matrix, trafo.matrix)
    assert dr.allclose(at.eval_scalar(0.5).translation(), [0.5, 1, 1.5])


@pytest.mark.parametrize("t0, t1, mid", [
    # Rotations are interpolated at constant angular speed
    (lambda T: T().rotate([0, 0, 1], 0),
     lambda T: T().rotate([0, 0, 1], 90),
     lambda T: T().rotate([0, 0, 1], 45)),
    # Scale factors are interpolated linearly
    (lambda T: T().scale([1, 1, 1]),
     lambda T: T().scale([2, 4, 8]),
     lambda T: T().scale([1.5, 2.5, 4.5])),
    # Translation, rotation and scale are interpolated component-wise
    (lambda T: T().translate([1, 2, 3]).rotate([0, 1, 0], 30).scale([1, 2, 1]),
     lambda T: T().translate([4, 5, 6]).rotate([0, 1, 0], 60).scale([2, 1, 2]),
     lambda T: T().translate([2.5, 3.5, 4.5]).rotate([0, 1, 0], 45).scale([1.5, 1.5, 1.5])),
])
def test02_interpolation(variant_scalar_rgb, t0, t1, mid):
    """Keyframes are reproduced exactly and interpolated component-wise"""
    T = mi.ScalarAffineTransform4f
    t0, t1, mid = t0(T), t1(T), mid(T)
    at = mi.AnimatedTransform4f({0.0: t0, 1.0: t1})

    assert dr.allclose(at.eval_scalar(0.0).matrix, t0.matrix)
    assert dr.allclose(at.eval_scalar(1.0).matrix, t1.matrix)
    assert dr.allclose(at.eval_scalar(0.5).matrix, mid.matrix)
    assert dr.allclose(at.eval_scalar(0.5).inverse().matrix, mid.inverse().matrix)


def sheared():
    m = mi.ScalarMatrix4f(1)
    m[0, 1] = 1.0
    return mi.ScalarAffineTransform4f(m)


@pytest.mark.parametrize("keyframes, match", [
    # Keyframes are stored in decomposed form, which cannot represent shear
    (lambda: [(0.0, sheared()), (1.0, mi.ScalarAffineTransform4f())],
     "must not contain shear"),
    (lambda: [], "at least two keyframes"),
    (lambda: [(0.0, mi.ScalarAffineTransform4f())], "at least two keyframes"),
    # Coincident keyframes would divide by zero
    (lambda: [(1.0, mi.ScalarAffineTransform4f.translate([0, 0, 0])),
              (1.0, mi.ScalarAffineTransform4f.translate([1, 0, 0]))],
     "same time"),
])
def test03_invalid_keyframes(variant_scalar_rgb, keyframes, match):
    """Invalid keyframe lists are rejected"""
    with pytest.raises(RuntimeError, match=match):
        mi.AnimatedTransform4f(keyframes())


def test04_xml_roundtrip(variant_scalar_rgb):
    """The <animation> tag loads and survives a round trip through the XML writer"""
    xml = """<scene version="3.0.0">
        <sensor type="perspective">
            <animation name="to_world">
                <transform time="0">
                    <translate x="0" y="0" z="0"/>
                </transform>
                <transform time="1.5">
                    <translate x="1" y="2" z="3"/>
                </transform>
            </animation>
        </sensor>
    </scene>"""

    config = mi.parser.ParserConfig(mi.variant())
    written = mi.parser.write_string(mi.parser.parse_string(config, xml))
    assert "<animation" in written

    for source in [xml, written]:
        state = mi.parser.parse_string(config, source)
        scene = mi.parser.instantiate(config, state)
        at = scene.sensors()[0].world_transform_anim()
        assert dr.allclose(at.eval_scalar(0.0).translation(), [0, 0, 0])
        assert dr.allclose(at.eval_scalar(0.75).translation(), [0.5, 1, 1.5])
        assert dr.allclose(at.eval_scalar(1.5).translation(), [1, 2, 3])

    # An <animation> may also sit directly under <scene>, in which case it is
    # instantiated as a plain child object.
    mi.load_string("""<scene version="3.0.0">
        <animation name="test_anim">
            <transform time="0"><translate x="0" y="0" z="0"/></transform>
            <transform time="1"><translate x="1" y="2" z="3"/></transform>
        </animation>
    </scene>""")

    with pytest.raises(Exception, match="at least two <transform>"):
        mi.load_string("""<scene version="3.0.0">
            <animation name="test_anim">
                <transform time="0"><translate x="1" y="2" z="3"/></transform>
            </animation>
        </scene>""")


def test05_endpoint_bbox(variant_scalar_rgb):
    """The bounding box of an animated endpoint covers all keyframe positions"""
    emitter = mi.load_dict({
        'type': 'point',
        'to_world': make_translation_anim([0.0, 1.0, 2.0],
                                          [[1, -2, 3], [-1, 5, 0], [0, 2, 8]])
    })

    bbox = emitter.bbox()
    assert dr.allclose(bbox.min, [-1, -2, 0])
    assert dr.allclose(bbox.max, [1, 5, 8])


def test06_parameters_changed(variants_vec_backends_once):
    """Writing a keyframe tensor via traverse() updates both evaluation paths"""
    at = make_translation_anim([0.0, 1.0], [[0, 0, 0], [1, 0, 0]])
    sensor = mi.load_dict({'type': 'perspective', 'to_world': at})
    params = mi.traverse(sensor)
    # translation.x of the second keyframe
    translation = mi.TensorXf(params['to_world.translation'])
    translation[1, 0] = 2.5
    params['to_world.translation'] = translation
    params.update()
    assert dr.allclose(at.eval_scalar(1.0).translation(), [2.5, 0, 0])
    assert dr.allclose(at.eval(mi.Float(1.0)).translation(), [2.5, 0, 0])


def test07_change_frame_number(variants_all_backends_once):
    """Writing all four tensors can change the number of keyframes"""
    at = make_translation_anim([0.0, 1.0], [[1, 0, 0], [3, 0, 0]])
    assert dr.allclose(at.eval_scalar(0.5).translation(), [2.0, 0.0, 0.0])

    # Grow to 3 keyframes. All four views must be written together, since
    # they have to agree on the number of keyframes.
    params = mi.traverse(at)
    params['times']       = mi.TensorXf([0.0, 1.0, 2.0], shape=(3,))
    params['scale']       = mi.TensorXf([1.0] * 9, shape=(3, 3))
    params['rotation']    = mi.TensorXf([0.0, 0.0, 0.0, 1.0] * 3, shape=(3, 4))
    params['translation'] = mi.TensorXf([1.0, 0.0, 0.0,
                                         3.0, 0.0, 0.0,
                                         7.0, 0.0, 0.0], shape=(3, 3))
    params.update()
    assert dr.allclose(at.eval_scalar(0.5).translation(), [2.0, 0.0, 0.0])
    assert dr.allclose(at.eval_scalar(1.5).translation(), [5.0, 0.0, 0.0])
    assert dr.allclose(at.eval(mi.Float(1.5)).translation(), [5.0, 0.0, 0.0])


def test08_vectorized_eval_many_keyframes(variants_vec_backends_once):
    """eval() locates the right keyframe pair in a longer animation"""
    at = make_translation_anim([0.0, 1.0, 2.0, 3.0],
                               [[0, 0, 0], [1, 0, 0], [1, 4, 0], [1, 4, 9]])

    times = mi.Float([0.0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0])
    got = at.eval(times).translation()
    expected = np.array([
        [0.0, 0.5, 1.0, 1.0, 1.0, 1.0, 1.0],
        [0.0, 0.0, 0.0, 2.0, 4.0, 4.0, 4.0],
        [0.0, 0.0, 0.0, 0.0, 0.0, 4.5, 9.0],
    ])
    assert dr.allclose(got, expected)


def test09_eval_matches_eval_scalar(variants_all_backends_once):
    """eval() and eval_scalar() agree, and both clamp outside the time range"""
    at = mi.AnimatedTransform4f({
        0.0: mi.ScalarAffineTransform4f.translate([1, 2, 3]).rotate([0, 1, 0], 10),
        1.0: mi.ScalarAffineTransform4f.translate([4, 5, 6]).rotate([0, 1, 0], 50).scale([2, 1, 1]),
        2.5: mi.ScalarAffineTransform4f.translate([0, 1, 0]).rotate([1, 0, 0], 80),
    })

    for t in [-1.0, 0.0, 0.3, 1.0, 1.9, 2.5, 4.0]:
        ref = at.eval_scalar(t)
        trafo = at.eval(mi.Float(t))
        assert dr.allclose(trafo.matrix, mi.Matrix4f(ref.matrix), atol=1e-5)
        assert dr.allclose(trafo.inverse().matrix,
                           mi.Matrix4f(ref.inverse().matrix), atol=1e-5)

    assert dr.allclose(at.eval_scalar(-1.0).matrix, at.eval_scalar(0.0).matrix)
    assert dr.allclose(at.eval_scalar(4.0).matrix, at.eval_scalar(2.5).matrix)


def test10_large_rotation_hemisphere(variant_scalar_rgb):
    """A step beyond 180 degrees flips the second quaternion onto the short path"""
    at = mi.AnimatedTransform4f({
        0.0: mi.ScalarAffineTransform4f.rotate([0, 0, 1], 0),
        1.0: mi.ScalarAffineTransform4f.rotate([0, 0, 1], 270),
    })
    # 270 degrees is -90 degrees on the short path, so the midpoint is -45.
    mid = at.eval_scalar(0.5)
    expected = mi.ScalarAffineTransform4f.rotate([0, 0, 1], -45)
    assert dr.allclose(mid.matrix, expected.matrix)


def test11_unsorted_keyframes_are_sorted(variant_scalar_rgb):
    """Keyframes are sorted by time on construction"""
    at = mi.AnimatedTransform4f([
        (2.0, mi.ScalarAffineTransform4f.translate([2, 0, 0])),
        (0.0, mi.ScalarAffineTransform4f.translate([0, 0, 0])),
        (1.0, mi.ScalarAffineTransform4f.translate([1, 0, 0])),
    ])
    assert dr.allclose(at.eval_scalar(0.0).translation(), [0, 0, 0])
    assert dr.allclose(at.eval_scalar(0.5).translation(), [0.5, 0, 0])
    assert dr.allclose(at.eval_scalar(2.0).translation(), [2, 0, 0])


def test12_shrink_to_static(variants_vec_backends_once):
    """Editing an attached animation down to one keyframe raises an error"""
    sensor = mi.load_dict({
        'type': 'perspective',
        'to_world': make_translation_anim([0.0, 1.0], [[0, 0, 0], [1, 0, 0]])
    })
    assert sensor.world_transform_anim() is not None

    params = mi.traverse(sensor)
    params['to_world.times']       = mi.TensorXf([0.0], shape=(1,))
    params['to_world.scale']       = mi.TensorXf([1.0, 1.0, 1.0], shape=(1, 3))
    params['to_world.rotation']    = mi.TensorXf([0.0, 0.0, 0.0, 1.0], shape=(1, 4))
    params['to_world.translation'] = mi.TensorXf([5.0, 0.0, 0.0], shape=(1, 3))
    with pytest.raises(RuntimeError, match="at least two keyframes"):
        params.update()


def test13_edited_keyframes_are_validated(variants_all_backends_once):
    """Keyframes written via traverse() are sorted and validated"""
    at = make_translation_anim([0.0, 1.0], [[0, 0, 0], [1, 0, 0]])
    params = mi.traverse(at)
    params['times'] = mi.TensorXf([1.0, 0.0], shape=(2,))
    params.update()
    assert dr.allclose(at.eval_scalar(0.0).translation(), [1, 0, 0])
    assert dr.allclose(at.eval(mi.Float(0.0)).translation(), [1, 0, 0])

    params['times'] = mi.TensorXf([0.5, 0.5], shape=(2,))
    with pytest.raises(RuntimeError, match="same time"):
        params.update()


@pytest.mark.parametrize("trafo_fn", [
    lambda T: T().scale([-1, 1, 1]),
    lambda T: T().scale([-1, -1, -1]),
    lambda T: T().rotate([0, 1, 0], 33).scale([-1, 2, 1]),
    lambda T: T().translate([1, 2, 3]).rotate([1, 0, 0], 90).scale([2, -3, 4]),
])
def test14_mirroring_decomposition(variant_scalar_rgb, trafo_fn):
    """Mirroring transformations survive the decomposition into keyframes"""
    trafo = trafo_fn(mi.ScalarAffineTransform4f)
    at = mi.AnimatedTransform4f({0.0: trafo, 1.0: trafo})
    for t in [0.0, 0.5, 1.0]:
        assert dr.allclose(at.eval_scalar(t).matrix, trafo.matrix, atol=1e-5), t
