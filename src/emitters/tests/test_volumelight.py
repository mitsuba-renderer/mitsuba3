import pytest
import drjit as dr
import mitsuba as mi

from mitsuba.scalar_rgb.test.util import fresolver_append_path


spectrum_dicts = {
    'd65': {
        "type": "d65",
    },
    'regular': {
        "type": "regular",
        "wavelength_min": 500,
        "wavelength_max": 600,
        "values": "1, 2"
    }
}


@fresolver_append_path
def create_emitter_and_spectrum(s_key='d65'):
    emitter = mi.load_dict({
        "type": "obj",
        "filename": "resources/data/tests/obj/cbox_smallbox.obj",
        "emitter" : { "type": "volumelight", "radiance" : spectrum_dicts[s_key] }
    })
    spectrum = mi.load_dict(spectrum_dicts[s_key])
    expanded = spectrum.expand()
    if len(expanded) == 1:
        spectrum = expanded[0]

    return emitter, spectrum


@fresolver_append_path
def create_emitter_rgb():
    r, c = 1.0, mi.ScalarPoint3f([0.0, 0.0, 0.0])
    emitter = mi.load_dict({
        "type": "sphere",
        "radius": r,
        "center": c,
        "emitter" : { "type": "volumelight", "radiance" : {
            "type": "rgb",
            "value": [1.0, 2.0, 1.0]
        } }
    })

    return r, c, emitter


def test01_constructor(variant_scalar_rgb):
    # Check that the shape is properly bound to the emitter
    shape, spectrum = create_emitter_and_spectrum()
    assert shape.emitter().bbox() == shape.bbox()

    # Check that we are not allowed to specify a to_world transform directly in the emitter.
    with pytest.raises(RuntimeError):
        e = mi.load_dict({
            "type" : "volumelight",
            "to_world" : mi.ScalarTransform4f.translate([5, 0, 0])
        })


@pytest.mark.parametrize("spectrum_key", spectrum_dicts.keys())
def test02_eval(variants_vec_spectral, spectrum_key):
    # Check that eval() return the same values as the 'radiance' spectrum

    shape, spectrum = create_emitter_and_spectrum(spectrum_key)
    emitter = shape.emitter()

    it = dr.zeros(mi.SurfaceInteraction3f, 3)
    assert dr.allclose(emitter.eval(it), spectrum.eval(it))

    # Check that eval returns 0.0 when the sample point is outside the shape
    it.p = mi.ScalarPoint3f([0.0, 0.0, 0.0])
    assert dr.allclose(emitter.eval(it), 0.0)


@pytest.mark.parametrize("spectrum_key", spectrum_dicts.keys())
def test03_sample_ray(variants_vec_spectral, spectrum_key):
    # Check the correctness of the sample_ray() method


    shape, spectrum = create_emitter_and_spectrum(spectrum_key)
    emitter = shape.emitter()

    time = 0.5
    wavelength_sample = [0.5, 0.33, 0.1]
    pos_sample = [[0.2, 0.1, 0.2], [0.6, 0.9, 0.2], [0.5]*3]
    dir_sample = [[0.4, 0.5, 0.3], [0.1, 0.4, 0.9]]

    # Sample a ray (position, direction, wavelengths) on the emitter
    ray, res = emitter.sample_ray(time, wavelength_sample, pos_sample, dir_sample)

    # Sample wavelengths on the spectrum
    it = dr.zeros(mi.SurfaceInteraction3f, 3)

    # Sample a position in the shape
    ps = shape.sample_position_volume(time, pos_sample)
    pdf = mi.warp.square_to_uniform_sphere_pdf(mi.warp.square_to_uniform_sphere(dir_sample))
    it.p = ps.p
    it.n = ps.n
    it.time = time

    assert dr.allclose(ray.time, time)
    assert dr.allclose(ray.o, ps.p, atol=2e-2)
    assert dr.allclose(ray.d, mi.Frame3f(ps.n).to_world(mi.warp.square_to_uniform_sphere(dir_sample)))

    it.wavelengths = mi.Float(wavelength_sample) * (mi.MI_CIE_MAX - mi.MI_CIE_MIN) + mi.MI_CIE_MIN
    spec = dr.select(ps.pdf > 0.0, emitter.eval(it) * (mi.MI_CIE_MAX - mi.MI_CIE_MIN) / (ps.pdf * pdf), 0.0)

    assert dr.allclose(res, spec)


