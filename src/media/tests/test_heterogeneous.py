"""
Tests for the two parameterizations of the `heterogeneous` medium.

The scattering coefficient can be given either indirectly, as a
single-scattering albedo, or directly as a `sigma_s` volume. The two are
related by sigma_s = sigma_t * albedo, so they must produce identical
coefficients; what differs is which one carries the gradient, since the
supplied volume is the leaf of the autodiff graph.
"""

import pytest
import drjit as dr
import mitsuba as mi


def make_grid(value):
    import numpy as np
    return mi.VolumeGrid(np.full((4, 4, 4, 1), value, dtype=np.float32))


def make_medium(sigma_t, *, albedo=None, sigma_s=None):
    d = {'type': 'heterogeneous',
         'sigma_t': {'type': 'gridvolume', 'grid': make_grid(sigma_t)}}
    if albedo is not None:
        d['albedo'] = {'type': 'gridvolume', 'grid': make_grid(albedo)}
    if sigma_s is not None:
        d['sigma_s'] = {'type': 'gridvolume', 'grid': make_grid(sigma_s)}
    return mi.load_dict(d)


def interaction(medium):
    mei = dr.zeros(mi.MediumInteraction3f)
    mei.p = mi.Point3f(0.5, 0.5, 0.5)
    mei.wi = mi.Vector3f(0, 0, 1)
    mei.sh_frame = mi.Frame3f(mei.wi)
    mei.wavelengths = mi.Color0f()
    mei.medium = mi.MediumPtr(medium)
    return mei


def test01_parameterizations_agree(variants_vec_backends_once_rgb):
    """sigma_s given directly must match the equivalent albedo."""
    sigma_t, albedo = 0.7, 0.4
    by_albedo = make_medium(sigma_t, albedo=albedo)
    by_sigmas = make_medium(sigma_t, sigma_s=sigma_t * albedo)

    for m in (by_albedo, by_sigmas):
        s, n, t = m.get_scattering_coefficients(interaction(m))
        if m is by_albedo:
            ref = (s, n, t)
        else:
            assert dr.allclose(s, ref[0]), (s, ref[0])
            assert dr.allclose(n, ref[1]), (n, ref[1])
            assert dr.allclose(t, ref[2]), (t, ref[2])


def test02_mutually_exclusive(variants_vec_backends_once_rgb):
    """The two describe the same quantity, so giving both is an error."""
    with pytest.raises(RuntimeError, match='mutually exclusive'):
        make_medium(0.7, albedo=0.4, sigma_s=0.28)


def test03_get_albedo_under_both(variants_vec_backends_once_rgb):
    """get_albedo() answers regardless of how the medium was specified."""
    sigma_t, albedo = 0.7, 0.4
    for m in (make_medium(sigma_t, albedo=albedo),
              make_medium(sigma_t, sigma_s=sigma_t * albedo)):
        assert dr.allclose(m.get_albedo(interaction(m)), albedo, atol=1e-6)


def test04_exposes_the_supplied_parameter(variants_vec_backends_once_rgb):
    """Whichever volume was given is the one the scene exposes."""
    by_albedo = mi.traverse(make_medium(0.7, albedo=0.4))
    by_sigmas = mi.traverse(make_medium(0.7, sigma_s=0.28))
    assert any('albedo' in k for k in by_albedo.keys())
    assert not any('sigma_s' in k for k in by_albedo.keys())
    assert any('sigma_s' in k for k in by_sigmas.keys())
    assert not any('albedo' in k for k in by_sigmas.keys())


def test05_gradient_reaches_the_supplied_volume(variants_all_ad_rgb):
    """The supplied volume is the leaf, so a gradient must arrive there."""
    m = make_medium(0.7, sigma_s=0.28)
    params = mi.traverse(m)
    key = next(k for k in params.keys() if 'sigma_s' in k and 'data' in k)
    dr.enable_grad(params[key])
    params.update()

    s, _, _ = m.get_scattering_coefficients(interaction(m))
    dr.backward(dr.sum(s, axis=None))
    assert dr.any(dr.grad(params[key]) != 0.0)
