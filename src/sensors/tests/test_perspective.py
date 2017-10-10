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
    assert c.x_fov() == approx(34)

    assert np.all((c.world_transform(0).matrix - np.array([
        [1, 0, 0,   1],
        [0, 1, 0,   0],
        [0, 0, 1, 1.5],
        [0, 0, 0,   1]
    ])) == approx(0))
    assert np.all((c.world_transform(0).inverse_transpose - np.array([
        [ 1, 0,    0,   0],
        [ 0, 1,    0,   0],
        [ 0, 0,    1,   0],
        [-1, 0, -1.5,   1]
    ])) == approx(0)), c.world_transform(0).inverse_transpose
    assert np.all((c.view_transform(0).matrix - np.array([
        [1, 0, 0,  -1],
        [0, 1, 0,   0],
        [0, 0, 1,-1.5],
        [0, 0, 0,   1]
    ])) == approx(0)), c.view_transform(0).matrix
    assert np.all((c.view_transform(0).inverse_transpose - np.array([
        [1, 0,   0,  0],
        [0, 1,   0,  0],
        [0, 0,   1,  0],
        [1, 0, 1.5,  1]
    ])) == approx(0)), c.view_transform(0).matrix
    assert c.shutter_open_time() == approx(0)
    assert not c.needs_time_sample()
    assert not c.needs_aperture_sample()
    assert c.bbox() == BoundingBox3f([1, 0, 1.5], [1, 0, 1.5])


    # Aspect should be the same as a default film
    # assert c.film() is not None
    # assert c.aspect() == approx(c.film().size()[0] / c.film().size()[1])

def test02_sample_rays():
    # TODO: more precise tests
    c = load_string("<sensor version='2.0.0' type='perspective'></sensor>")
    assert c.sample_ray(position_sample=[0, 0], aperture_sample=[0, 0], time_sample=0) is not None

def test03_sample_time():
    c = load_string("""
        <sensor version='2.0.0' type='perspective'>
            <float name='shutter_open' value='1.5'/>
            <float name='shutter_close' value='5'/>
        </sensor>
    """)
    assert c.shutter_open() == approx(1.5)
    assert c.shutter_open_time() == approx(5 - 1.5)
    assert c.needs_time_sample()

    assert c.sample_time(0.5) == approx(c.shutter_open() + 0.5 * c.shutter_open_time())
    times = np.linspace(0, 1, 10)
    assert np.all(c.sample_time(times) == [
        approx(c.shutter_open() + t * c.shutter_open_time())
        for t in times
    ])
