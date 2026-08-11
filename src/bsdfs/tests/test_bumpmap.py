import numpy as np
import drjit as dr
import mitsuba as mi


def test01_flat_texture(variants_vec_backends_once_rgb):
    """A bump map whose texture has no gradient leaves the nested BSDF
    untouched, including the orientation of its anisotropy. The surface is
    parameterized so that ``dp_du`` points along the world Z axis, which the
    perturbed shading frame must not depend on."""
    nested = mi.load_dict({'type': 'roughconductor', 'alpha_u': 0.4,
                           'alpha_v': 0.1, 'material': 'Al'})
    bumpmap = mi.load_dict({
        'type': 'bumpmap',
        'texture': {'type': 'bitmap', 'raw': True,
                    'bitmap': mi.Bitmap(np.full((4, 4), 0.5, np.float32))},
        'nested_bsdf': nested
    })

    si = dr.zeros(mi.SurfaceInteraction3f)
    si.n = mi.Normal3f(0, 1, 0)
    si.sh_frame = mi.Frame3f(si.n)
    si.dp_du = mi.Vector3f(0, 0, 1)
    si.dp_dv = mi.Vector3f(1, 0, 0)
    si.wi = mi.Vector3f(0, 0, 1)

    ctx = mi.BSDFContext()
    wo = dr.normalize(mi.Vector3f([0.3, -0.5], [0.2, 0.1], [0.9, 0.7]))

    dr.assert_allclose(bumpmap.eval(ctx, si, wo), nested.eval(ctx, si, wo))
    dr.assert_allclose(bumpmap.pdf(ctx, si, wo), nested.pdf(ctx, si, wo))


def test02_shading_frame_is_world_space(variants_vec_backends_once_rgb):
    """The ``aov`` integrator writes ``BSDF::sh_frame()`` out as a shading
    normal, which has to be in world coordinates."""
    to_world = mi.ScalarTransform4f().rotate([1, 0, 0], 90)
    scene = mi.load_dict({
        'type': 'scene',
        'plate': {
            'type': 'rectangle',
            'to_world': to_world,
            'bsdf': {
                'type': 'bumpmap',
                'texture': {'type': 'bitmap', 'raw': True,
                            'bitmap': mi.Bitmap(np.full((4, 4), 0.5, np.float32))},
                'nested_bsdf': {'type': 'diffuse'}
            }
        }
    })
    sensor = mi.load_dict({
        'type': 'perspective',
        'to_world': mi.ScalarTransform4f().look_at(origin=[0, -2, 0],
                                                   target=[0, 0, 0], up=[0, 0, 1]),
        'film': {'type': 'hdrfilm', 'width': 4, 'height': 4,
                 'pixel_format': 'rgb', 'rfilter': {'type': 'box'}},
        'sampler': {'type': 'independent', 'sample_count': 1}
    })
    image = mi.load_dict({'type': 'aov', 'aovs': 'nn:sh_normal',
                          'img': {'type': 'path'}}).render(scene, sensor=sensor)

    expected = np.array(to_world @ mi.ScalarNormal3f(0, 0, 1)).ravel()
    assert np.allclose(np.array(image)[..., 3:6], expected, atol=1e-5)


def test03_sample_consistency(variants_vec_backends_once_rgb):
    """Sampling and evaluation observe the same perturbed frame: the density
    that ``sample()`` reports agrees with ``pdf()`` at the sampled direction,
    and both vanish where the perturbation pushed that direction below the
    surface."""
    rng = mi.PCG32(size=4096)
    texture = np.float32(np.arange(64).reshape(8, 8) % 5) / 4
    bumpmap = mi.load_dict({
        'type': 'bumpmap', 'scale': 4,
        'texture': {'type': 'bitmap', 'raw': True,
                    'bitmap': mi.Bitmap(texture)},
        'nested_bsdf': {'type': 'roughconductor', 'alpha': 0.2,
                        'material': 'Al'}
    })

    si = dr.zeros(mi.SurfaceInteraction3f, 4096)
    si.n = mi.Normal3f(0, 0, 1)
    si.sh_frame = mi.Frame3f(si.n)
    si.dp_du, si.dp_dv = mi.Vector3f(1, 0, 0), mi.Vector3f(0, 1, 0)
    si.uv = mi.Point2f(rng.next_float32(), rng.next_float32())
    si.wi = dr.normalize(mi.Vector3f(rng.next_float32() - 0.5,
                                     rng.next_float32() - 0.5,
                                     rng.next_float32() * 0.8 + 0.2))

    ctx = mi.BSDFContext()
    bs, weight = bumpmap.sample(ctx, si, rng.next_float32(),
                                mi.Point2f(rng.next_float32(),
                                           rng.next_float32()))
    valid = dr.any(weight != 0)
    assert dr.count(valid) > 0

    # Rejected samples carry no density, valid ones agree with pdf()
    assert dr.all(valid | (bs.pdf == 0))
    pdf = bumpmap.pdf(ctx, si, bs.wo)
    dr.assert_allclose(dr.select(valid, pdf, 0), dr.select(valid, bs.pdf, 0),
                       rtol=1e-4)

    value, pdf_2 = bumpmap.eval_pdf(ctx, si, bs.wo)
    dr.assert_allclose(value, bumpmap.eval(ctx, si, bs.wo))
    dr.assert_allclose(pdf_2, pdf)
