import numpy as np
import pytest
from mitsuba.core.xml  import load_string
from mitsuba.core      import Ray3f
from mitsuba.render    import Shape
from mitsuba.core.math import Pi

def test01_create():
    s = load_string("<shape version='2.0.0' type='sphere'></shape>")
    assert s is not None
    assert s.primitive_count() == 1
    assert np.allclose(s.surface_area(), 4 * Pi)

def test02_bbox():
    s = load_string("<shape version='2.0.0' type='sphere'></shape>")
    b = s.bbox()

    assert b.valid()
    assert np.allclose(b.center(), [0, 0, 0])
    assert b.volume() == 8.0

def test03_ray_intersect():
    s = load_string("<shape version='2.0.0' type='sphere'></shape>")
    # sphere radius
    r = 1.0
    # grid size
    N = 40
    invN = 1.0 / N
    tol = 1e-8

    for x in range(N):
        for y in range(N):

            x_coord = r*(2*(x*invN)-1)
            y_coord = r*(2*(y*invN)-1)

            ray = Ray3f([x_coord, y_coord, -5], [0, 0, 1])
            (its_found, its_t) = s.ray_intersect(ray, 0, 1000)

            assert((its_found == (x_coord*x_coord + y_coord*y_coord <= r))
                   or (abs(x_coord*x_coord + y_coord*y_coord - r) <= tol))

def test04_ray_intersect_transform():
    s = load_string("<shape version='2.0.0' type='sphere'>"
                    "   <transform name='to_world'>"
                    "       <translate x='0.0' y='1.0' z='0.0'/>"
                    "   </transform>"
                    "</shape>")
    # sphere radius
    r = 1.0
    # grid size
    N = 40
    invN = 1.0 / N
    tol = 1e-8

    for x in range(N):
        for y in range(N):

            x_coord = r*(2*(x*invN)-1)
            y_coord = r*(2*(y*invN)-1)

            ray = Ray3f([x_coord, y_coord+1, -5], [0, 0, 1])
            (its_found, its_t) = s.ray_intersect(ray, 0, 1000)

            assert((its_found == (x_coord*x_coord + y_coord*y_coord <= r))
                   or (abs(x_coord*x_coord + y_coord*y_coord - r) <= tol))
