import numpy as np
import pytest
from mitsuba.core.xml  import load_string
from mitsuba.core      import Ray3f
from mitsuba.render    import Shape
from mitsuba.core.math import Pi

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

def test03_ray_intersect():
    r = 1.0
    s = example_scene(radius=1.0)
    # grid size
    n = 40
    inv_n = 1.0 / n
    tol = 1e-8

    for x in range(n):
        for y in range(n):
            x_coord = r * (2 * (x * inv_n) - 1)
            y_coord = r * (2 * (y * inv_n) - 1)

            ray = Ray3f(o=[x_coord, y_coord, -5], d=[0, 0, 1], time=0.0,
                        wavelengths=[400, 500, 600, 700])
            its_found = s.ray_test(ray)

            assert its_found == (x_coord * x_coord + y_coord * y_coord <= r) \
                or abs(x_coord * x_coord + y_coord * y_coord - r) <= tol

def test04_ray_intersect_transform():
    r = 1.0
    s = example_scene(radius=1.0,
                      extra = """<transform name="to_world">
                                     <translate x="0.0" y="1.0" z="0.0"/>
                                  </transform>""")
    # grid size
    n = 40
    inv_n = 1.0 / n
    tol = 1e-8

    for x in range(n):
        for y in range(n):
            x_coord = r * (2 * (x * inv_n) - 1)
            y_coord = r * (2 * (y * inv_n) - 1)

            ray = Ray3f(o=[x_coord, y_coord + 1, -5], d=[0, 0, 1], time=0.0,
                        wavelengths=[400, 500, 600, 700])
            its_found = s.ray_test(ray)

            assert its_found == (x_coord * x_coord + y_coord * y_coord <= r) \
                or abs(x_coord*x_coord + y_coord*y_coord - r) <= tol
