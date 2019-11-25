import numpy as np
import pytest

from mitsuba.scalar_rgb.core.math import Pi
from mitsuba.scalar_rgb.core.xml import load_string
from mitsuba.scalar_rgb.core import warp
from mitsuba.scalar_rgb.render import Interaction3f

def test01_point_construct():
    c = load_string("<emitter version='2.0.0' type='point'></emitter>")
    assert c is not None

def test02_point_sample_direction():
    e = load_string("""<emitter version='2.0.0' type='point'>
            <point name='position' x='10' y='-1' z='2'/>
        </emitter>""")

    # Direction sampling
    it = Interaction3f()
    it.wavelengths = [1, 1, 1]
    it.p = [0.0, -2.0, 4.5] # Some position
    it.time = 0.3

    sample = np.array([0.1, 0.5])
    local = warp.square_to_cosine_hemisphere(sample)
    d = np.array([10, -1, 2]) - it.p

    (d_rec, res) = e.sample_direction(it, sample)
    assert d_rec.time == it.time
    assert d_rec.pdf == 1.0
    assert d_rec.delta
    assert np.allclose(d_rec.d, d / np.linalg.norm(d))
    assert np.all(res > 0)

def test03_point_sample_direction_vec():
    try:
        from mitsuba.packet_rgb.render import Interaction3f as Interaction3fX
        from mitsuba.packet_rgb.core.xml import load_string as load_string_packet
    except ImportError:
        pytest.skip("packet_rgb mode not enabled")
    e = load_string_packet("""<emitter version='2.0.0' type='point'>
            <point name='position' x='10' y='-1' z='2'/>
        </emitter>""")

    # Direction sampling
    it = Interaction3fX()
    it.wavelengths = [[1, 1, 1], [1, 1, 1], [1, 1, 1]]
    it.p = [[0.0, -2.0, 4.5], [0.0, 0.0, 4.5], [0.0, -2.0, 0.0]] # Some positions
    it.time = [0.3, 0.3, 0.3]

    sample = np.array([[0.1, 0.5], [0.1, 0.5], [0.1, 0.5]])
    d = np.array([10, -1, 2]) - it.p

    (d_rec, res) = e.sample_direction(it, sample)
    assert np.all(d_rec.time == it.time)
    assert np.all(d_rec.pdf == 1.0)
    assert np.all(d_rec.delta)
    assert np.allclose(d_rec.d, [d[i] / np.linalg.norm(d[i]) for i in range(len(it.time))])
    assert np.all(res > 0)
