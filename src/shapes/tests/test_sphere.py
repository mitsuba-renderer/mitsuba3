import numpy as np

from mitsuba.core.xml  import load_string
from mitsuba.core      import Ray3f
from mitsuba.render    import Shape
from mitsuba.core.math import Pi
from mitsuba.core      import MTS_WAVELENGTH_SAMPLES

def example_sphere(radius = 1.0):
    return load_string("""<shape version='2.0.0' type='sphere'>
        <float name="radius" value="{}"/>
    </shape>""".format(radius))

def example_scene(radius = 1.0, extra = ""):
    return load_string("""<scene version='2.0.0'>
        <shape version='2.0.0' type='sphere'>
            <float name="radius" value="{}"/>
            {}
        </shape>
    </scene>""".format(radius, extra))


def test01_create():
    s = example_sphere()
    assert s is not None
    assert s.primitive_count() == 1
    assert np.allclose(s.surface_area(), 4 * Pi)


def test02_bbox():
    for r in [1, 2, 4]:
        s = example_sphere(r)
        b = s.bbox()

        assert b.valid()
        assert np.allclose(b.center(), [0, 0, 0])
        assert np.all(b.min == -r)
        assert np.all(b.max == r)
        assert np.allclose(b.extents(), 2 * r)


def test04_ray_intersect_transform():
    for r in [1, 3]:
        s = example_scene(radius=r,
                          extra="""<transform name="to_world">
                                       <rotate y="1.0" angle="30"/>
                                       <translate x="0.0" y="1.0" z="0.0"/>
                                   </transform>""")
        # grid size
        n = 21
        inv_n = 1.0 / n

        wl = np.zeros(MTS_WAVELENGTH_SAMPLES)
        for x in range(n):
            for y in range(n):
                x_coord = r * (2 * (x * inv_n) - 1)
                y_coord = r * (2 * (y * inv_n) - 1)

                ray = Ray3f(o=[x_coord, y_coord + 1, -8], d=[0, 0, 1],
                            time=0.0, wavelengths=wl)
                si_found = s.ray_test(ray)

                assert si_found == (x_coord ** 2 + y_coord ** 2 <= r * r) \
                    or np.abs(x_coord ** 2 + y_coord ** 2 - r * r) < 1e-8

                if si_found:
                    ray = Ray3f(o=[x_coord, y_coord + 1, -8], d=[0, 0, 1],
                                time=0.0, wavelengths=wl)
                    si = s.ray_intersect(ray)
                    ray_u = Ray3f(ray)
                    ray_v = Ray3f(ray)
                    eps = 1e-4
                    ray_u.o += si.dp_du * eps
                    ray_v.o += si.dp_dv * eps
                    si_u = s.ray_intersect(ray_u)
                    si_v = s.ray_intersect(ray_v)
                    if si_u.is_valid():
                        du = (si_u.uv - si.uv) / eps
                        assert(np.allclose(du, [1, 0], atol=2e-2))
                    if si_v.is_valid():
                        dv = (si_v.uv - si.uv) / eps
                        assert(np.allclose(dv, [0, 1], atol=2e-2))

