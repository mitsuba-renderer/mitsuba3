import numpy as np
import os
import pytest

from mitsuba.core import warp
from mitsuba.core.math import Pi
from mitsuba.core.xml import load_string
from mitsuba.render import EMeasure, PositionSample3f, Interaction3f
from mitsuba.test.util import fresolver_append_path

@fresolver_append_path
def example_shape(filename = "data/triangle.ply", has_emitter = True):
    return load_string(
           "<shape version='2.0.0' type='ply'>"
        + ("    <string name='filename' value='%s'/>" % filename)
        + ("    <emitter type='area'/>" if has_emitter else "")
        +  "    <transform name='to_world'>"
           "        <translate x='10' y='-1' z='2'/>"
           "    </transform>"
           "</shape>")

def test01_area_construct():
    e = load_string("""<emitter version="2.0.0" type="area">
            <spectrum name="radiance" value="1000f"/>
        </emitter>""")
    assert e is not None

    shape = example_shape()
    assert e is not None
    e = shape.emitter()
    ref_shape = example_shape(has_emitter=False)
    assert e.bbox() == ref_shape.bbox()

    with pytest.raises(RuntimeError):
        # Should not allow specifying a to_world transform directly in the emitter.
        e = load_string("""<emitter version="2.0.0" type="area">
                <transform name="to_world">
                    <translate x="5"/>
                </transform>
            </emitter>""")

@pytest.mark.skip("Mesh position sampling is not implemented yet.")
def test02_area_sample_position():
    # TODO: implement this test when Mesh::sample_position is ready.
    shape = example_shape()
    e = shape.emitter()

    p_rec = PositionSample3f()
    p = [10, -1, 2] # Light position
    assert np.all(e.sample_position(p_rec, [0.5, 0.5]) == 4 * Pi)
    assert np.all(p_rec.p == p)
    assert np.all(p_rec.n == [0, 0, 0])
    assert p_rec.pdf == 1
    assert p_rec.measure == EMeasure.EArea

def test03_area_sample_direction():
    shape = example_shape()
    e = shape.emitter()
    # Direction sampling is conditioned on a sampled position
    it = Interaction3f()
    it.wavelengths = [400, 500, 600, 750]
    it.p = [-5, 3, -1] # Some position
    it.time = 1.0

    sample = np.array([0.5, 0.5])
    local = warp.square_to_cosine_hemisphere(sample)

    (d_rec, res) = e.sample_direction(it, sample)
    d = (d_rec.p - it.p) / np.linalg.norm(d_rec.p - it.p)

    assert np.all(res > 0)
    assert np.allclose(d_rec.d, d)
    assert d_rec.pdf > 1.0

@pytest.mark.skip("Mesh position sampling is not implemented yet.")
def test04_area_sample_ray():
    # TODO: implement this test when Mesh::sample_position is ready.
    shape = example_shape()
    e = shape.emitter()
    (ray, weight) = e.sample_ray(position_sample=[0.1, 0.2],
                                 direction_sample=[0.1, 0.5], time_sample=5)
    assert ray.time == 5
    assert np.all(ray.o == p)
    assert np.all(ray.d == warp.square_to_uniform_sphere([0.1, 0.5]))
    assert np.all(weight == 4 * Pi)

# TODO: test vectorized variants
