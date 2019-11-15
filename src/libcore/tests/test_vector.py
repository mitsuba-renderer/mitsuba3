from mitsuba.scalar_rgb.core import coordinate_system
from mitsuba.scalar_rgb.core.warp import square_to_uniform_sphere
import numpy as np

def branchless_onb(n):
    """
    Building an Orthonormal Basis, Revisited
    Tom Duff, James Burgess, Per Christensen, Christophe Hery,
    Andrew Kensler, Max Liani, and Ryusuke Villemin
    """
    sign = np.copysign(1.0, n[2])
    a = -1.0 / (sign + n[2])
    b = n[0] * n[1] * a;
    return (
        np.array(
            [1.0 + sign * n[0] * n[0] * a, sign * b, -sign * n[0]]
        ),
        np.array(
            [b, sign + n[1] * n[1] * a, -n[1]]
        )
    )

def test01_coordinate_system():
    a = np.array([0.70710678, -0. , -0.70710678])
    b = np.array([-0.,  1.,  0.])
    assert np.allclose(
        branchless_onb([np.sqrt(0.5), 0, np.sqrt(0.5)]), (a, b), atol=1e-6)
    assert np.allclose(
        coordinate_system([np.sqrt(0.5), 0, np.sqrt(0.5)]), (a, b), atol=1e-6)

    for u in np.linspace(0, 1, 10):
        for v in np.linspace(0, 1, 10):
            n = square_to_uniform_sphere([u, v])
            s1, t1 = branchless_onb(n)
            s2, t2 = coordinate_system(n)
            assert np.allclose(s1, s2, atol=1e-6)
            assert np.allclose(t1, t2, atol=1e-6)
