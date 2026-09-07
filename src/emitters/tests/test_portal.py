import pytest
import drjit as dr
import mitsuba as mi
import numpy as np


def portal_transform(translate, axis=None, angle=0, scale=1.0):
    T = mi.ScalarAffineTransform4f
    t = T().translate(translate)
    if axis is not None:
        t = t @ T().rotate(axis, angle)
    return t @ T().scale(scale)


def portal(to_world):
    return {'type': 'portal', 'to_world': to_world}


def env_emitter(emitter_type):
    if emitter_type == 'constant':
        return {'type': 'constant', 'radiance': 1.0}

    # A smooth, non-constant environment map
    v, u = np.meshgrid(np.linspace(0, 1, 16), np.linspace(0, 1, 32), indexing='ij')
    img = np.stack([1 + 0.5 * np.cos(2 * np.pi * u), 1 + v, 2 - v], axis=-1)
    return {'type': 'envmap', 'bitmap': mi.Bitmap(img.astype(np.float32))}


def make_emitter(emitter_type, portals={}, **kwargs):
    return mi.load_dict({
        'type': 'scene',
        'emitter': env_emitter(emitter_type),
        **portals,
        **kwargs
    }).emitters()[0]


# Two portals facing the region around the origin, from +z and from -x
def facing_portals():
    return {
        'portal_0': portal(portal_transform([0, 0, 1], [1, 0, 0], 180)),
        'portal_1': portal(portal_transform([-1.5, 0, 0], [0, 1, 0], 90, scale=0.5)),
    }


def make_interaction(p, n=1):
    it = dr.zeros(mi.Interaction3f, n)
    it.p = mi.Point3f(p)
    return it


def random_samples(n, seed=0):
    return mi.Point2f(np.random.default_rng(seed).random((2, n)).astype(np.float32))


@pytest.mark.parametrize("emitter_type", ['constant', 'envmap'])
def test01_pdf_consistency(variants_vec_backends_once_rgb, emitter_type):
    # pdf_direction() of a sampled direction must match ds.pdf, and the
    # sampling weight must equal radiance / pdf
    emitter = make_emitter(emitter_type, facing_portals(), portal_weight=0.6)
    n = 256
    it = make_interaction([0.1, 0.2, -0.3], n)
    ds, weight = emitter.sample_direction(it, random_samples(n))
    assert dr.allclose(emitter.pdf_direction(it, ds), ds.pdf, rtol=1e-4)
    assert dr.all(ds.pdf > 0)
    assert dr.allclose(weight, emitter.eval_direction(it, ds) / ds.pdf, rtol=1e-4)


@pytest.mark.parametrize("emitter_type", ['constant', 'envmap'])
@pytest.mark.parametrize("portal_weight", [0.5, 0.9])
def test02_unbiased(variants_vec_backends_once_rgb, emitter_type, portal_weight):
    # The irradiance at a point behind a portal must not depend on the sampling strategy
    n = 1 << 18
    it = make_interaction([0.1, -0.2, 0.0], n)
    normal = mi.Vector3f(0, 0, 1)

    def irradiance(emitter, seed):
        ds, weight = emitter.sample_direction(it, random_samples(n, seed))
        cos_theta = dr.maximum(dr.dot(ds.d, normal), 0)
        return dr.mean(dr.mean(weight * cos_theta, axis=None))

    plain = make_emitter(emitter_type)
    with_portals = make_emitter(emitter_type, facing_portals(), portal_weight=portal_weight)

    ref = irradiance(plain, 1)
    if emitter_type == 'constant':
        assert dr.allclose(ref, dr.pi, rtol=0.02)

    for seed in range(2):
        assert dr.allclose(irradiance(with_portals, seed + 2), ref, rtol=0.02)


@pytest.mark.parametrize("emitter_type", ['constant', 'envmap'])
def test03_facing_away(variants_vec_backends_once_rgb, emitter_type):
    # A portal whose front side faces away from the reference point is ignored
    plain = make_emitter(emitter_type)
    with_portal = make_emitter(emitter_type, {'portal': portal(portal_transform([0, 0, 1]))})

    n = 64
    it = make_interaction([0.3, 0.1, 0.2], n)
    ds0, w0 = plain.sample_direction(it, random_samples(n))
    ds1, w1 = with_portal.sample_direction(it, random_samples(n))
    assert dr.allclose(ds0.d, ds1.d)
    assert dr.allclose(ds0.pdf, ds1.pdf)
    assert dr.allclose(w0, w1)
    assert dr.allclose(plain.pdf_direction(it, ds0), with_portal.pdf_direction(it, ds0))

    # The same portal is used once the reference point moves to its front side
    it = make_interaction([0.3, 0.1, 2.0])
    ds = mi.DirectionSample3f()
    ds.d = mi.Vector3f(0, 0, -1)
    assert dr.all(with_portal.pdf_direction(it, ds) > plain.pdf_direction(it, ds))


@pytest.mark.parametrize("emitter_type", ['constant', 'envmap'])
def test04_chi2(variants_vec_backends_once_rgb, emitter_type):
    emitter = make_emitter(emitter_type, facing_portals())

    def sample_func(sample):
        ds, _ = emitter.sample_direction(make_interaction([0, 0, 0]), sample)
        return ds.d

    def pdf_func(wo):
        ds = dr.zeros(mi.DirectionSample3f)
        ds.d = wo
        return emitter.pdf_direction(make_interaction([0, 0, 0]), ds)

    chi2 = mi.chi2.ChiSquareTest(
        domain=mi.chi2.SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=2,
        ires=32
    )

    assert chi2.run()


def test05_reject_skewed(variants_all_rgb):
    # Solid angle sampling needs a rectangle, so sheared portals are rejected
    T = mi.ScalarAffineTransform4f
    skew = T().rotate([0, 0, 1], 30) @ T().scale([2, 1, 1]) @ T().rotate([0, 0, 1], -30)
    with pytest.raises(RuntimeError, match='rectangular'):
        mi.load_dict(portal(skew))

    # Non-uniform scaling and rotations are fine
    mi.load_dict(portal(T().rotate([0, 0, 1], 30) @ T().scale([2, 1, 1])))


def test06_scene_lists(variants_all_rgb):
    # Portals are kept apart from the emitters that integrators sample
    scene = mi.load_dict({
        'type': 'scene',
        'emitter': env_emitter('constant'),
        **facing_portals()
    })
    assert len(scene.emitters()) == 1
    assert len(scene.portals()) == 2
    assert all(p.is_portal() for p in scene.portals())
    assert not scene.emitters()[0].is_portal()

    with pytest.raises(RuntimeError, match='attached to shapes'):
        mi.load_dict({'type': 'rectangle', 'emitter': {'type': 'portal'}})
