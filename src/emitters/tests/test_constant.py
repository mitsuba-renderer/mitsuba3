import numpy as np
import os
import pytest

from mitsuba.core import warp
from mitsuba.core.math import InvFourPi, Epsilon
from mitsuba.core.xml import load_string
from mitsuba.render import PositionSample3f, Interaction3f

def example_emitter(spectrum = "1.0"):
    return load_string("""
        <emitter version="2.0.0" type="constant">
            <spectrum name="radiance" value="{}"/>
        </emitter>
    """.format(spectrum))

@pytest.fixture
def interaction():
    it = Interaction3f()
    it.wavelengths = [400, 500, 600, 750]
    it.p = [-0.5, 0.3, -0.1] # Some position inside the unit sphere
    it.time = 1.0
    return it

def test01_construct():
    e = example_emitter()
    assert e is not None
    # The emitter's bounding box is reduced to a point for practical reasons
    assert np.all(e.bbox().extents() == 0.0)

def test02_sample_ray():
    e = example_emitter()
    ray, weight = e.sample_ray(0.3, 0.4, [0.1, 0.6], [1.0, 0.0])
    assert np.allclose(ray.time, 0.3) and np.allclose(ray.mint, Epsilon)
    # Ray should point towards the scene
    assert np.sum(ray.d > 0) > 0
    assert np.dot(ray.d, ray.o) < 0
    assert np.all(weight > 0)
    # TODO: check unique wavelengths being sampled
    n = len(ray.wavelengths)
    assert n > 0 and len(np.unique(ray.wavelengths))

def test03_sample_direction(interaction):
    e = example_emitter(spectrum="400:3.1,500:6,600:10,700:15")
    ds, spectrum = e.sample_direction(interaction, [0.85, 0.13])

    assert np.allclose(ds.pdf, InvFourPi)
    # Ray points towards the scene's bounding sphere
    assert np.any(np.abs(ds.d) > 0)
    assert np.dot(ds.d, [0, 0, 1]) > 0
    assert np.allclose(e.pdf_direction(interaction, ds), InvFourPi)
    assert np.allclose(ds.time, interaction.time)
