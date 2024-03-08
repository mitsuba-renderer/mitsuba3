import pytest
import drjit as dr
import mitsuba as mi


def test01_basics(variant_scalar_rgb):
    bsphere1 = mi.BoundingSphere3f()
    bsphere2 = mi.BoundingSphere3f([0, 1, 2], 1)

    assert str(bsphere1) == "BoundingSphere3f[empty]"
    assert str(bsphere2) == "BoundingSphere3f[\n  center = [0, 1, 2],\n  radius = 1\n]"
    assert bsphere1.radius == 0
    assert dr.all(bsphere1.center == [0, 0, 0])
    assert bsphere2.radius == 1
    assert dr.all(bsphere2.center == [0, 1, 2])
    assert bsphere1 != bsphere2
    assert bsphere2 == bsphere2
    assert bsphere1.empty()
    assert not bsphere2.empty()
    bsphere1.expand([0, 1, 0])
    assert not bsphere1.empty()
    assert bsphere1.contains([0, 0, 1])
    assert not bsphere1.contains([0, 0, 1], strict=True)


def test02_ray_intersect(variant_scalar_rgb):
    bsphere = mi.BoundingSphere3f([0, 0, 0], 1)

    hit, mint, maxt = bsphere.ray_intersect(mi.Ray3f([-5, 0, 0], [1, 0, 0]))
    assert hit and dr.allclose(mint, 4.0) and dr.allclose(maxt, 6.0)

    hit, mint, maxt = bsphere.ray_intersect(mi.Ray3f([-1, -1, -1], dr.normalize(mi.Vector3f(1))))
    assert hit and dr.allclose(mint, dr.sqrt(3) - 1) and dr.allclose(maxt, dr.sqrt(3) + 1)

    hit, mint, maxt = bsphere.ray_intersect(mi.Ray3f([-2, 0, 0], [0, 1, 0]))
    assert not hit


def test06_ray_intersect_vec(variant_scalar_rgb):
    def kernel(o, center, radius):
        bsphere = mi.BoundingSphere3f(center, radius)
        hit, mint, maxt = bsphere.ray_intersect(mi.Ray3f(o, dr.normalize(-o)))

        mint = dr.select(hit, mint, -1.0)
        maxt = dr.select(hit, maxt, -1.0)

        return mint, maxt

    from mitsuba.test.util import check_vectorization
    check_vectorization(kernel, arg_dims = [3, 3, 1])
