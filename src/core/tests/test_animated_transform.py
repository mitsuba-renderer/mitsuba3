import pytest
import drjit as dr
import mitsuba as mi
import numpy as np
def test01_empty(variant_scalar_rgb):
    at = mi.AnimatedTransform4f()
    assert not at.is_animated()



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

    # For single frames (no animation), shear is allowed
    at = mi.AnimatedTransform4f(trafo)
    assert dr.allclose(at.eval_scalar(0.0).matrix, m)
    at = mi.AnimatedTransform4f({0.0: trafo})
    assert dr.allclose(at.eval_scalar(0.0).matrix, m)

    # Interpolating it is not possible, and is rejected.
    with pytest.raises(RuntimeError, match="contains shear"):
        mi.AnimatedTransform4f({0.0: trafo,
                                1.0: mi.ScalarAffineTransform4f.translate([1, 0, 0])})



def test09_no_keyframes_error(variant_scalar_rgb):
    at = mi.AnimatedTransform4f()
    with pytest.raises(RuntimeError, match="at least one keyframe"):
        at.eval_scalar(0.5)

    with pytest.raises(RuntimeError, match="at least one keyframe"):
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



def test17_change_frame_number(variants_vec_backends_once):
    # Initialize with 1 keyframe
    at = mi.AnimatedTransform4f(mi.ScalarAffineTransform4f.translate([1.0, 0.0, 0.0]))
    assert dr.allclose(at.eval_scalar(0.0).translation(), [1.0, 0.0, 0.0])

    # Modify to 2 keyframes. All four views must be written together, since
    # they have to agree on the number of keyframes.
    params = mi.traverse(at)
    keys = ["times", "scale", "rotation", "translation"]
    params['times']       = mi.TensorXf([0.0, 1.0], shape=(2,))
    params['scale']       = mi.TensorXf([1.0] * 6, shape=(2, 3))
    params['rotation']    = mi.TensorXf([0.0, 0.0, 0.0, 1.0] * 2, shape=(2, 4))
    params['translation'] = mi.TensorXf([1.0, 0.0, 0.0,
                                        3.0, 0.0, 0.0], shape=(2, 3))
    at.parameters_changed(keys)
    assert dr.allclose(at.eval_scalar(0.0).translation(), [1.0, 0.0, 0.0])
    assert dr.allclose(at.eval_scalar(0.5).translation(), [2.0, 0.0, 0.0])
    assert dr.allclose(at.eval_scalar(1.0).translation(), [3.0, 0.0, 0.0])

    # Shrink back to 1 keyframe
    params = mi.traverse(at)
    params['times']       = mi.TensorXf([0.5], shape=(1,))
    params['scale']       = mi.TensorXf([1.0, 1.0, 1.0], shape=(1, 3))
    params['rotation']    = mi.TensorXf([0.0, 0.0, 0.0, 1.0], shape=(1, 4))
    params['translation'] = mi.TensorXf([5.0, 0.0, 0.0], shape=(1, 3))
    at.parameters_changed(keys)
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
    with pytest.raises(RuntimeError, match="uniform range of keyframes"):
        at4.ensure_uniform_keyframes()



def make_translation_anim(times, offsets):
    return mi.AnimatedTransform4f({
        t: mi.ScalarAffineTransform4f.translate(o)
        for t, o in zip(times, offsets)
    })



def test19_vectorized_eval_many_keyframes(variants_vec_backends_once):
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



def test20_eval_matches_eval_scalar(variants_vec_backends_once):
    at = mi.AnimatedTransform4f({
        0.0: mi.ScalarAffineTransform4f.translate([1, 2, 3]).rotate([0, 1, 0], 10),
        1.0: mi.ScalarAffineTransform4f.translate([4, 5, 6]).rotate([0, 1, 0], 50).scale([2, 1, 1]),
        2.5: mi.ScalarAffineTransform4f.translate([0, 1, 0]).rotate([1, 0, 0], 80),
    })

    for t in [-1.0, 0.0, 0.3, 1.0, 1.9, 2.5, 4.0]:
        assert dr.allclose(at.eval(mi.Float(t)).matrix,
                           mi.Matrix4f(at.eval_scalar(t).matrix), atol=1e-5)



def test21_clamping_outside_time_range(variant_scalar_rgb):
    at = make_translation_anim([1.0, 2.0], [[0, 0, 0], [10, 0, 0]])

    # Times before/after the range clamp to the first/last keyframe.
    for t in [-5.0, 0.0, 1.0]:
        assert dr.allclose(at.eval_scalar(t).translation(), [0, 0, 0])
    for t in [2.0, 3.0, 100.0]:
        assert dr.allclose(at.eval_scalar(t).translation(), [10, 0, 0])



