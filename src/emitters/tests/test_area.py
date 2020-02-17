import mitsuba
import pytest
import enoki as ek
from enoki.dynamic import Float32 as Float

from mitsuba.python.test.util import fresolver_append_path


@fresolver_append_path
def create_emitter_and_spectrum():
    from mitsuba.core.xml import load_string
    emitter = load_string("""<shape version='2.0.0' type='ply'>
                                 <string name='filename' value='data/triangle.ply'/>
                                 <emitter type='area'>
                                     <spectrum name='radiance' value='1'/>
                                 </emitter>
                                 <transform name='to_world'>
                                     <translate x='10' y='-1' z='2'/>
                                 </transform>
                             </shape>""")
    spectrum = load_string(
        "<spectrum version='2.0.0' type='d65'/>").expand()[0]
    return emitter, spectrum


def test01_area_construct(variant_scalar_rgb):
    from mitsuba.core.xml import load_string

    # Check that the shape is properly bound to the emitter
    shape, spectrum = create_emitter_and_spectrum()
    assert shape.emitter().bbox() == shape.bbox()

    # Should not allow specifying a to_world transform directly in the emitter.
    with pytest.raises(RuntimeError):
        e = load_string("""<emitter version="2.0.0" type="area">
                               <transform name="to_world">
                                   <translate x="5"/>
                               </transform>
                           </emitter>""")

# TODO rewrite this test
def test02_area_sample_direction(variant_scalar_rgb):
    from mitsuba.core import warp
    from mitsuba.render import Interaction3f

    shape, spectrum = create_emitter_and_spectrum()
    emitter = shape.emitter()
    # Direction sampling is conditioned on a sampled position
    it = Interaction3f.zero()
    it.p = [-5, 3, -1]  # Some position
    it.time = 1.0

    sample = [0.5, 0.5]
    local = warp.square_to_cosine_hemisphere(sample)

    (d_rec, res) = emitter.sample_direction(it, sample)
    d = (d_rec.p - it.p) / ek.norm(d_rec.p - it.p)

    assert ek.all(res > 0)
    assert ek.allclose(d_rec.d, d)
    assert d_rec.pdf > 1.0


# TODO rewrite this test
def test03_area_sample_ray(variant_scalar_spectral):
    from mitsuba.render import SurfaceInteraction3f
    from mitsuba.core import Frame3f, warp, sample_shifted
    from mitsuba.core import MTS_WAVELENGTH_SAMPLES

    shape, spectrum = create_emitter_and_spectrum()
    emitter = shape.emitter()

    wav_sample = 0.44
    it = SurfaceInteraction3f.zero()

    wavs, wav_weight = spectrum.sample(it, sample_shifted(wav_sample))

    (ray, weight) = emitter.sample_ray(time=0.98, sample1=wav_sample,
                                       sample2=[0.4, 0.6], sample3=[0.1, 0.2])

    assert ek.allclose(ray.time, 0.98)
    assert ek.allclose(wavs, ray.wavelengths)
    # Position on the light source
    assert ek.allclose(ray.o, [10, -0.53524196, 2.22540331])
    # Direction pointing out from the light source, in world coordinates.
    warped = warp.square_to_cosine_hemisphere([0.1, 0.2])
    assert ek.allclose(ray.d, Frame3f([-1, 0, 0]).to_world(warped))
    assert ek.allclose(weight, wav_weight * shape.surface_area() * ek.pi)


# TODO rewrite this test
def test04_area_sample_direction_vec(variant_packet_rgb):
    from mitsuba.render import SurfaceInteraction3f

    shape, spectrum = create_emitter_and_spectrum()
    emitter = shape.emitter()

    # Direction sampling is conditioned on a sampled position
    it = SurfaceInteraction3f.zero()
    it.p = [[-5, 0, -5, -5],
            [3, 3, 0, 3],
            [-1, -1, -1, 0]]  # Some positions

    it.time = [1.0, 1.0, 1.0, 1.0]

    sample = [0.5, 0.5]

    (d_rec, res) = emitter.sample_direction(it, sample)

    d = (d_rec.p - it.p) / ek.norm(d_rec.p - it.p)

    assert ek.all_nested(res > 0)
    assert ek.allclose(d_rec.d, d)
    assert ek.all(d_rec.pdf > 1.0)
