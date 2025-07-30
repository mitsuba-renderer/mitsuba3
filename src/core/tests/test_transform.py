import pytest
import drjit as dr
import mitsuba as mi

import numpy as np
import numpy.linalg as la


# Common decorator for parameterizing tests over transform types
transform_types = pytest.mark.parametrize("transform_type", [
    "AffineTransform4f",
    "ProjectiveTransform4f"
])


@transform_types
def test01_basics(variants_all_backends_once, transform_type):
    TransformClass = getattr(mi, transform_type)

    # Check that default constructor give identity transform
    assert dr.allclose(TransformClass().matrix, dr.identity(mi.Matrix4f))
    assert dr.allclose(TransformClass().inverse_transpose, dr.identity(mi.Matrix4f))

    # Check Matrix and Transform construction from Python array and Numpy array
    # Use proper matrices for each transform type
    if transform_type == "AffineTransform4f":
        # For AffineTransform, use invertible affine matrices (last row [0,0,0,1])
        m1 = [[2, 0, 0, 1], [0, 3, 0, 2], [0, 0, 4, 3], [0, 0, 0, 1]]
    else:
        # For ProjectiveTransform, use invertible 4x4 matrices
        m1 = [[1, 0, 0, 1], [0, 1, 0, 2], [0, 0, 1, 3], [0, 0, 0, 2]]

    m2 = np.array(m1)
    m3 = mi.Matrix4f(m1)
    m4 = mi.Matrix4f(m2)

    assert(dr.allclose(m3, m2))
    assert(dr.allclose(m4, m2))
    assert(dr.allclose(m3, m4))

    assert dr.allclose(TransformClass(m1).matrix, m1)
    assert dr.allclose(TransformClass(m2).matrix, m2)
    assert dr.allclose(TransformClass(m3).matrix, m3)

    mi.ProjectiveTransform4f(mi.ScalarProjectiveTransform4f(m1))
    mi.AffineTransform4f(mi.ScalarAffineTransform4f(m1))

    mi.ProjectiveTransform4f(mi.AffineTransform4f(m1))
    mi.AffineTransform4f(mi.ProjectiveTransform4f(m1))

    if dr.is_dynamic_v(mi.Float):
        # Check that it can be constructed from a wide Numpy array
        mat = np.tile(np.eye(4, 4)[..., None], (1, 1, 10))
        mat[3, 2, 0] += 0.5
        mat[1, 0, 7] += 0.7
        trafo1 = TransformClass(mi.Matrix4f(mat))
        assert dr.allclose(trafo1.matrix, mat)
        assert dr.width(trafo1.matrix) == 10
        trafo2 = TransformClass(mat)
        assert dr.allclose(trafo2.matrix, mat)
        assert dr.width(trafo2.matrix) == 10


@transform_types
def test02_inverse(variant_scalar_rgb, np_rng, transform_type):
    TransformClass = getattr(mi, transform_type)

    p = [1, 2, 3]
    def check_inverse(tr, expected):
        inv_trafo = tr.inverse()

        assert dr.allclose(inv_trafo.matrix, np.array(expected)), \
               '\n' + str(inv_trafo.matrix) + '\n' + str(expected)

        assert dr.allclose(inv_trafo @ (tr @ p), p,
                           rtol=1e-4)

    # --- Scale
    trafo = TransformClass().scale([1.0, 10.0, 500.0])
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
    trafo = TransformClass().translate([1, 0, 1.5])
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

    # --- Perspective (only available for ProjectiveTransform)
    if transform_type == "ProjectiveTransform4f":
        trafo = TransformClass().perspective(34.022,         1,     1000)
        # Spot-checked against a known transform from Mitsuba1.
        expected = [
            [3.26860189,          0,        0,         0],
            [         0, 3.26860189,        0,         0],
            [         0,          0, 1.001001, -1.001001],
            [         0,          0,        1,         0]
        ]
        assert dr.allclose(trafo.matrix, expected)
        check_inverse(trafo, la.inv(expected))

    # Test random matrices, but only for valid cases
    for i in range(10):
        mtx = np_rng.random((4, 4))

        # For AffineTransform, ensure the matrix is affine (bottom row should be [0,0,0,1])
        if transform_type == "AffineTransform4f":
            mtx[3, :] = [0, 0, 0, 1]

        inv_ref = la.inv(mtx)

        trafo = TransformClass(mtx)
        inv_val = trafo.inverse_transpose.T

        assert dr.allclose(trafo.matrix, mtx, atol=1e-4)
        assert la.norm(inv_ref-inv_val, 'fro') / la.norm(inv_val, 'fro') < 5e-4

        p = np_rng.random((3,))
        res = trafo.inverse() @ (trafo @ p)
        assert la.norm(res - p, 2) < 5e-3


