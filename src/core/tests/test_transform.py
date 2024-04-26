import pytest
import drjit as dr
import mitsuba as mi

import numpy as np
import numpy.linalg as la


def test01_basics(variants_all_backends_once):
    # Check that default constructor give identity transform
    assert dr.allclose(mi.Transform4f().matrix, dr.identity(mi.Matrix4f))
    assert dr.allclose(mi.Transform4f().inverse_transpose, dr.identity(mi.Matrix4f))

    # Check Matrix and Transfrom construction from Python array and Numpy array
    m1 = [[1, 2, 3, 4], [5, 6, 7, 8], [9, 10, 11, 12], [13, 14, 15, 16]]
    m2 = np.array(m1)
    m3 = mi.Matrix4f(m1)
    m4 = mi.Matrix4f(m2)

    assert(dr.allclose(m3, m2))
    assert(dr.allclose(m4, m2))
    assert(dr.allclose(m3, m4))

    assert dr.allclose(mi.Transform4f(m1).matrix, m1)
    assert dr.allclose(mi.Transform4f(m2).matrix, m2)
    assert dr.allclose(mi.Transform4f(m3).matrix, m3)

def test02_inverse(variant_scalar_rgb, np_rng):
    p = [1, 2, 3]
    def check_inverse(tr, expected):
        inv_trafo = tr.inverse()

        assert dr.allclose(inv_trafo.matrix, np.array(expected)), \
               '\n' + str(inv_trafo.matrix) + '\n' + str(expected)

        assert dr.allclose(inv_trafo @ (tr @ p), p,
                           rtol=1e-4)

    # --- Scale
    trafo = mi.Transform4f().scale([1.0, 10.0, 500.0])
    assert dr.allclose(trafo.matrix,
                       [[1, 0, 0, 0],
                        [0, 10, 0, 0],
                        [0, 0, 500, 0],
                        [0, 0, 0, 1]])
    assert dr.allclose(trafo @ p, [1, 20, 1500])
    check_inverse(trafo, [
        [1,      0,       0,    0],
        [0, 1/10.0,       0,    0],
        [0,      0, 1/500.0,    0],
        [0,      0,       0,    1]
    ])

    # --- Translation
    trafo = mi.Transform4f().translate([1, 0, 1.5])
    assert dr.allclose(trafo.matrix,
                       [[1, 0, 0, 1],
                        [0, 1, 0, 0],
                        [0, 0, 1, 1.5],
                        [0, 0, 0, 1]])
    assert dr.allclose(trafo @ p, [2, 2, 4.5])
    check_inverse(trafo, [
        [1, 0, 0,   -1],
        [0, 1, 0,    0],
        [0, 0, 1, -1.5],
        [0, 0, 0,    1]
    ])

    # --- Perspective                x_fov, near clip, far clip
    trafo = mi.Transform4f().perspective(34.022,         1,     1000)
    # Spot-checked against a known transform from Mitsuba1.
    expected = [
        [3.26860189,          0,        0,         0],
        [         0, 3.26860189,        0,         0],
        [         0,          0, 1.001001, -1.001001],
        [         0,          0,        1,         0]
    ]
    assert dr.allclose(trafo.matrix, expected)
    check_inverse(trafo, la.inv(expected))

    for i in range(10):
        mtx = np_rng.random((4, 4))
        inv_ref = la.inv(mtx)

        trafo = mi.Transform4f(mtx)
        inv_val = trafo.inverse_transpose.T

        assert dr.allclose(trafo.matrix, mtx, atol=1e-4)
        assert la.norm(inv_ref-inv_val, 'fro') / la.norm(inv_val, 'fro') < 5e-4

        p = np_rng.random((3,))
        res = trafo.inverse() @ (trafo @ p)
        assert la.norm(res - p, 2) < 5e-3


def test03_matmul(variant_scalar_rgb):
    A = np.array([[1, 2, 3, 4], [5, 6, 7, 8], [9, 10, 11, 12], [13, 14, 15, 16]])
    B = np.array([[1, -2, 3, -4], [5, -6, 7, -8], [9, -10, 11, -12], [13, -14, 15, -16]])
    At = mi.Transform4f(A)
    Bt = mi.Transform4f(B)
    assert dr.allclose(np.dot(A, B), (At@Bt).matrix)
    assert dr.allclose(np.dot(B, A), (Bt@At).matrix)


def test04_transform_point(variant_scalar_rgb):
    A = mi.Matrix4f(1)
    A[3, 3] = 2
    assert dr.allclose(mi.Transform4f(A) @ mi.Point3f(2, 4, 6), [1, 2, 3])
    assert dr.allclose(mi.Transform4f(A) @ mi.Point3f(4, 6, 8), [2, 3, 4])

    def kernel(p : mi.Point3f):
        return mi.Transform4f(A) @ p

    from mitsuba.test.util import check_vectorization
    check_vectorization(kernel)


def test05_transform_vector(variant_scalar_rgb):
    A = mi.Matrix4f(1)
    A[3, 3] = 2
    A[1, 1] = .5
    assert dr.allclose(mi.Transform4f(A) @ mi.Vector3f(2, 4, 6), [2, 2, 6])

    def kernel(v : mi.Vector3f):
        return mi.Transform4f(A) @ v

    from mitsuba.test.util import check_vectorization
    check_vectorization(kernel)


