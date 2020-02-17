
import pytest
import enoki as ek
import mitsuba

import numpy as np
import numpy.linalg as la

def test01_basics(variant_scalar_rgb):
    from mitsuba.core import Transform4f, Matrix4f

    assert(ek.allclose(Transform4f().matrix, Transform4f(Matrix4f.identity()).matrix))
    assert(ek.allclose(Transform4f().inverse_transpose, Transform4f(Matrix4f.identity()).inverse_transpose))

    assert(
        repr(Transform4f(Matrix4f([[1, 2, 3, 4], [5, 6, 7, 8],
                          [9, 10, 11, 12], [13, 14, 15, 16]]))) ==
        "[[1, 2, 3, 4],\n [5, 6, 7, 8],\n [9, 10, 11, 12],\n [13, 14, 15, 16]]"
    )

    m1 = [
       [ 0.295806  ,  0.19926196,  0.46541812,  0.31066751],
       [ 0.19926196,  0.4046916 ,  0.86071449,  0.14933838],
       [ 0.46541812,  0.86071449,  0.75046024,  1.90353475],
       [ 0.31066751,  0.14933838,  0.90353475,  0.30514665]
    ]

    m2 = np.array(m1)
    m3 = Matrix4f(m1)
    m4 = Matrix4f(np.array(m1))

    assert(ek.allclose(m1, m2))
    # assert(ek.allclose(m1, m3)) # TODO not handles yet (row-major vs column major)
    # assert(ek.allclose(m1, m4)) # TODO not handles yet (row-major vs column major)
    assert(ek.allclose(m2, m3))
    assert(ek.allclose(m2, m4))
    assert(ek.allclose(m3, m4))

    trafo = Transform4f(m1)

    assert(ek.allclose(trafo.matrix, Transform4f(m2).matrix))
    assert(ek.allclose(trafo.matrix, Transform4f(m3).matrix))
    assert(ek.allclose(trafo.matrix, Transform4f(m4).matrix))

    assert la.norm(trafo.matrix - m2) < 1e-5
    assert la.norm(trafo.inverse_transpose - la.inv(m2).T) < 1e-5


def test02_inverse(variant_scalar_rgb):
    from mitsuba.core import Transform4f, Matrix4f

    p = [1, 2, 3]
    def check_inverse(tr, expected):
        inv_trafo = tr.inverse()

        assert ek.allclose(inv_trafo.matrix, np.array(expected)), \
               '\n' + str(inv_trafo.matrix) + '\n' + str(expected)

        assert ek.allclose(inv_trafo.transform_point(tr.transform_point(p)), p,
                           rtol=1e-4)

    # --- Scale
    trafo = Transform4f.scale([1.0, 10.0, 500.0])
    assert ek.all(trafo.matrix == np.array([
        [1,    0,    0,    0],
        [0,   10,    0,    0],
        [0,    0,  500,    0],
        [0,    0,    0,    1]
    ]))
    assert ek.all(trafo.transform_point(p) == np.array([1, 20, 1500]))
    check_inverse(trafo, [
        [1,      0,       0,    0],
        [0, 1/10.0,       0,    0],
        [0,      0, 1/500.0,    0],
        [0,      0,       0,    1]
    ])
    # --- Translation
    trafo = Transform4f.translate([1, 0, 1.5])
    assert ek.all(trafo.matrix == np.array([
        [1, 0, 0,    1],
        [0, 1, 0,    0],
        [0, 0, 1,  1.5],
        [0, 0, 0,    1],
    ]))
    assert ek.all(trafo.transform_point(p) == np.array([2, 2, 4.5]))
    check_inverse(trafo, [
        [1, 0, 0,   -1],
        [0, 1, 0,    0],
        [0, 0, 1, -1.5],
        [0, 0, 0,    1]
    ])
    # --- Perspective                x_fov, near clip, far clip
    trafo = Transform4f.perspective(34.022,         1,     1000)
    # Spot-checked against a known transform from Mitsuba1.
    expected = np.array([
        [3.26860189,          0,        0,         0],
        [         0, 3.26860189,        0,         0],
        [         0,          0, 1.001001, -1.001001],
        [         0,          0,        1,         0]
    ])
    assert ek.allclose(trafo.matrix, expected)
    check_inverse(trafo, la.inv(expected))

    for i in range(10):
        mtx = np.random.random((4, 4))
        inv_ref = la.inv(mtx)
        trafo = Transform4f(mtx)
        inv_val = trafo.inverse_transpose.T
        assert ek.all(trafo.matrix == mtx)
        assert la.norm(inv_ref-inv_val, 'fro') / la.norm(inv_val, 'fro') < 5e-4

        p = np.random.random((3,))
        res = trafo.inverse().transform_point(trafo.transform_point(p))
        assert la.norm(res - p, 2) < 5e-3


