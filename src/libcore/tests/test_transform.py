from mitsuba.core import Transform
from mitsuba.core import PCG32
import numpy as np
import numpy.linalg as la

def test01_basics():
    assert(np.allclose(Transform().matrix(), Transform(np.eye(4, 4)).matrix()))
    assert(np.allclose(Transform().inverse_matrix(), Transform(np.eye(4, 4)).inverse_matrix()))
    assert(
        repr(Transform([[1, 2, 3, 4], [5, 6, 7, 8], [9, 10, 11, 12],
                       [13, 14, 15, 16]])) ==
        "[[1, 2, 3, 4],\n [5, 6, 7, 8],\n [9, 10, 11, 12],\n [13, 14, 15, 16]]"
    )

def test02_inverse():
    rng = PCG32()
    for i in range(1000):
        mtx = rng.next_float(4, 4)
        inv_ref = la.inv(mtx)
        trafo = Transform(mtx)
        inv_val = trafo.inverse_matrix()
        assert np.all(trafo.matrix() == mtx)
        assert la.norm(inv_ref-inv_val, 'fro') / la.norm(inv_val, 'fro') < 5e-4

def test03_matmul():
    A = np.array([[1, 2, 3, 4], [5, 6, 7, 8], [9, 10, 11, 12],
                  [13, 14, 15, 16]])
    B = np.array([[1, -2, 3, -4], [5, -6, 7, -8], [9, -10, 11, -12],
                  [13, -14, 15, -16]])
    At = Transform(A)
    Bt = Transform(B)
    assert np.allclose(np.dot(A, B), (At*Bt).matrix())
    assert np.allclose(np.dot(B, A), (Bt*At).matrix())

def test04_transform_point():
    A = np.eye(4)
    A[3, 3] = 2
    assert np.allclose(Transform(A).transform_point([2, 4, 6]), [1, 2, 3])
    assert np.allclose(
        Transform(A).transform_point([[2, 4, 6], [4, 6, 8]]), [[1, 2, 3], [2, 3, 4]])

def test04_transform_vector():
    A = np.eye(4)
    A[3, 3] = 2
    A[1, 1] = .5
    assert np.allclose(Transform(A).transform_vector([2, 4, 6]), [2, 2, 6])
    assert np.allclose(
        Transform(A).transform_vector([[2, 4, 6], [4, 6, 8]]), [[2, 2, 6], [4, 3, 8]])

def test04_transform_normal():
    A = np.eye(4)
    A[3, 3] = 2
    A[1, 2] = .5
    A[1, 1] = .5
    assert np.allclose(Transform(A).transform_normal([2, 4, 6]), [2, 8, 2])
    assert np.allclose(
        Transform(A).transform_normal([[2, 4, 6], [4, 6, 8]]), [[2, 8, 2], [4, 12, 2]])