@transform_types
def test03_matmul(variant_scalar_rgb, transform_type):
    TransformClass = getattr(mi, transform_type)

    # Use proper matrices for each transform type
    if transform_type == "AffineTransform4f":
        # For AffineTransform, use invertible affine matrices (last row [0,0,0,1])
        A = np.array([[2, 0, 0, 1], [0, 3, 0, 2], [0, 0, 4, 3], [0, 0, 0, 1]])
        B = np.array([[1, 0, 0, 4], [0, 2, 0, 5], [0, 0, 3, 6], [0, 0, 0, 1]])
    else:
        # For ProjectiveTransform, use invertible 4x4 matrices
        A = np.array([[1, 0, 0, 1], [0, 1, 0, 2], [0, 0, 1, 3], [0, 0, 0, 2]])
        B = np.array([[2, 0, 0, 1], [0, 2, 0, 1], [0, 0, 2, 1], [0, 0, 0, 1]])

    At = TransformClass(A)
    Bt = TransformClass(B)
    result_AB = At @ Bt
    result_BA = Bt @ At

    # Test matrix multiplication
    assert dr.allclose(np.dot(A, B), result_AB.matrix)
    assert dr.allclose(np.dot(B, A), result_BA.matrix)

    # Test inverse_transpose multiplication: (A * B)^{-T} = A^{-T} * B^{-T}
    expected_inv_t_AB = np.dot(At.inverse_transpose, Bt.inverse_transpose)
    expected_inv_t_BA = np.dot(Bt.inverse_transpose, At.inverse_transpose)
    assert dr.allclose(expected_inv_t_AB, result_AB.inverse_transpose)
    assert dr.allclose(expected_inv_t_BA, result_BA.inverse_transpose)


@transform_types
def test04_transform_point(variant_scalar_rgb, transform_type):
    TransformClass = getattr(mi, transform_type)

    A = mi.Matrix4f(1)
    A[1, 1] = 2
    A[3, 3] = 2

    # Different behavior for different transform types
    if transform_type == "ProjectiveTransform4f":
        assert dr.allclose(TransformClass(A) @ mi.Point3f(2, 4, 6), [1, 4, 3])
    else:
        assert dr.allclose(TransformClass(A) @ mi.Point3f(2, 4, 6), [2, 8, 6])


@transform_types
def test05_transform_vector(variant_scalar_rgb, transform_type):
    TransformClass = getattr(mi, transform_type)

    A = mi.Matrix4f(1)
    A[3, 3] = 2
    A[1, 1] = .5
    assert dr.allclose(TransformClass(A) @ mi.Vector3f(2, 4, 6), [2, 2, 6])


@transform_types
def test06_transform_normal(variant_scalar_rgb, transform_type):
    TransformClass = getattr(mi, transform_type)

    A = mi.Matrix4f(1)
    A[3, 3] = 2
    A[1, 2] = .5
    A[1, 1] = .5

    assert dr.allclose(TransformClass(A) @ mi.Normal3f(2, 4, 7), [2, 8, 3])

