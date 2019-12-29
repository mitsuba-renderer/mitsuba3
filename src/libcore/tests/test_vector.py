import numpy as np # TODO remove this
import enoki as ek
import pytest
import mitsuba

@pytest.fixture
def variant():
    mitsuba.set_variant('scalar_rgb')

def test01_coordinate_system(variant):
    from mitsuba.core import coordinate_system, Vector3f
    from mitsuba.core.warp import square_to_uniform_sphere

    def branchless_onb(n):
        """
        Building an Orthonormal Basis, Revisited
        Tom Duff, James Burgess, Per Christensen, Christophe Hery,
        Andrew Kensler, Max Liani, and Ryusuke Villemin
        """
        sign = np.copysign(1.0, n[2])
        a = -1.0 / (sign + n[2])
        b = n[0] * n[1] * a
        return (
            Vector3f(
                [1.0 + sign * n[0] * n[0] * a, sign * b, -sign * n[0]]
            ),
            Vector3f(
                [b, sign + n[1] * n[1] * a, -n[1]]
            )
        )

    a = Vector3f([0.70710678, -0. , -0.70710678])
    b = Vector3f([-0.,  1.,  0.])
    assert np.allclose(
        branchless_onb([ek.sqrt(0.5), 0, ek.sqrt(0.5)]), (a, b), atol=1e-6)
    assert np.allclose(
        coordinate_system([ek.sqrt(0.5), 0, ek.sqrt(0.5)]), (a, b), atol=1e-6)

    for u in np.linspace(0, 1, 10):
        for v in np.linspace(0, 1, 10):
            n = square_to_uniform_sphere([u, v])
            s1, t1 = branchless_onb(n)
            s2, t2 = coordinate_system(n)
            assert np.allclose(s1, s2, atol=1e-6)
            assert np.allclose(t1, t2, atol=1e-6)