def test06_transform_normal(variant_scalar_rgb):
    A = mi.Matrix4f(1)
    A[3, 3] = 2
    A[1, 2] = .5
    A[1, 1] = .5
    assert dr.allclose(mi.Transform4f(A)@ mi.Normal3f(2, 4, 6), [2, 8, 2])

    def kernel(n : mi.Normal3f):
        return mi.Transform4f(A) @ n

    from mitsuba.test.util import check_vectorization
    check_vectorization(kernel)


def test07_transform_has_scale(variant_scalar_rgb):
    T = mi.Transform4f()
    assert not T.rotate([1, 0, 0], 0.5).has_scale()
    assert not T.rotate([0, 1, 0], 50).has_scale()
    assert not T.rotate([0, 0, 1], 1e3).has_scale()
    assert not T.translate([41, 1e3, 0]).has_scale()
    assert not T.scale([1, 1, 1]).has_scale()
    assert T.scale([1, 1, 1.1]).has_scale() == True
    assert T.perspective(90, 0.1, 100).has_scale()
    assert T.orthographic(0.01, 200).has_scale()
    assert not T.look_at(origin=[10, -1, 3], target=[1, 1, 2], up=[0, 1, 0]).has_scale()
    assert not T.has_scale()


def test08_transform_chain(variants_all_rgb):
    T = mi.Transform3f()
    assert T.scale(4.0).scale(6.0) == T.scale(4.0) @ T.scale(6.0)
    assert T.scale(4.0).rotate(6.0) == T.scale(4.0) @ T.rotate(6.0)
    assert T.rotate(6.0).scale(4.0) == T.rotate(6.0) @ T.scale(4.0)
    assert T.translate([6.0, 1.0]).scale(4.0) == T.translate([6.0, 1.0]) @ T.scale(4.0)

    T = mi.Transform4f()
    assert T.scale(4.0).scale(6.0) == T.scale(4.0) @ T.scale(6.0)
    assert T.scale(4.0).rotate([1, 0, 0], 6.0) == T.scale(4.0) @ T.rotate([1, 0, 0], 6.0)
    assert T.rotate([1, 0, 0], 6.0).scale(4.0) == T.rotate([1, 0, 0], 6.0) @ T.scale(4.0)
    assert T.translate([6.0, 1.0, 0.0]).scale(4.0) == T.translate([6.0, 1.0, 0.0]) @ T.scale(4.0)
    assert T.perspective(0.5, 0.5, 1.5).scale(4.0) == T.perspective(0.5, 0.5, 1.5) @ T.scale(4.0)
    assert T.orthographic(0.5, 1.5).scale(4.0) == T.orthographic(0.5, 1.5) @ T.scale(4.0)
    assert T.look_at([0, 0, 0], [0, 1, 0], [1, 0, 0]).scale(4.0) == T.look_at([0, 0, 0], [0, 1, 0], [1, 0, 0]) @ T.scale(4.0)
    assert T.from_frame(mi.Frame3f([1, 0, 0])).scale(4.0) == T.from_frame(mi.Frame3f([1, 0, 0])) @ T.scale(4.0)
    assert T.to_frame(mi.Frame3f([1, 0, 0])).scale(4.0) == T.to_frame(mi.Frame3f([1, 0, 0])) @ T.scale(4.0)


# def test08_atransform_construct(variant_scalar_rgb):
#     t = mi.Transform4f.rotate([1, 0, 0], 30)
#     a = AnimatedTransform(t)

#     t0 = a.eval(0)
#     assert t0 == t
#     assert not t0.has_scale()
#     # Animation is constant over time
#     for v in [10, 200, 1e5]:
#         assert np.all(t0 == a.eval(v))


# def test10_atransform_interpolate_rotation(variant_scalar_rgb):
#     a = AnimatedTransform()
#     axis = np.array([1.0, 2.0, 3.0])
#     axis /= la.norm(axis)

#     trafo0 = mi.Transform4f.rotate(axis, 0)
#     trafo1 = mi.Transform4f.rotate(axis, 30)
#     trafo_mid = mi.Transform4f.rotate(axis, 15)
#     a.append(2, trafo0)
#     a.append(3, trafo1)

#     assert dr.allclose(a.eval(-10).matrix, trafo0.matrix)
#     assert dr.allclose(a.eval(2.5).matrix, trafo_mid.matrix)
#     assert dr.allclose(a.eval( 10).matrix, trafo1.matrix)


# def test11_atransform_interpolate_scale(variant_scalar_rgb):
#     a = AnimatedTransform()
#     trafo0 = mi.Transform4f.scale([1,2,3])
#     trafo1 = mi.Transform4f.scale([4,5,6])
#     trafo_mid = mi.Transform4f.scale([2.5, 3.5, 4.5])
#     a.append(2, trafo0)
#     a.append(3, trafo1)
#     assert dr.allclose(a.eval(-10).matrix, trafo0.matrix)
#     assert dr.allclose(a.eval(2.5).matrix, trafo_mid.matrix)
#     assert dr.allclose(a.eval( 10).matrix, trafo1.matrix)