@transform_types
def test07_transform_has_scale(variant_scalar_rgb, transform_type):
    TransformClass = getattr(mi, transform_type)

    T = TransformClass()
    assert not T.rotate([1, 0, 0], 0.5).has_scale()
    assert not T.rotate([0, 1, 0], 50).has_scale()
    assert not T.rotate([0, 0, 1], 1e3).has_scale()
    assert not T.translate([41, 1e3, 0]).has_scale()
    assert not T.scale([1, 1, 1]).has_scale()
    assert T.scale([1, 1, 1.1]).has_scale() == True

    # Perspective only available for ProjectiveTransform
    if transform_type == "ProjectiveTransform4f":
        assert T.perspective(90, 0.1, 100).has_scale()

    assert T.orthographic(0.01, 200).has_scale()
    assert not T.look_at(origin=[10, -1, 3], target=[1, 1, 2], up=[0, 1, 0]).has_scale()
    assert not T.has_scale()


@pytest.mark.parametrize("transform_type_3d,transform_type_4d", [
    ("AffineTransform3f", "AffineTransform4f"),
    ("ProjectiveTransform3f", "ProjectiveTransform4f")
])
def test08_transform_chain(variants_all_rgb, transform_type_3d, transform_type_4d):
    TransformClass3f = getattr(mi, transform_type_3d)
    TransformClass4f = getattr(mi, transform_type_4d)

    T = TransformClass3f()
    assert T.scale(4.0).scale(6.0) == T.scale(4.0) @ T.scale(6.0)
    assert T.scale(4.0).rotate(6.0) == T.scale(4.0) @ T.rotate(6.0)
    assert T.rotate(6.0).scale(4.0) == T.rotate(6.0) @ T.scale(4.0)
    assert T.translate([6.0, 1.0]).scale(4.0) == T.translate([6.0, 1.0]) @ T.scale(4.0)

    T = TransformClass4f()
    assert T.scale(4.0).scale(6.0) == T.scale(4.0) @ T.scale(6.0)
    assert T.scale(4.0).rotate([1, 0, 0], 6.0) == T.scale(4.0) @ T.rotate([1, 0, 0], 6.0)
    assert T.rotate([1, 0, 0], 6.0).scale(4.0) == T.rotate([1, 0, 0], 6.0) @ T.scale(4.0)
    assert T.translate([6.0, 1.0, 0.0]).scale(4.0) == T.translate([6.0, 1.0, 0.0]) @ T.scale(4.0)

    # Perspective only available for ProjectiveTransform
    if transform_type_4d == "ProjectiveTransform4f":
        assert T.perspective(0.5, 0.5, 1.5).scale(4.0) == T.perspective(0.5, 0.5, 1.5) @ T.scale(4.0)

    # Orthographic is available for both transform types
    assert T.orthographic(0.5, 1.5).scale(4.0) == T.orthographic(0.5, 1.5) @ T.scale(4.0)

    assert T.look_at([0, 0, 0], [0, 1, 0], [1, 0, 0]).scale(4.0) == T.look_at([0, 0, 0], [0, 1, 0], [1, 0, 0]) @ T.scale(4.0)
    assert T.from_frame(mi.Frame3f([1, 0, 0])).scale(4.0) == T.from_frame(mi.Frame3f([1, 0, 0])) @ T.scale(4.0)
    assert T.to_frame(mi.Frame3f([1, 0, 0])).scale(4.0) == T.to_frame(mi.Frame3f([1, 0, 0])) @ T.scale(4.0)


@transform_types
def test06_dual_callable(variant_scalar_rgb, transform_type):
    """Test the ability to call Transform methods as class/instance methods"""

    T = getattr(mi, transform_type)

    # Test that class methods create identity transform first
    assert dr.allclose(T.translate([1, 2, 3]).matrix, T().translate([1, 2, 3]).matrix)
    assert dr.allclose(T.scale([2, 2, 2]).matrix, T().scale([2, 2, 2]).matrix)
    assert dr.allclose(T.rotate([0, 1, 0], 45).matrix, T().rotate([0, 1, 0], 45).matrix)

    # Test that chaining works correctly
    t1 = T.translate([1, 0, 0]).rotate([0, 1, 0], 45).scale([2, 2, 2])
    t2 = T().translate([1, 0, 0]).rotate([0, 1, 0], 45).scale([2, 2, 2])
    assert dr.allclose(t1.matrix, t2.matrix)
