import pytest
import mitsuba
import enoki as ek


def test01_basics(variant_scalar_rgb):
    from mitsuba.core import BoundingSphere3f as BSphere

    bsphere1 = BSphere()
    bsphere2 = BSphere([0, 1, 2], 1)

    assert str(bsphere1) == "BoundingSphere3f[empty]"
    assert str(bsphere2) == "BoundingSphere3f[\n  center = [0, 1, 2],\n  radius = 1\n]"
    assert bsphere1.radius == 0
    assert (bsphere1.center == [0, 0, 0])
    assert bsphere2.radius == 1
    assert (bsphere2.center == [0, 1, 2])
    assert bsphere1 != bsphere2
    assert bsphere2 == bsphere2
    assert bsphere1.empty()
    assert not bsphere2.empty()
    bsphere1.expand([0, 1, 0])
    assert not bsphere1.empty()
    assert bsphere1.contains([0, 0, 1])
    assert not bsphere1.contains([0, 0, 1], strict=True)


def test02_ray_intersect(variant_scalar_rgb):
    from mitsuba.core import BoundingSphere3f as BSphere, Vector3f
    from mitsuba.core import Ray3f

    bsphere = BSphere([0, 0, 0], 1)

    hit, mint, maxt = bsphere.ray_intersect(Ray3f([-5, 0, 0], [1, 0, 0], 0, []))
    assert hit and ek.allclose(mint, 4.0) and ek.allclose(maxt, 6.0)

    hit, mint, maxt = bsphere.ray_intersect(Ray3f([-1, -1, -1], ek.normalize(Vector3f(1)), 0, []))
    assert hit and ek.allclose(mint, ek.sqrt(3) - 1) and ek.allclose(maxt, ek.sqrt(3) + 1)

    hit, mint, maxt = bsphere.ray_intersect(Ray3f([-2, 0, 0], [0, 1, 0], 0, []))
    assert not hit


def test06_ray_intersect_vec(variant_scalar_rgb):
    from mitsuba.python.test.util import check_vectorization

    def kernel(o, center, radius):
        from mitsuba.core import BoundingSphere3f as BSphere
        from mitsuba.core import Ray3f

        bsphere = BSphere(center, radius)
        hit, mint, maxt = bsphere.ray_intersect(Ray3f(o, ek.normalize(-o), 0, []))

        mint = ek.select(hit, mint, -1.0)
        maxt = ek.select(hit, maxt, -1.0)

        return mint, maxt

    check_vectorization(kernel, arg_dims = [3, 3, 1])
