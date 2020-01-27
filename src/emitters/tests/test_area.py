import mitsuba
import pytest
import enoki as ek
from enoki.dynamic import Float32 as Float

from mitsuba.python.test import variant_scalar, variant_packet, variant_spectral
from mitsuba.python.test.util import fresolver_append_path

@fresolver_append_path
def example_shape(filename = "data/triangle.ply", has_emitter = True):
    from mitsuba.core.xml import load_string

    return load_string("""
        <shape version='2.0.0' type='ply'>
            <string name='filename' value='{filename}'/>
            {emitter}
            <transform name='to_world'>
                <translate x='10' y='-1' z='2'/>
            </transform>
        </shape>
    """.format(filename=filename,
               emitter="<emitter type='area'><spectrum name='radiance' value='1'/></emitter>" if has_emitter else ""))

def test01_area_construct(variant_scalar):
    from mitsuba.core.xml import load_string

    e = load_string("""<emitter version="2.0.0" type="area">
            <spectrum name="radiance" value="1000"/>
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

def test02_area_sample_direction(variant_scalar):
    from mitsuba.core import warp
    from mitsuba.render import Interaction3f

    shape = example_shape()
    e = shape.emitter()
    # Direction sampling is conditioned on a sampled position
    it = Interaction3f()
    it.wavelengths = []
    it.p = [-5, 3, -1] # Some position
    it.time = 1.0

    sample = [0.5, 0.5]
    local = warp.square_to_cosine_hemisphere(sample)

    (d_rec, res) = e.sample_direction(it, sample)
    d = (d_rec.p - it.p) / ek.norm(d_rec.p - it.p)

    assert ek.all(res > 0)
    assert ek.allclose(d_rec.d, d)
    assert d_rec.pdf > 1.0

@fresolver_append_path
def test03_area_sample_ray(variant_spectral):
    from mitsuba.core.xml import load_string
    from mitsuba.render import SurfaceInteraction3f
    from mitsuba.core import warp, Frame3f
    from mitsuba.core import MTS_WAVELENGTH_SAMPLES

    shape = load_string("""
                <shape version='2.0.0' type='ply'>
                    <string name='filename' value='{filename}'/>
                    {emitter}
                    <transform name='to_world'>
                        <translate x='10' y='-1' z='2'/>
                    </transform>
                </shape>
            """.format(filename="data/triangle.ply",
                    emitter="<emitter type='area'><spectrum name='radiance' value='1'/></emitter>"))

    e = shape.emitter()

    radiance = load_string("<spectrum type='d65' version='2.0.0'/>").expand()[0]
    # Shifted wavelength sample
    wav_sample = 0.44 + ek.arange(Float, MTS_WAVELENGTH_SAMPLES) / float(MTS_WAVELENGTH_SAMPLES)
    wav_sample[wav_sample >= 1.0] -= 1.0

    (wavs, wav_weight) = radiance.sample(SurfaceInteraction3f(), wav_sample.numpy())

    (ray, weight) = e.sample_ray(time=0.98, sample1=wav_sample[0],
                                 sample2=[0.4, 0.6], sample3=[0.1, 0.2])

    assert ek.allclose(ray.time, 0.98)
    assert ek.allclose(wavs, ray.wavelengths)
    # Position on the light source
    assert ek.allclose(ray.o, [10, -0.53524196, 2.22540331])
    # Direction pointing out from the light source, in world coordinates.
    warped = warp.square_to_cosine_hemisphere([0.1, 0.2])
    assert ek.allclose(ray.d, Frame3f([-1, 0, 0]).to_world(warped))
    assert ek.allclose(weight, wav_weight * shape.surface_area() * ek.pi)

@fresolver_append_path
def example_shape_vec(filename = "data/triangle.ply", has_emitter = True):
    from mitsuba.core.xml import load_string

    return load_string("""
        <shape version='2.0.0' type='ply'>
            <string name='filename' value='{filename}'/>
            {emitter}
            <transform name='to_world'>
                <translate x='10' y='-1' z='2'/>
            </transform>
        </shape>
    """.format(filename=filename,
               emitter="<emitter type='area'><spectrum name='radiance' value='1'/></emitter>" if has_emitter else ""))

def test04_area_sample_direction_vec(variant_packet):
    from mitsuba.core import Vector2f
    from mitsuba.render import Interaction3f

    shape = example_shape_vec()
    e = shape.emitter()

    # Direction sampling is conditioned on a sampled position
    it = Interaction3f()
    it.wavelengths = []
    it.p = [[-5,  0, -5, -5],
            [ 3,  3,  0,  3],
            [-1, -1, -1,  0]] # Some positions

    it.time = [1.0, 1.0, 1.0, 1.0]

    sample = Vector2f([0.5, 0.5])
    ek.set_slices(sample, 4)

    (d_rec, res) = e.sample_direction(it, sample)

    d = (d_rec.p - it.p) / ek.norm(d_rec.p - it.p)

    assert ek.all_nested(res > 0)
    assert ek.allclose(d_rec.d, d)
    assert ek.all(d_rec.pdf > 1.0)
