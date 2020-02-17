import mitsuba
import pytest
import enoki as ek
from enoki.dynamic import Float32 as Float


def test01_point_construct(variant_scalar_rgb):
    from mitsuba.core.xml import load_string

    c = load_string("<emitter version='2.0.0' type='point'></emitter>")
    assert c is not None


def test02_point_sample_direction(variant_scalar_rgb):
    from mitsuba.core.xml import load_string
    from mitsuba.core import warp
    from mitsuba.render import Interaction3f

    e = load_string("""<emitter version='2.0.0' type='point'>
            <point name='position' x='10' y='-1' z='2'/>
        </emitter>""")

    # Direction sampling
    it = Interaction3f()
    it.wavelengths = []
    it.p = [0.0, -2.0, 4.5] # Some position
    it.time = 0.3

    sample = [0.1, 0.5]
    local = warp.square_to_cosine_hemisphere(sample)
    d = -it.p + [10, -1, 2]

    (d_rec, res) = e.sample_direction(it, sample)
    assert d_rec.time == it.time
    assert d_rec.pdf == 1.0
    assert d_rec.delta
    assert ek.allclose(d_rec.d, d / ek.norm(d))
    assert ek.all(res > 0)


def test03_point_sample_direction_vec(variant_packet_rgb):
    from mitsuba.render import Interaction3f
    from mitsuba.core.xml import load_string

    e = load_string("""<emitter version='2.0.0' type='point'>
            <point name='position' x='10' y='-1' z='2'/>
        </emitter>""")

    # Direction sampling
    it = Interaction3f()
    it.wavelengths = []
    it.p = [[0.0, 0.0, 0.0], [-2.0, 0.0, -2.0], [4.5, 4.5, 0.0]] # Some positions
    it.time = [0.3, 0.3, 0.3]

    sample = [0.1, 0.5]
    ek.set_slices(sample, 3)
    d = -it.p + [10, -1, 2]

    (d_rec, res) = e.sample_direction(it, sample)
    assert ek.all(d_rec.time == it.time)
    assert ek.all(d_rec.pdf == 1.0)
    assert ek.all(d_rec.delta)
    assert ek.allclose(d_rec.d, d / ek.norm(d))
    assert ek.all_nested(res > 0)
