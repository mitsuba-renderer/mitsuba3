import enoki as ek
from enoki.dynamic import Float32 as Float
import pytest
import mitsuba


def test01_coordinate_system(variant_scalar_rgb):
    from mitsuba.core import coordinate_system, warp

    def branchless_onb(n):
        """
        Building an Orthonormal Basis, Revisited
        Tom Duff, James Burgess, Per Christensen, Christophe Hery,
        Andrew Kensler, Max Liani, and Ryusuke Villemin
        """
        sign = ek.copysign(1.0, n[2])
        a = -1.0 / (sign + n[2])
        b = n[0] * n[1] * a
        return (
            [1.0 + sign * n[0] * n[0] * a, sign * b, -sign * n[0]],
            [b, sign + n[1] * n[1] * a, -n[1]]
        )

    a = [0.70710678, -0. , -0.70710678]
    b = [-0.,  1.,  0.]
    assert ek.allclose(
        branchless_onb([ek.sqrt(0.5), 0, ek.sqrt(0.5)]), (a, b), atol=1e-6)
    assert ek.allclose(
        coordinate_system([ek.sqrt(0.5), 0, ek.sqrt(0.5)]), (a, b), atol=1e-6)

    for u in ek.linspace(Float, 0, 1, 10):
        for v in ek.linspace(Float, 0, 1, 10):
            n = warp.square_to_uniform_sphere([u, v])
            s1, t1 = branchless_onb(n)
            s2, t2 = coordinate_system(n)
            assert ek.allclose(s1, s2, atol=1e-6)
            assert ek.allclose(t1, t2, atol=1e-6)
