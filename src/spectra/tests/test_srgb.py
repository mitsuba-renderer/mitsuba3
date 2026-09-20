import pytest
import drjit as dr
import mitsuba as mi
import numpy as np


def spectral_si():
    si = dr.zeros(mi.SurfaceInteraction3f)
    si.wavelengths = type(si.wavelengths)(450., 550., 600., 650.)
    return si


@pytest.mark.parametrize('plugin', ['srgb', 'd65'])
def test01_color_update(variants_all_spectral, plugin):
    """The sRGB color is the authoritative representation of these spectra"""
    si = spectral_si()
    tex = mi.load_dict({'type': plugin, 'color': [0.2, 0.5, 0.8]})

    params = mi.traverse(tex)
    dr.assert_allclose(params['value'], [0.2, 0.5, 0.8])

    params['value'] = mi.Color3f(0.8, 0.5, 0.2)
    params.update()

    # Updating the color re-derives the upsampling coefficients
    ref = mi.load_dict({'type': plugin, 'color': [0.8, 0.5, 0.2]})
    dr.assert_allclose(tex.eval(si), ref.eval(si), atol=1e-5)

    if plugin == 'srgb':
        dr.assert_allclose(tex.eval_3(si), [0.8, 0.5, 0.2])


@pytest.mark.parametrize('plugin', ['srgb', 'd65'])
def test02_forward_ad(variants_all_ad_spectral, plugin):
    """Gradients reach the sRGB color through the spectral upsampling model"""
    si = spectral_si()

    def eval_green(green):
        return mi.load_dict({'type': plugin,
                             'color': [0.2, green, 0.8]}).eval(si)

    tex = mi.load_dict({'type': plugin, 'color': [0.2, 0.5, 0.8]})
    params = mi.traverse(tex)
    dr.enable_grad(params['value'])
    params.update()

    value = tex.eval(si)
    dr.forward(params['value'].y)

    eps = 1e-3
    finite_diff = (eval_green(0.5 + eps) - eval_green(0.5 - eps)) / (2 * eps)
    dr.assert_allclose(dr.grad(value), finite_diff, atol=1e-3)


def test03_fetch_matches_host(variants_vec_spectral):
    """The device implementation of the model agrees with rgb2spec_fetch()"""
    rng = np.random.default_rng(0)
    colors = rng.random((256, 3)).astype(np.float32)
    special = np.array([[0, 0, 0], [1, 1, 1], [0.5, 0.5, 0.5], [0.5, 0.5, 0.3],
                        [0.3, 0.5, 0.5], [0.5, 0.3, 0.5], [1, 0, 0], [0, 1, 1]],
                       dtype=np.float32)
    colors = np.concatenate([colors, special])

    ref = np.array([mi.srgb_model_fetch(mi.ScalarColor3f(c)) for c in colors])
    dev = np.array(mi.srgb_model_fetch(mi.Color3f(*colors.T))).T

    assert np.allclose(ref, dev, rtol=1e-5, atol=1e-5)
