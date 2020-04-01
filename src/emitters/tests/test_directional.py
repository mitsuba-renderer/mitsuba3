import mitsuba
import pytest
import enoki as ek


xml_spectrum = {
    "d65": """
        <spectrum version="2.0.0" name="irradiance" type="d65"/>
        """,
    "regular": """
        <spectrum version="2.0.0" name="irradiance" type="regular">
           <float name="lambda_min" value="500"/>
           <float name="lambda_max" value="600"/>
           <string name="values" value="1, 2"/>
        </spectrum>
        """,
}

xml_spectrum_keys = list(set(xml_spectrum.keys()) - {"null"})


def make_spectrum(spectrum_key="d65"):
    from mitsuba.core.xml import load_string

    spectrum = load_string(xml_spectrum[spectrum_key])
    expanded = spectrum.expand()

    if len(expanded) == 1:
        spectrum = expanded[0]

    return spectrum


def make_emitter(direction=None, spectrum_key="d65"):
    from mitsuba.core.xml import load_string

    if direction is None:
        xml_direction = ""
    else:
        if type(direction) is not str:
            direction = ",".join([str(x) for x in direction])
        xml_direction = \
            """<vector name="direction" value="{}"/>""".format(direction)

    return load_string("""
        <emitter version="2.0.0" type="directional">
            {d}
            {s}
        </emitter>
    """.format(d=xml_direction, s=xml_spectrum[spectrum_key]))


def test_construct(variant_scalar_rgb):
    # Test if the emitter can be constructed as intended
    emitter = make_emitter()
    assert not emitter.bbox().valid()  # Degenerate bounding box
    assert ek.allclose(
        emitter.world_transform().eval(0.).matrix,
        [[1, 0, 0, 0],
         [0, 1, 0, 0],
         [0, 0, 1, 0],
         [0, 0, 0, 1]]
    )  # Identity transform matrix by default

    # Check transform setup correctness
    emitter = make_emitter(direction=[0, 0, -1])
    assert ek.allclose(
        emitter.world_transform().eval(0.).matrix,
        [[0, 1, 0, 0],
         [1, 0, 0, 0],
         [0, 0, -1, 0],
         [0, 0, 0, 1]]
    )


@pytest.mark.parametrize("spectrum_key", xml_spectrum_keys)
def test_eval(variant_scalar_spectral, spectrum_key):
    # Check correctness of the eval() method
    from mitsuba.core import Vector3f
    from mitsuba.render import SurfaceInteraction3f

    direction = Vector3f([0, 0, -1])
    emitter = make_emitter(direction, spectrum_key)
    spectrum = make_spectrum(spectrum_key)

    # Incident direction in the illuminated direction
    wi = [0, 0, 1]
    it = SurfaceInteraction3f()
    it.p = [0, 0, 0]
    it.wi = wi
    assert ek.allclose(emitter.eval(it), 0.)

    # Incident direction off the illuminated direction
    wi = [0, 0, 1.1]
    it = SurfaceInteraction3f()
    it.p = [0, 0, 0]
    it.wi = wi
    assert ek.allclose(emitter.eval(it), 0.)


@pytest.mark.parametrize("spectrum_key", xml_spectrum_keys)
@pytest.mark.parametrize("direction", [[0, 0, -1], [1, 1, 1], [0, 0, 1]])
def test_sample_direction(variant_scalar_spectral, spectrum_key, direction):
    # Check correctness of sample_direction() and pdf_direction() methods

    from mitsuba.render import SurfaceInteraction3f
    from mitsuba.core import Vector3f

    direction = Vector3f(direction)
    emitter = make_emitter(direction, spectrum_key)
    spectrum = make_spectrum(spectrum_key)

    it = SurfaceInteraction3f.zero()
    # Some position inside the unit sphere (i.e. within the emitter's default bounding sphere)
    it.p = [-0.5, 0.3, -0.1]
    it.time = 1.0

    # Sample direction
    samples = [0.85, 0.13]
    ds, res = emitter.sample_direction(it, samples)

    # Direction should point *towards* the illuminated direction
    assert ek.allclose(ds.d, -direction / ek.norm(direction))
    assert ek.allclose(ds.pdf, 1.)
    assert ek.allclose(emitter.pdf_direction(it, ds), 0.)
    assert ek.allclose(ds.time, it.time)

    # Check spectrum (no attenuation vs distance)
    spec = spectrum.eval(it)
    assert ek.allclose(res, spec)


@pytest.mark.parametrize("spatial_sample", [[0.85, 0.13], [0.16, 0.50], [0.00, 1.00]])
@pytest.mark.parametrize("direction", [[0, 0, -1], [1, 1, 1], [0, 0, 1]])
def test_sample_ray(variant_scalar_rgb, spatial_sample, direction):
    import enoki as ek
    from mitsuba.core import Vector2f, Vector3f

    emitter = make_emitter(direction=direction)
    direction = Vector3f(direction)
    
    time = 1.0
    wavelength_sample = 0.3
    directional_sample = [0.3, 0.2]

    ray, wavelength = emitter.sample_ray(
        time, wavelength_sample, spatial_sample, directional_sample)

    # Check that ray direction is what is expected
    assert ek.allclose(ray.d, direction / ek.norm(direction))

    # Check that ray origin is outside of bounding sphere
    # Bounding sphere is centered at world origin and has radius 1 without scene
    assert ek.norm(ray.o) >= 1.
