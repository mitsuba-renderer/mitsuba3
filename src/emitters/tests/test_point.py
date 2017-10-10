import numpy as np
import pytest

from mitsuba.core.math import Pi
from mitsuba.core.xml import load_string
from mitsuba.core import warp
from mitsuba.render import EMeasure
from mitsuba.render import PositionSample3f, DirectionSample3f

def test01_point_construct():
    c = load_string("<emitter version='2.0.0' type='point'></emitter>")
    assert c is not None
    assert not c.is_environment_emitter()
    # TODO: check flag-style emitters bindings
    assert c.type() == c.EFlags.EDeltaPosition


def test02_point_sample():
    p_rec = PositionSample3f()
    p = [10, -1, 2] # Light position
    c = load_string("""<emitter version='2.0.0' type='point'>
            <point name='position' x='10' y='-1' z='2'/>
        </emitter>""")

    # Position sampling
    assert np.all(c.sample_position(p_rec, [0.5, 0.5]) == 4 * Pi)
    assert np.all(p_rec.p == p)
    assert np.all(p_rec.n == [0, 0, 0])
    assert p_rec.pdf == 1
    assert p_rec.measure == EMeasure.EDiscrete

    # Ray sampling
    (ray, weight) = c.sample_ray(position_sample=[0.1, 0.2],
                                 direction_sample=[0.1, 0.5], time_sample=5)
    assert ray.time == 5
    assert np.all(ray.o == p)
    assert np.all(ray.d == warp.square_to_uniform_sphere([0.1, 0.5]))
    assert np.all(weight == 4 * Pi)

    # TODO: test vectorized variants