@pytest.mark.parametrize("spectrum_key", spectrum_dicts.keys())
def test04_sample_direction(variants_vec_spectral, spectrum_key):
    # Check the correctness of the sample_direction(), pdf_direction(), and eval_direction() methods
    shape, spectrum = create_emitter_and_spectrum(spectrum_key)
    emitter = shape.emitter()

    # Direction sampling is conditioned on a sampled position
    it = dr.zeros(mi.SurfaceInteraction3f, 3)
    it.p = [[0.2, -200.1, 186.0], [0.6, -0.9, 82.5],
            [0.4, 0.9, 168.5]]  # Some positions
    it.time = 1.0

    # Sample direction on the emitter
    samples = [[0.4, 0.5, 0.3], [0.1, 0.24, 0.9], [0.5]*3]

    ds, res = emitter.sample_direction(it, samples)

    # Sample direction on the shape
    shape_ds = dr.zeros(mi.DirectionSample3f)
    shape_ds.p = it.p
    ps = shape.sample_position_volume(it.time, samples)
    shape_ds.d = dr.normalize(ps.p - it.p)
    shape_ds.pdf = shape.pdf_direction_volume(it, ds)

    assert dr.allclose(ds.pdf, shape_ds.pdf)
    assert dr.allclose(ds.pdf, emitter.pdf_direction(it, ds))
    assert dr.allclose(ds.d, shape_ds.d, atol=1e-3)
    assert dr.allclose(ds.time, it.time)

    # Evaluate the spectrum (divide by the pdf)
    spec = dr.select(ds.pdf > 0.0, emitter.eval(it) / ds.pdf, 0.0)
    assert dr.allclose(res, spec)

    assert dr.allclose(emitter.eval_direction(it, ds), spec)


def test05_sample_ray_rgb(variants_vec_rgb):
    # Check the correctness of the sample_ray() method
    _, _, shape = create_emitter_rgb()
    emitter = shape.emitter()

    time = 0.5
    wavelength_sample = [0.5, 0.33, 0.1]
    pos_sample = [[0.2, 0.1, 0.2], [0.6, 0.9, 0.2], [0.5]*3]
    dir_sample = [[0.4, 0.5, 0.3], [0.1, 0.4, 0.9]]

    # Sample a ray (position, direction, wavelengths) on the emitter
    ray, res = emitter.sample_ray(time, wavelength_sample, pos_sample, dir_sample)

    # Sample wavelengths on the spectrum
    it = dr.zeros(mi.SurfaceInteraction3f, 3)

    # Sample a position in the shape
    ps = shape.sample_position_volume(time, pos_sample)
    pdf = mi.warp.square_to_uniform_sphere_pdf(mi.warp.square_to_uniform_sphere(dir_sample))
    it.p = ps.p
    it.n = ps.n
    it.time = time

    assert dr.allclose(ray.time, time)
    assert dr.allclose(ray.o, ps.p, atol=2e-2)
    assert dr.allclose(ray.d, mi.Frame3f(ps.n).to_world(mi.warp.square_to_uniform_sphere(dir_sample)))

    spec = dr.select(ps.pdf > 0.0, emitter.eval(it) / (ps.pdf * pdf), 0.0)

    assert dr.allclose(res, spec)


def test06_sample_direction_rgb(variants_vec_rgb):
    # Check the correctness of the sample_direction(), pdf_direction(), and eval_direction() methods
    radius, center, shape = create_emitter_rgb()
    emitter = shape.emitter()

    # Direction sampling is conditioned on a sampled position
    it = dr.zeros(mi.SurfaceInteraction3f, 3)
    it.p = [[0.0, 0.48, 0.19, 10, -2], [0.0, 0.9018, 2.19, 2, 1.02], [0.0, 0.5, 3.291, 3.1, 9.1]]  # Some positions
    it.time = 1.0

    # Sample direction on the emitter
    samples = mi.Point3f([[0.01, 0.5, 0.0, 0.123, 0.01], [0.20, 0.5, 0.9, 0.21, 0.895], [0.1, 0.5, 0.0, 0.647, 0.25]])

    ds, res = emitter.sample_direction(it, samples)

    # Sample direction on the shape
    shape_ds = dr.zeros(mi.DirectionSample3f)
    shape_ds.p = it.p
    ps = shape.sample_position_volume(it.time, samples)
    shape_ds.d = dr.normalize(ps.p - it.p)
    shape_ds.pdf = shape.pdf_direction_volume(it, ds)

    assert dr.allclose(ds.pdf, shape_ds.pdf)
    assert dr.allclose(ds.pdf, emitter.pdf_direction(it, ds))
    assert dr.allclose(ds.d, shape_ds.d, atol=1e-3)
    assert dr.allclose(ds.time, it.time)

    A = 1
    dist_from_c = dr.norm(it.p - center)
    B = 2*dr.dot(shape_ds.d, it.p - center + dist_from_c*shape_ds.d)
    C = dr.squared_norm(it.p - center + dist_from_c*shape_ds.d) - dr.sqr(radius)

    r0 = 0.5 * (-B - dr.sqrt(dr.sqr(B) - 4*A*C))+dist_from_c
    r1 = 0.5 * (-B + dr.sqrt(dr.sqr(B) - 4*A*C))+dist_from_c
    r0 = dr.clamp(r0, 0.0, dr.inf)
    r1 = dr.clamp(r1, 0.0, dr.inf)

    analytic_pdf = dr.select(
        dr.isfinite(r0) & dr.isfinite(r1),
        (dr.power(r1, 3) - dr.power(r0, 3))/(4*dr.pi*dr.power(radius, 3)),
        0.0
    )

    assert(dr.allclose(shape_ds.pdf, analytic_pdf, atol=1e-3, rtol=1e-2))

    # Evaluate the spectrum (divide by the pdf)
    spec = dr.select(ds.pdf > 0.0, emitter.eval(it) / ds.pdf, 0.0)
    assert dr.allclose(res, spec)

    assert dr.allclose(emitter.eval_direction(it, ds) / ds.pdf, spec)
