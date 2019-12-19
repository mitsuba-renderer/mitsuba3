import numpy as np
import pytest

import mitsuba
from mitsuba.scalar_rgb.core      import Ray3f
from mitsuba.scalar_rgb.core.xml  import load_string
from mitsuba.scalar_rgb.core.math import Pi
from mitsuba.core import traverse

UNSUPPORTED = mitsuba.USE_EMBREE or mitsuba.USE_OPTIX

def example_disk(scale = (1, 1, 1), translate = (0, 0, 0)):
    return load_string("""<shape version="2.0.0" type="disk">
        <transform name="to_world">
            <scale x="{}" y="{}" z="{}"/>
            <translate x="{}" y="{}" z="{}"/>
        </transform>
    </shape>""".format(scale[0], scale[1], scale[2],
                       translate[0], translate[1], translate[2]))

def example_scene(scale = (1, 1, 1), translate = (0, 0, 0)):
    return load_string("""<scene version='2.0.0'>
        <shape type="disk">
            <transform name="to_world">
                <scale x="{}" y="{}" z="{}"/>
                <translate x="{}" y="{}" z="{}"/>
            </transform>
        </shape>
    </scene>""".format(scale[0], scale[1], scale[2],
                       translate[0], translate[1], translate[2]))


def test01_create():
    s = example_disk()
    assert s is not None
    assert s.primitive_count() == 1
    assert np.allclose(s.surface_area(), Pi)


def test02_bbox():
    sy = 2.5
    for sx in [1, 2, 4]:
        for translate in [np.array([1.3, -3.0, 5]),
                          np.array([-10000, 3.0, 31])]:
            s = example_disk((sx, sy, 1.0), translate)
            b = s.bbox()

            assert np.allclose(s.surface_area(), sx * sy * Pi)

            assert b.valid()
            assert np.allclose(b.center(), translate)
            assert np.allclose(b.min, translate - np.array([sx, sy, 0.0]))
            assert np.allclose(b.max, translate + np.array([sx, sy, 0.0]))


@pytest.mark.xfail(condition=UNSUPPORTED,
                   reason="Shape intersections not implemented with Embree or OptiX")
def test03_ray_intersect():
    for r in [1, 3, 5]:
        for translate in [np.array([0.0, 0.0, 0.0]),
                          np.array([1.0, -5.0, 0.0])]:
            s = example_scene((r, r, 1.0), translate)

            # grid size
            n = 10

            wl = np.zeros(3)

            xx = np.linspace(-1, 1, n)
            yy = np.linspace(-1, 1, n)

            for x in xx:
                for y in yy:
                    x = 1.1*r*(x - translate[0])
                    y = 1.1*r*(y - translate[1])

                    ray = Ray3f(o=[x, y, -10], d=[0, 0, 1],
                                time=0.0, wavelengths=wl)
                    si_found = s.ray_test(ray)

                    assert si_found == (x**2 + y**2 <= r*r)

                    if  si_found:
                        ray = Ray3f(o=[x, y, -10], d=[0, 0, 1],
                                    time=0.0, wavelengths=wl)

                        si = s.ray_intersect(ray)
                        ray_u = Ray3f(ray)
                        ray_v = Ray3f(ray)
                        eps = 1e-4
                        ray_u.o += si.dp_du * eps
                        ray_v.o += si.dp_dv * eps
                        si_u = s.ray_intersect(ray_u)
                        si_v = s.ray_intersect(ray_v)

                        dn = si.shape.normal_derivative(si, True, True)

                        if si_u.is_valid():
                            dp_du = (si_u.p - si.p) / eps
                            dn_du = (si_u.n - si.n) / eps
                            assert np.allclose(dp_du, si.dp_du, atol=2e-2)
                            assert np.allclose(dn_du, dn[0], atol=2e-2)
                        if si_v.is_valid():
                            dp_dv = (si_v.p - si.p) / eps
                            dn_dv = (si_v.n - si.n) / eps
                            assert np.allclose(dp_dv, si.dp_dv, atol=2e-2)
                            assert np.allclose(dn_dv, dn[1], atol=2e-2)

@pytest.mark.skip()
def test04_parameters_changed():
    disk = example_disk()
    params = traverse(disk)

    ps = disk.sample_position(1.0, np.array([0.7, 0.3]))

    params['disk.frame'].n = np.array([0.5, 0.5, 0.5])

    disk.parameters_changed()

    params['disk.frame'].n = np.array([0.0, 0.0, 1.0])

    disk.parameters_changed()

    ps_new = disk.sample_position(1.0, np.array([0.7, 0.3]))

    assert np.allclose(ps_new.p, ps.p, atol=2e-2)
    assert np.allclose(ps_new.n, ps.n, atol=2e-2)
