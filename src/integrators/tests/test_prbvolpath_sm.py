"""
Tests for the sample-matching volumetric integrator `prbvolpath_sm`: the
primal image and the backward extinction gradient, both against `prbvolpath`.
The gradient is checked for a single-channel extinction, a three-channel
extinction in RGB variants and a wavelength-dependent one in spectral variants.
"""

import gc

import pytest
import drjit as dr
import mitsuba as mi


SM_CONFIGS = [
    {'type': 'prbvolpath_sm'},
    {'type': 'prbvolpath_sm', 'gradient_samples_per_segment': 4},
    {'type': 'prbvolpath_sm_linear'},
    {'type': 'prbvolpath_sm_quad'},
]


def make_scene(integrator_dict, grid, grid_scale=1.0):
    import numpy as np
    return mi.load_dict({
        'type': 'scene',
        'integrator': integrator_dict,
        'light': {'type': 'constant', 'radiance': 1.0},
        'sensor': {
            'type': 'perspective', 'fov': 45,
            'to_world': mi.ScalarTransform4f().look_at([0, 0, 4], [0, 0, 0], [0, 1, 0]),
            'film': {'type': 'hdrfilm', 'width': 16, 'height': 16,
                     'rfilter': {'type': 'box'}, 'pixel_format': 'rgb'},
            'sampler': {'type': 'independent', 'sample_count': 8},
        },
        'medium_box': {
            'type': 'cube', 'bsdf': {'type': 'null'},
            'interior': {
                'type': 'heterogeneous',
                'sigma_t': {
                    'type': 'gridvolume',
                    'grid': mi.VolumeGrid((grid * grid_scale).astype(np.float32)),
                    'to_world': mi.ScalarTransform4f().translate([-1, -1, -1]).scale(2.0),
                },
                'albedo': 0.8, 'scale': 2.0,
            },
        },
        # An opaque surface inside the medium exercises the mixed
        # surface/medium code path and the probe segment bookkeeping.
        'sphere': {
            'type': 'sphere', 'radius': 0.35,
            'to_world': mi.ScalarTransform4f().translate([0.5, 0, 0]),
            'bsdf': {'type': 'diffuse',
                     'reflectance': {'type': 'rgb', 'value': [0.5, 0.5, 0.5]}},
        },
    })


def make_grid():
    import numpy as np
    rng = np.random.default_rng(0)
    return rng.uniform(0.4, 1.6, size=(4, 4, 4, 1)).astype(np.float32)


def channel_gradient_sums(scene, n_seeds, spp):
    """The gradient is a vector over voxels. Summing it per channel turns it
    into one scalar per channel (the dot product with a one-hot channel
    direction), which is what the tests compare. Returns (n_seeds, channels)."""
    import numpy as np
    params = mi.traverse(scene)
    key = [k for k in params.keys() if 'sigma_t' in k and k.endswith('.data')][0]
    channels = params[key].shape[-1]
    vals = []
    for s in range(n_seeds):
        dr.enable_grad(params[key])
        params.update()
        img = mi.render(scene, params, spp=spp, seed=10 + s, seed_grad=1000 + s)
        dr.backward(dr.mean(img, axis=None))
        g = np.array(dr.grad(params[key])).reshape(-1, channels)
        dr.disable_grad(params[key])
        assert np.isfinite(g).all(), 'non-finite gradients'
        vals.append(g.sum(axis=0))
    return np.array(vals)


def assert_gradients_agree(v_ref, v_sm, config):
    """Per channel, the two means must agree within some noise threshold"""
    n_seeds = v_ref.shape[0]
    for c in range(v_ref.shape[1]):
        m_ref, m_sm = v_ref[:, c].mean(), v_sm[:, c].mean()
        sigma = (v_ref[:, c].std() + v_sm[:, c].std()) / (n_seeds ** 0.5)
        assert abs(m_sm - m_ref) < max(4 * sigma, 0.15 * abs(m_ref)), \
            f'gradient mismatch in channel {c}: sm={m_sm:.4e} ref={m_ref:.4e} (config={config})'


