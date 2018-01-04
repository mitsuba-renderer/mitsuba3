import pytest
import numpy as np

from mitsuba.core import BoundingBox3f
from mitsuba.core.xml import load_string
from pytest import approx

def test01_create():
    c = load_string("<sensor version='2.0.0' type='perspective'></sensor>")
    assert c is not None
    assert approx(c.near_clip(), 0.01)

    c = load_string("""
        <sensor version='2.0.0' type='perspective'>
            <float name='near_clip' value='1'/>
            <float name='far_clip' value='35.4'/>
            <float name='focus_distance' value='15'/>
            <float name='fov' value='34'/>

            <transform name="to_world">
                <translate x="1" y="0" z="1.5"/>
            </transform>
        </sensor>
    """)
    assert c.near_clip() == approx(1)
    assert c.far_clip() == approx(35.4)
    assert c.focus_distance() == approx(15)

    wt = c.world_transform().eval(0)
    vt = wt.inverse()

    assert np.all((wt.matrix - np.array([
        [1, 0, 0,   1],
        [0, 1, 0,   0],
        [0, 0, 1, 1.5],
        [0, 0, 0,   1]
    ])) == approx(0))
    assert np.all((wt.inverse_transpose - np.array([
        [ 1, 0,    0,   0],
        [ 0, 1,    0,   0],
        [ 0, 0,    1,   0],
        [-1, 0, -1.5,   1]
    ])) == approx(0))
    assert np.all((vt.matrix - np.array([
        [1, 0, 0,  -1],
        [0, 1, 0,   0],
        [0, 0, 1,-1.5],
        [0, 0, 0,   1]
    ])) == approx(0))
    assert np.all((vt.inverse_transpose - np.array([
        [1, 0,   0,  0],
        [0, 1,   0,  0],
        [0, 0,   1,  0],
        [1, 0, 1.5,  1]
    ])) == approx(0))
    assert c.shutter_open_time() == approx(0)
    assert not c.needs_aperture_sample()
    assert c.bbox() == BoundingBox3f([1, 0, 1.5], [1, 0, 1.5])

    # Aspect should be the same as a default film
    assert c.film() is not None

def test02_sample_rays():
    c = load_string("<sensor version='2.0.0' type='perspective'></sensor>")
    (ray, spec_weight) = c.sample_ray(0.0, 0.6, [0, 0], [0, 0])
    assert ray is not None
    assert np.all(spec_weight > 0)

def test03_needs_time_sample():
    c = load_string("""
        <sensor version='2.0.0' type='perspective'>
            <float name='shutter_open' value='1.5'/>
            <float name='shutter_close' value='5'/>
        </sensor>
    """)
    assert c.shutter_open() == approx(1.5)
    assert c.shutter_open_time() == approx(5 - 1.5)
