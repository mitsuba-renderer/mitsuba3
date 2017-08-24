from mitsuba.core import Transform4f, AnimatedTransform
from mitsuba.core import PCG32
import numpy as np
import numpy.linalg as la
import pytest

def test01_basics():
    assert(np.allclose(Transform4f().matrix, Transform4f(np.eye(4, 4)).matrix))
    assert(np.allclose(Transform4f().inverse_transpose, Transform4f(np.eye(4, 4)).inverse_transpose))
    assert(
        repr(Transform4f([[1, 2, 3, 4], [5, 6, 7, 8], [9, 10, 11, 12],
                       [13, 14, 15, 16]])) ==
        "[[1, 2, 3, 4],\n [5, 6, 7, 8],\n [9, 10, 11, 12],\n [13, 14, 15, 16]]"
    )

def test02_inverse():
    rng = PCG32()
    for i in range(1000):
        mtx = rng.next_float((4, 4))
        inv_ref = la.inv(mtx)
        trafo = Transform4f(mtx)
        inv_val = trafo.inverse_transpose.T
        assert np.all(trafo.matrix == mtx)
        assert la.norm(inv_ref-inv_val, 'fro') / la.norm(inv_val, 'fro') < 5e-4

def test03_matmul():
    A = np.array([[1, 2, 3, 4], [5, 6, 7, 8], [9, 10, 11, 12],
                  [13, 14, 15, 16]])
    B = np.array([[1, -2, 3, -4], [5, -6, 7, -8], [9, -10, 11, -12],
                  [13, -14, 15, -16]])
    At = Transform4f(A)
    Bt = Transform4f(B)
    assert np.allclose(np.dot(A, B), (At*Bt).matrix)
    assert np.allclose(np.dot(B, A), (Bt*At).matrix)

def test04_transform_point():
    A = np.eye(4)
    A[3, 3] = 2
    assert np.allclose(Transform4f(A).transform_point([2, 4, 6]), [1, 2, 3])
    assert np.allclose(
        Transform4f(A).transform_point([[2, 4, 6], [4, 6, 8]]), [[1, 2, 3], [2, 3, 4]])

def test05_transform_vector():
    A = np.eye(4)
    A[3, 3] = 2
    A[1, 1] = .5
    assert np.allclose(Transform4f(A).transform_vector([2, 4, 6]), [2, 2, 6])
    assert np.allclose(
        Transform4f(A).transform_vector([[2, 4, 6], [4, 6, 8]]), [[2, 2, 6], [4, 3, 8]])

def test06_transform_normal():
    A = np.eye(4)
    A[3, 3] = 2
    A[1, 2] = .5
    A[1, 1] = .5
    assert np.allclose(Transform4f(A).transform_normal([2, 4, 6]), [2, 8, 2])
    assert np.allclose(
        Transform4f(A).transform_normal([[2, 4, 6], [4, 6, 8]]), [[2, 8, 2], [4, 12, 2]])

def test07_atransform_lookup():
    a = AnimatedTransform()
    trafo0 = Transform4f.translate([1, 2, 3])
    trafo1 = Transform4f.translate([2, 4, 6])
    assert(len(a) == 0)
    assert a.lookup(100) == Transform4f()

    a.append(1, trafo0)
    assert a.lookup(100) == trafo0
    assert(len(a) == 1)
    with pytest.raises(Exception):
        a.append(-1, trafo1)
    with pytest.raises(Exception):
        a.append(1, trafo1)
    a.append(2, trafo1)
    assert(len(a) == 2)
    assert(np.all(a[0].time == 1))
    assert(np.all(a[0].trans == [1, 2, 3]))
    assert(np.all(a[0].quat == [0, 0, 0, 1]))
    assert(np.all(a[0].scale == np.eye(3)))
    assert(np.all(a[1].time == 2))
    assert(np.all(a[1].trans == [2, 4, 6]))
    assert(np.all(a[1].quat == [0, 0, 0, 1]))
    assert(np.all(a[1].scale == np.eye(3)))
    with pytest.raises(IndexError):
        print(a[2].trans)
    a.append(3, trafo1)
    assert np.allclose(a.lookup(0).matrix, trafo0.matrix)
    assert np.allclose(a.lookup(1).matrix, trafo0.matrix)
    assert np.allclose(a.lookup(1.5).matrix, 0.5*(trafo0.matrix + trafo1.matrix))
    assert np.allclose(a.lookup([1.5])[0].matrix, 0.5*(trafo0.matrix + trafo1.matrix))
    assert np.allclose(a.lookup(2).matrix, trafo1.matrix)
    assert np.allclose(a.lookup(3).matrix, trafo1.matrix)

    vec_transform = a.lookup([-100,1.5,100])
    assert np.allclose(vec_transform[0].matrix, trafo0.matrix)
    assert np.allclose(vec_transform[1].matrix, 0.5 * (trafo0.matrix + trafo1.matrix))
    assert np.allclose(vec_transform[2].matrix, trafo1.matrix)

def test08_atransform_interpolate_rotation():
    a = AnimatedTransform()
    axis = np.array([1.0, 2.0, 3.0])
    axis /= la.norm(axis)

    trafo0 = Transform4f.rotate(axis, 0)
    trafo1 = Transform4f.rotate(axis, 30)
    trafo_mid = Transform4f.rotate(axis, 15)
    a.append(2, trafo0)
    a.append(3, trafo1)
    vec_transform = a.lookup([-10,2.5,10])
    assert np.allclose(vec_transform[0].matrix, trafo0.matrix)
    assert np.allclose(vec_transform[1].matrix, trafo_mid.matrix)
    assert np.allclose(vec_transform[2].matrix, trafo1.matrix)

def test08_atransform_interpolate_scale():
    a = AnimatedTransform()
    trafo0 = Transform4f.scale([1,2,3])
    trafo1 = Transform4f.scale([4,5,6])
    trafo_mid = Transform4f.scale([2.5, 3.5, 4.5])
    a.append(2, trafo0)
    a.append(3, trafo1)
    vec_transform = a.lookup([-10,2.5,10])
    assert np.allclose(vec_transform[0].matrix, trafo0.matrix)
    assert np.allclose(vec_transform[1].matrix, trafo_mid.matrix)
    assert np.allclose(vec_transform[2].matrix, trafo1.matrix)
