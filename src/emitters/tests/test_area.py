import mitsuba
import pytest
import enoki as ek
from enoki.dynamic import Float32 as Float

from mitsuba.python.test.util import fresolver_append_path


spectrum_strings = {
    'd65':
    """<spectrum version='2.0.0' name="radiance" type='d65'/>""",
    'regular':
    """<spectrum version='2.0.0' name="radiance" type='regular'>
           <float name="lambda_min" value="500"/>
           <float name="lambda_max" value="600"/>
           <string name="values" value="1, 2"/>
       </spectrum>""",
}


@fresolver_append_path
def create_emitter_and_spectrum(s_key='d65'):
    from mitsuba.core.xml import load_string
    emitter = load_string("""<shape version='2.0.0' type='ply'>
                                 <string name='filename' value='data/triangle.ply'/>
                                 <emitter type='area'>
                                     {s}
                                 </emitter>
                                 <transform name='to_world'>
                                     <translate x='10' y='-1' z='2'/>
                                 </transform>
                             </shape>""".format(s=spectrum_strings[s_key]))
    spectrum = load_string(spectrum_strings[s_key])
    expanded = spectrum.expand()
    if len(expanded) == 1:
        spectrum = expanded[0]

    return emitter, spectrum


def test01_constructor(variant_scalar_rgb):
    # Check that the shape is properly bound to the emitter

    shape, spectrum = create_emitter_and_spectrum()
    assert shape.emitter().bbox() == shape.bbox()

    # Check that we are not allowed to specify a to_world transform directly in the emitter.

    from mitsuba.core.xml import load_string
    with pytest.raises(RuntimeError):
        e = load_string("""<emitter version="2.0.0" type="area">
                               <transform name="to_world">
                                   <translate x="5"/>
                               </transform>
                           </emitter>""")


@pytest.mark.parametrize("spectrum_key", spectrum_strings.keys())
def test02_eval(variant_packet_spectral, spectrum_key):
    # Check that eval() return the same values as the 'radiance' spectrum

    from mitsuba.render import SurfaceInteraction3f

    shape, spectrum = create_emitter_and_spectrum(spectrum_key)
    emitter = shape.emitter()

    it = SurfaceInteraction3f.zero(3)
    assert ek.allclose(emitter.eval(it), spectrum.eval(it))

    # Check that eval return 0.0 when direction points inside the shape

    it.wi = ek.normalize([0.2, 0.2, -0.5])
    assert ek.allclose(emitter.eval(it), 0.0)


@pytest.mark.parametrize("spectrum_key", spectrum_strings.keys())
def test03_sample_ray(variant_packet_spectral, spectrum_key):
    # Check the correctness of the sample_ray() method

    from mitsuba.core import warp, Frame3f, sample_shifted
    from mitsuba.render import SurfaceInteraction3f

    shape, spectrum = create_emitter_and_spectrum(spectrum_key)
    emitter = shape.emitter()

    time = 0.5
    wavelength_sample = [0.5, 0.33, 0.1]
    pos_sample = [[0.2, 0.1, 0.2], [0.6, 0.9, 0.2]]
    dir_sample = [[0.4, 0.5, 0.3], [0.1, 0.4, 0.9]]

    # Sample a ray (position, direction, wavelengths) on the emitter
    ray, res = emitter.sample_ray(
        time, wavelength_sample, pos_sample, dir_sample)

    # Sample wavelengths on the spectrum
    it = SurfaceInteraction3f.zero(3)
    wav, spec = spectrum.sample(it, sample_shifted(wavelength_sample))

    # Sample a position on the shape
    ps = shape.sample_position(time, pos_sample)

    assert ek.allclose(res, spec * shape.surface_area() * ek.pi)
    assert ek.allclose(ray.time, time)
    assert ek.allclose(ray.wavelengths, wav)
    assert ek.allclose(ray.o, ps.p)
    assert ek.allclose(
        ray.d, Frame3f(ps.n).to_world(warp.square_to_cosine_hemisphere(dir_sample)))


@pytest.mark.parametrize("spectrum_key", spectrum_strings.keys())
def test04_sample_direction(variant_packet_spectral, spectrum_key):
    # Check the correctness of the sample_direction() and pdf_direction() methods

    from mitsuba.render import SurfaceInteraction3f

    shape, spectrum = create_emitter_and_spectrum(spectrum_key)
    emitter = shape.emitter()

    # Direction sampling is conditioned on a sampled position
    it = SurfaceInteraction3f.zero(3)
    it.p = [[0.2, 0.1, 0.2], [0.6, -0.9, 0.2],
            [0.4, 0.9, -0.2]]  # Some positions
    it.time = 1.0

    # Sample direction on the emitter
    samples = [[0.4, 0.5, 0.3], [0.1, 0.4, 0.9]]
    ds, res = emitter.sample_direction(it, samples)

    # Sample direction on the shape
    shape_ds = shape.sample_direction(it, samples)

    assert ek.allclose(ds.pdf, shape_ds.pdf)
    assert ek.allclose(ds.pdf, emitter.pdf_direction(it, ds))
    assert ek.allclose(ds.d, shape_ds.d)
    assert ek.allclose(ds.time, it.time)

    # Evalutate the spectrum (divide by the pdf)
    spec = spectrum.eval(it) / ds.pdf
    assert ek.allclose(res, spec)
