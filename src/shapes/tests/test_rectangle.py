import numpy as np
import pytest

import mitsuba
from mitsuba.core      import Ray3f, Ray3fX
from mitsuba.core.xml  import load_string

UNSUPPORTED = mitsuba.USE_EMBREE or mitsuba.USE_OPTIX

def example_rectangle(scale = (1, 1, 1), translate = (0, 0, 0)):
    return load_string("""<shape version="2.0.0" type="rectangle">
        <transform name="to_world">
            <scale x="{}" y="{}" z="{}"/>
            <translate x="{}" y="{}" z="{}"/>
        </transform>
    </shape>""".format(scale[0], scale[1], scale[2],
                       translate[0], translate[1], translate[2]))


def test01_create():
    s = example_rectangle()
    assert s is not None
    assert s.primitive_count() == 1
    assert np.allclose(s.surface_area(), 4.0)


def test02_bbox():
    sy = 2.5
    for sx in [1, 2, 4]:
        for translate in [np.array([1.3, -3.0, 5]),
                          np.array([-10000, 3.0, 31])]:
            s = example_rectangle((sx, sy, 1.0), translate)
            b = s.bbox()

            assert np.allclose(s.surface_area(), sx * sy * 4)

            assert b.valid()
            assert np.allclose(b.center(), translate)
            assert np.allclose(b.min, translate - np.array([sx, sy, 0.0]))
            assert np.allclose(b.max, translate + np.array([sx, sy, 0.0]))


@pytest.mark.xfail(condition=UNSUPPORTED,
                   reason="Shape intersections not implemented with Embree or OptiX")
def test03_ray_intersect():
    scene = load_string("""<scene version="2.0.0">
        <shape type="rectangle">
            <transform name="to_world">
                <scale x="2" y="0.5" z="1"/>
            </transform>
        </shape>
    </scene>""")

    # Scalar
    n = 15
    coords = np.linspace(-1, 1, n)
    rays = [Ray3f(o=[a, a, 5], d=[0, 0, -1], time=0.0,
                  wavelengths=[400, 500, 600, 700]) for a in coords]
    si_scalar = []
    valid_count = 0
    for i in range(n):
        # print(rays[i])
        its_found = scene.ray_test(rays[i])
        si = scene.ray_intersect(rays[i])

        assert its_found == (abs(coords[i]) <= 0.5)
        assert si.is_valid() == its_found
        si_scalar.append(si)
        valid_count += its_found

    assert valid_count == 7

    packet = Ray3fX(n)
    for i in range(n):
        packet[i] = rays[i]
    si_p = scene.ray_intersect(packet)
    its_found_p = scene.ray_test(packet)

    assert np.all(si_p.is_valid() == its_found_p)
    for i in range(n):
        assert np.allclose(si_p[i].t, si_scalar[i].t)