def test01_primal_matches_prbvolpath(variants_all_ad_rgb_unpolarized):
    import numpy as np
    grid = make_grid()
    img_ref = np.array(mi.render(
        make_scene({'type': 'prbvolpath', 'max_depth': 4}, grid), spp=32, seed=3))
    img_sm = np.array(mi.render(
        make_scene({'type': 'prbvolpath_sm', 'max_depth': 4}, grid),
        spp=32, seed=3))
    assert np.isfinite(img_sm).all()
    assert np.allclose(img_sm.mean(), img_ref.mean(), rtol=0.05)


@pytest.mark.parametrize('config', SM_CONFIGS)
def test02_gradients_single_channel_extinction(variants_all_ad_rgb_unpolarized, config):
    import numpy as np
    grid = make_grid()
    n_seeds, spp = 4, 32

    v_ref = channel_gradient_sums(
        make_scene({'type': 'prbvolpath', 'max_depth': 4}, grid), n_seeds, spp)
    v_sm = channel_gradient_sums(
        make_scene({'max_depth': 4, **config}, grid), n_seeds, spp)
    assert_gradients_agree(v_ref, v_sm, config)


def make_grid_rgb():
    """Three-channel extinction; one channel would see the same value at every wavelength"""
    import numpy as np
    rng = np.random.default_rng(0)
    g = rng.uniform(0.4, 1.6, size=(4, 4, 4, 3)).astype(np.float32)
    g[..., 1] *= 0.6
    g[..., 2] *= 1.4
    return g


@pytest.mark.parametrize('config', SM_CONFIGS)
def test03_gradients_three_channel_extinction(variants_all_ad_rgb_unpolarized, config):
    import numpy as np
    grid = make_grid_rgb()
    n_seeds, spp = 4, 64

    scene_ref = make_scene({'type': 'prbvolpath', 'max_depth': 4}, grid)
    v_ref = channel_gradient_sums(scene_ref, n_seeds, spp)

    # Release the reference scene so that only one Medium instance is alive
    del scene_ref
    gc.collect()
    v_sm = channel_gradient_sums(make_scene({'max_depth': 4, **config}, grid), n_seeds, spp)
    assert_gradients_agree(v_ref, v_sm, config)


def test04_primal_spectral_variant(variants_all_ad_spectral):
    """Primal image of a three-channel extinction in a spectral variant. The
    segment records of the second kernel must carry the path's wavelengths."""
    if mi.is_polarized:
        pytest.skip('prbvolpath_sm and prbvolpath do not support polarized rendering')
    import numpy as np
    grid = make_grid_rgb()
    img_ref = np.array(mi.render(
        make_scene({'type': 'prbvolpath', 'max_depth': 4}, grid), spp=32, seed=3))
    img_sm = np.array(mi.render(
        make_scene({'type': 'prbvolpath_sm', 'max_depth': 4}, grid),
        spp=32, seed=3))
    assert np.isfinite(img_sm).all()
    assert np.allclose(img_sm.mean(), img_ref.mean(), rtol=0.05)


@pytest.mark.parametrize('config', SM_CONFIGS)
def test05_gradients_spectral_extinction(variants_all_ad_spectral, config):
    """Extinction gradient in a spectral variant. If a record lost the
    wavelengths, the second kernel would evaluate the medium at wavelength zero
    and the gradient would be biased without any error."""
    if mi.is_polarized:
        pytest.skip('prbvolpath_sm and prbvolpath do not support polarized rendering')
    import numpy as np
    grid = make_grid_rgb()
    n_seeds, spp = 4, 64

    scene_ref = make_scene({'type': 'prbvolpath', 'max_depth': 4}, grid)
    v_ref = channel_gradient_sums(scene_ref, n_seeds, spp)

    # Release the reference scene so that only one Medium instance is alive
    del scene_ref
    gc.collect()
    v_sm = channel_gradient_sums(make_scene({'max_depth': 4, **config}, grid), n_seeds, spp)
    assert_gradients_agree(v_ref, v_sm, config)