def test03_matmul(variant_scalar_rgb):
    from mitsuba.core import Transform4f, Matrix4f

    A = np.array([[1, 2, 3, 4], [5, 6, 7, 8], [9, 10, 11, 12], [13, 14, 15, 16]])
    B = np.array([[1, -2, 3, -4], [5, -6, 7, -8], [9, -10, 11, -12], [13, -14, 15, -16]])
    At = Transform4f(A)
    Bt = Transform4f(B)
    assert ek.allclose(np.dot(A, B), (At*Bt).matrix)
    assert ek.allclose(np.dot(B, A), (Bt*At).matrix)


def test04_transform_point(variant_scalar_rgb):
    from mitsuba.core import Transform4f

    A = np.eye(4)
    A[3, 3] = 2
    assert ek.allclose(Transform4f(A).transform_point([2, 4, 6]), [1, 2, 3])

    try:
        mitsuba.set_variant("packet_rgb")
        from mitsuba.core import Transform4f as Transform4fX
    except:
        return

    assert ek.allclose(Transform4fX(A).transform_point([[2, 4], [4, 6], [6, 8]]), [[1, 2], [2, 3], [3, 4]])


def test05_transform_vector(variant_scalar_rgb):
    from mitsuba.core import Transform4f

    A = np.eye(4)
    A[3, 3] = 2
    A[1, 1] = .5
    assert ek.allclose(Transform4f(A).transform_vector([2, 4, 6]), [2, 2, 6])

    try:
        mitsuba.set_variant("packet_rgb")
        from mitsuba.core import Transform4f as Transform4fX
    except:
        return

    assert ek.allclose(
        Transform4fX(A).transform_vector([[2, 4], [4, 6], [6, 8]]), [[2, 4], [2, 3], [6, 8]])


def test06_transform_normal(variant_scalar_rgb):
    from mitsuba.core import Transform4f

    A = np.eye(4)
    A[3, 3] = 2
    A[1, 2] = .5
    A[1, 1] = .5
    assert ek.allclose(Transform4f(A).transform_normal([2, 4, 6]), [2, 8, 2])

    try:
        mitsuba.set_variant("packet_rgb")
        from mitsuba.core import Transform4f as Transform4fX
    except:
        return

    assert ek.allclose(
        Transform4fX(A).transform_normal([[2, 4], [4, 6], [6, 8]]), [[2, 4], [8, 12], [2, 2]])


# def test07_transform_has_scale(variant_scalar_rgb):
#     try:
#         from mitsuba.packet_rgb.core import Transform4f as Transform4fX
#     except:
#         pytest.skip("packet_rgb mode not enabled")

#     t = Transform4fX(11)
#     t[0] = Transform4f.rotate([1, 0, 0], 0.5)
#     t[1] = Transform4f.rotate([0, 1, 0], 50)
#     t[2] = Transform4f.rotate([0, 0, 1], 1e3)
#     t[3] = Transform4f.translate([41, 1e3, 0])
#     t[4] = Transform4f.scale([1, 1, 1])
#     t[5] = Transform4f.scale([1, 1, 1.1])
#     t[6] = Transform4f.perspective(90, 0.1, 100)
#     t[7] = Transform4f.orthographic(0.01, 200)
#     t[8] = Transform4f.look_at(origin=[10, -1, 3], target=[1, 1, 2], up=[0, 1, 0])
#     t[9] = Transform4f()
#     t[10] = AnimatedTransform(Transform4f()).eval(0)

#     # Vectorized
#     expected = [
#         False, False, False, False, False,
#         True, True, True, False, False, False
#     ]
#     assert np.all(t.has_scale() == expected)

#     # Single
#     for i in range(11):
#         assert t[i].has_scale() == expected[i]

#     # Spot
#     assert la.norm(np.all(t[3].inverse_transpose - np.array([
#         [ 1,   0, 0,   0],
#         [ 0,   1, 0,   0],
#         [ 0,   0, 1,   0],
#         [41, 1e3, 0,   1]
#     ]))) < 1e-5


# def test08_atransform_construct(variant_scalar_rgb):
#     t = Transform4f.rotate([1, 0, 0], 30)
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

#     trafo0 = Transform4f.rotate(axis, 0)
#     trafo1 = Transform4f.rotate(axis, 30)
#     trafo_mid = Transform4f.rotate(axis, 15)
#     a.append(2, trafo0)
#     a.append(3, trafo1)

#     assert ek.allclose(a.eval(-10).matrix, trafo0.matrix)
#     assert ek.allclose(a.eval(2.5).matrix, trafo_mid.matrix)
#     assert ek.allclose(a.eval( 10).matrix, trafo1.matrix)


# def test11_atransform_interpolate_scale(variant_scalar_rgb):
#     a = AnimatedTransform()
#     trafo0 = Transform4f.scale([1,2,3])
#     trafo1 = Transform4f.scale([4,5,6])
#     trafo_mid = Transform4f.scale([2.5, 3.5, 4.5])
#     a.append(2, trafo0)
#     a.append(3, trafo1)
#     assert ek.allclose(a.eval(-10).matrix, trafo0.matrix)
#     assert ek.allclose(a.eval(2.5).matrix, trafo_mid.matrix)
#     assert ek.allclose(a.eval( 10).matrix, trafo1.matrix)
