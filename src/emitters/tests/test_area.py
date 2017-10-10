import numpy as np
import os
import pytest

from mitsuba.core import Thread, FileResolver, Struct, float_dtype
from mitsuba.core import warp, Frame3f
from mitsuba.core.math import Pi
from mitsuba.core.xml import load_string
from mitsuba.render import EMeasure
from mitsuba.render import PositionSample3f, DirectionSample3f

# TODO: refactor to common test utils
def fresolver_append_path(func):
    def f(*args, **kwargs):
        # New file resolver
        thread = Thread.thread()
        fres_old = thread.file_resolver()
        fres = FileResolver(fres_old)
        fres.append(os.path.dirname(os.path.realpath(__file__)))
        thread.set_file_resolver(fres)
        # Run actual function
        res = func(*args, **kwargs)
        # Restore previous file resolver
        thread.set_file_resolver(fres_old)

        return res
    return f

@fresolver_append_path
def example_shape(filename = "data/triangle.ply", has_emitter = True):
    return load_string(
           "<shape version='2.0.0' type='ply'>"
        + ("    <string name='filename' value='%s'/>" % filename)
        + ("    <emitter type='area'/>" if has_emitter else "")
        +  "    <transform name='to_world'>"
        +  "        <translate x='10' y='-1' z='2'/>"
        +  "    </transform>"
        +  "</shape>")

def test01_area_construct():
    e = load_string("""<emitter version="2.0.0" type="area">
            <spectrum name="radiance" value="4"/>
        </emitter>""")
    assert e is not None
    assert not e.is_environment_emitter()
    assert not e.is_compound()

    shape = example_shape()
    assert e is not None
    e = shape.emitter()
    assert not e.is_environment_emitter()
    assert not e.is_compound()
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
    p_rec = PositionSample3f()
    p_rec.n = [0, 1, 0]

    d_rec = DirectionSample3f()
    p = [10, -1, 2] # Light position
    sample = [0.5, 0.5]
    local = warp.square_to_cosine_hemisphere(sample)
    assert np.all(e.sample_direction(d_rec, p_rec, sample) == 1)
    assert np.all(d_rec.d == Frame3f(p_rec.n).to_world(local))
    assert np.all(d_rec.pdf == warp.square_to_cosine_hemisphere_pdf(local))
    assert d_rec.measure == EMeasure.ESolidAngle

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