def test22_clamping_outside_time_range_vec(variants_vec_backends_once):
    at = make_translation_anim([1.0, 2.0, 3.0], [[0, 0, 0], [10, 0, 0], [20, 0, 0]])
    got = at.eval(mi.Float([-5.0, 1.0, 3.0, 99.0])).translation()
    assert dr.allclose(got, np.array([[0.0, 0.0, 20.0, 20.0],
                                      [0.0, 0.0, 0.0, 0.0],
                                      [0.0, 0.0, 0.0, 0.0]]))



def test23_large_rotation_hemisphere(variant_scalar_rgb):
    # A >180 degree step between keyframes: the constructor flips the second
    # quaternion so that slerp takes the short path consistently.
    at = mi.AnimatedTransform4f({
        0.0: mi.ScalarAffineTransform4f.rotate([0, 0, 1], 0),
        1.0: mi.ScalarAffineTransform4f.rotate([0, 0, 1], 270),
    })
    # 270 degrees is -90 degrees on the short path, so the midpoint is -45.
    mid = at.eval_scalar(0.5)
    expected = mi.ScalarAffineTransform4f.rotate([0, 0, 1], -45)
    assert dr.allclose(mid.matrix, expected.matrix)



def test24_unsorted_keyframes_are_sorted(variant_scalar_rgb):
    at = mi.AnimatedTransform4f([
        (2.0, mi.ScalarAffineTransform4f.translate([2, 0, 0])),
        (0.0, mi.ScalarAffineTransform4f.translate([0, 0, 0])),
        (1.0, mi.ScalarAffineTransform4f.translate([1, 0, 0])),
    ])
    bounds = at.get_time_bounds()
    assert bounds.min == 0.0 and bounds.max == 2.0
    assert dr.allclose(at.eval_scalar(0.0).translation(), [0, 0, 0])
    assert dr.allclose(at.eval_scalar(0.5).translation(), [0.5, 0, 0])
    assert dr.allclose(at.eval_scalar(2.0).translation(), [2, 0, 0])



def test25_duplicate_keyframe_times(variant_scalar_rgb):
    # Coincident keyframes would divide by zero during interpolation.
    with pytest.raises(RuntimeError, match="same time"):
        mi.AnimatedTransform4f([
            (1.0, mi.ScalarAffineTransform4f.translate([0, 0, 0])),
            (1.0, mi.ScalarAffineTransform4f.translate([1, 0, 0])),
        ])



def test28_grad_enabled(variants_all_ad_rgb):
    static = mi.AnimatedTransform4f(mi.ScalarAffineTransform4f.translate([1, 0, 0]))
    assert not static.parameters_grad_enabled()

    animated = make_translation_anim([0.0, 1.0], [[0, 0, 0], [1, 0, 0]])
    assert not animated.parameters_grad_enabled()

    # Gradients enter through a write: enabling them on a view attaches the
    # packed buffer once the view is folded back in by parameters_changed()
    params = mi.traverse(animated)
    translation = mi.TensorXf(params['translation'])
    dr.enable_grad(translation)
    params['translation'] = translation
    params.update()
    assert animated.parameters_grad_enabled()



def test29_spatial_bounds_hits_keyframes(variant_scalar_rgb):
    # A keyframe that is an extremum but does not fall on the uniform sample
    # grid must still be included in the swept bounds.
    at = make_translation_anim([0.0, 1.0 / 3.0, 1.0],
                               [[0, 0, 0], [0, 100, 0], [0, 0, 0]])
    bbox = mi.ScalarBoundingBox3f([0, 0, 0], [1, 1, 1])
    bounds = at.get_spatial_bounds(bbox)
    assert bounds.max[1] >= 100.0



def test31_negative_times(variant_scalar_rgb):
    at = make_translation_anim([-2.0, -1.5, -1.0],
                               [[0, 0, 0], [1, 0, 0], [2, 0, 0]])

    assert dr.allclose(at.eval_scalar(-2.0).translation(), [0, 0, 0])
    assert dr.allclose(at.eval_scalar(-1.75).translation(), [0.5, 0, 0])
    assert dr.allclose(at.eval_scalar(-1.0).translation(), [2, 0, 0])

    bounds = at.get_time_bounds()
    assert bounds.min == -2.0 and bounds.max == -1.0

    # A uniform range of negative times must not be rejected by ensure_uniform_keyframes().
    at.ensure_uniform_keyframes()
