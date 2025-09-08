import pytest
import mitsuba as mi
import drjit as dr


def test01_affine_transform_field(variants_all_backends_once):

    scalar_transform = mi.ScalarTransform4f.translate([1, 2, 3])
    field = mi.FieldAffineTransform4f(scalar_transform)

    assert dr.allclose(field.scalar.matrix, scalar_transform.matrix)
    assert dr.allclose(field.value.matrix, scalar_transform.matrix)

    new_transform = mi.ScalarTransform4f.rotate([0, 1, 0], 45)
    field.value = new_transform

    assert dr.allclose(field.scalar.matrix, new_transform.matrix)
    assert dr.allclose(field.value.matrix, new_transform.matrix)


def test02_float_field(variants_all_backends_once):
    field = mi.FieldFloat(1.0)
    assert dr.allclose(field.scalar, 1.0)
    assert dr.allclose(field.value, 1.0)

    field.value = 2.0
    assert dr.allclose(field.scalar, 2.0)
    assert dr.allclose(field.value, 2.0)


def test03_point3f_field(variants_all_backends_once):
    field = mi.FieldPoint3f([1, 2, 3])
    assert dr.allclose(field.scalar, [1, 2, 3])
    assert dr.allclose(field.value, [1, 2, 3])

    field.value = [4, 5, 6]
    assert dr.allclose(field.scalar, [4, 5, 6])
    assert dr.allclose(field.value, [4, 5, 6])
