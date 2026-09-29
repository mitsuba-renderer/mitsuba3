"""
Tests for the sample matching volumetric integrator `prbvolpath_sm`. The primal image and
the extinction gradient are compared against `prbvolpath`, and the gradient is also compared
against finite differences.
"""

import gc

import pytest
import drjit as dr
import mitsuba as mi


SM_CONFIGS = [
    {'type': 'prbvolpath_sm'},
    {'type': 'prbvolpath_sm', 'gradient_samples_per_segment': 1},
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
        # An opaque sphere inside the medium, so that paths also hit surfaces inside the medium
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
    """Sums the voxel gradient over each channel, one value per seed and channel.
    Returns an array of shape (n_seeds, channels)."""
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
    """Per channel, the two means must agree within four standard errors"""
    n_seeds = v_ref.shape[0]
    for c in range(v_ref.shape[1]):
        m_ref, m_sm = v_ref[:, c].mean(), v_sm[:, c].mean()
        se = ((v_ref[:, c].var() + v_sm[:, c].var()) / n_seeds) ** 0.5
        assert abs(m_sm - m_ref) < 4 * se, \
            f'gradient mismatch in channel {c}: sm={m_sm:.4e} ref={m_ref:.4e} se={se:.1e} (config={config})'


def test01_primal_matches_prbvolpath(variants_all_ad_rgb_unpolarized):
    """Checks that the primal image matches prbvolpath in each channel, in RGB variants"""
    import numpy as np
    grid = make_grid()
    img_ref = np.array(mi.render(
        make_scene({'type': 'prbvolpath', 'max_depth': 4}, grid), spp=32, seed=3))
    img_sm = np.array(mi.render(
        make_scene({'type': 'prbvolpath_sm', 'max_depth': 4}, grid),
        spp=32, seed=3))
    assert np.isfinite(img_sm).all()
    assert np.allclose(img_sm.mean(axis=(0, 1)), img_ref.mean(axis=(0, 1)), rtol=0.05)


@pytest.mark.parametrize('config', SM_CONFIGS)
def test02_gradients_single_channel_extinction(variants_all_ad_rgb_unpolarized, config):
    """Checks that the gradient of a single channel extinction matches prbvolpath, in RGB
    variants (unbiased scene, which doesn't include zero density)"""
    import numpy as np
    grid = make_grid()
    n_seeds, spp = 16, 32

    v_ref = channel_gradient_sums(
        make_scene({'type': 'prbvolpath', 'max_depth': 4}, grid), n_seeds, spp)
    v_sm = channel_gradient_sums(
        make_scene({'max_depth': 4, **config}, grid), n_seeds, spp)
    assert_gradients_agree(v_ref, v_sm, config)


def make_grid_rgb():
    """Three channel extinction grid. RGB variants use the channels directly, and spectral
    variants turn each voxel's RGB value into a spectrum."""
    import numpy as np
    rng = np.random.default_rng(0)
    g = rng.uniform(0.4, 1.6, size=(4, 4, 4, 3)).astype(np.float32)
    g[..., 1] *= 0.6
    g[..., 2] *= 1.4
    return g


@pytest.mark.parametrize('config', SM_CONFIGS)
def test03_gradients_three_channel_extinction(variants_all_ad_rgb_unpolarized, config):
    """Checks that the gradient of a three channel extinction matches prbvolpath in each
    channel, in RGB variants (unbiased scene, which doesn't include zero density)"""
    import numpy as np
    grid = make_grid_rgb()
    n_seeds, spp = 32, 64

    scene_ref = make_scene({'type': 'prbvolpath', 'max_depth': 4}, grid)
    v_ref = channel_gradient_sums(scene_ref, n_seeds, spp)

    # Release the reference scene so that only one Medium instance is alive
    del scene_ref
    gc.collect()
    v_sm = channel_gradient_sums(make_scene({'max_depth': 4, **config}, grid), n_seeds, spp)
    assert_gradients_agree(v_ref, v_sm, config)


def test04_primal_spectral_variant(variants_all_ad_spectral):
    """Checks that the primal image matches prbvolpath in each channel, in spectral variants"""
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
    assert np.allclose(img_sm.mean(axis=(0, 1)), img_ref.mean(axis=(0, 1)), rtol=0.05)


@pytest.mark.parametrize('config', SM_CONFIGS)
def test05_gradients_spectral_extinction(variants_all_ad_spectral, config):
    """Checks that the gradient of a wavelength dependent extinction matches prbvolpath, in
    spectral variants (unbiased scene, which doesn't include zero density)"""
    if mi.is_polarized:
        pytest.skip('prbvolpath_sm and prbvolpath do not support polarized rendering')
    import numpy as np
    grid = make_grid_rgb()
    n_seeds, spp = 16, 64

    scene_ref = make_scene({'type': 'prbvolpath', 'max_depth': 4}, grid)
    v_ref = channel_gradient_sums(scene_ref, n_seeds, spp)

    # Release the reference scene so that only one Medium instance is alive
    del scene_ref
    gc.collect()
    v_sm = channel_gradient_sums(make_scene({'max_depth': 4, **config}, grid), n_seeds, spp)
    assert_gradients_agree(v_ref, v_sm, config)


def make_grid_thin():
    """Half of the voxels are nearly empty, but stay above the finite difference step"""
    import numpy as np
    rng = np.random.default_rng(0)
    grid = rng.uniform(0.4, 1.6, size=(8, 8, 8, 1)).astype(np.float32)
    thin = rng.uniform(size=grid.shape) < 0.5
    grid[thin] = rng.uniform(0.05, 0.1, size=int(thin.sum()))
    return grid


def assert_matches_finite_differences(config):
    """The gradient along all voxels matches a central finite difference"""
    import numpy as np
    grid, h, n_seeds = make_grid_thin(), 0.04, 8

    def loss(g, seed):
        scene = make_scene({'type': 'prbvolpath_sm', 'max_depth': 4}, g)
        return np.array(mi.render(scene, spp=4096, seed=seed)).mean()

    fd = [(loss(grid + h, s) - loss(grid - h, s)) / (2 * h) for s in range(n_seeds)]
    ad = channel_gradient_sums(make_scene({'max_depth': 4, **config}, grid), n_seeds, 128)[:, 0]

    se = np.sqrt(np.var(fd) / n_seeds + np.var(ad) / n_seeds)
    assert abs(np.mean(ad) - np.mean(fd)) < 4 * se, \
        f'ad={np.mean(ad):.4e} fd={np.mean(fd):.4e} se={se:.1e} (config={config})'


@pytest.mark.parametrize('config', [{'type': 'prbvolpath_sm'}, {'type': 'prbvolpath_sm_linear'}])
def test06_gradients_match_finite_differences(variants_all_ad_rgb_unpolarized, config):
    """Checks that the gradient matches a central finite difference on a grid with nearly
    empty voxels, in RGB variants"""
    assert_matches_finite_differences(config)


@pytest.mark.parametrize('config', [{'type': 'prbvolpath_sm'}, {'type': 'prbvolpath_sm_linear'}])
def test07_gradients_match_finite_differences_spectral(variants_all_ad_spectral, config):
    """Same as test06, in spectral variants"""
    if mi.is_polarized:
        pytest.skip('prbvolpath_sm does not support polarized rendering')
    assert_matches_finite_differences(config)


def make_colored_slab_scene(integrator_dict):
    """Scene of test08 and test09: a colored homogeneous slab in front of a white background,
    and a heterogeneous cube that no camera ray hits."""
    import numpy as np
    T = mi.ScalarTransform4f
    return mi.load_dict({
        'type': 'scene',
        'integrator': integrator_dict,
        'light': {'type': 'constant', 'radiance': 1.0},
        'sensor': {
            'type': 'orthographic',
            'to_world': T().look_at([0, 0, 5], [0, 0, 0], [0, 1, 0]) @ T().scale([0.3, 0.3, 1]),
            'film': {'type': 'hdrfilm', 'width': 16, 'height': 16,
                     'rfilter': {'type': 'box'}, 'pixel_format': 'rgb'},
            'sampler': {'type': 'independent'},
        },
        'slab': {
            'type': 'cube', 'bsdf': {'type': 'null'}, 'to_world': T().scale(0.5),
            'interior': {'type': 'homogeneous', 'albedo': 0.0, 'scale': 2.0,
                         'sigma_t': {'type': 'rgb', 'value': [0.1, 0.5, 1.0]}},
        },
        'unseen': {
            'type': 'cube', 'bsdf': {'type': 'null'}, 'to_world': T().translate([10, 0, 0]),
            'interior': {'type': 'heterogeneous', 'albedo': 0.5,
                         'sigma_t': {'type': 'gridvolume',
                                     'grid': mi.VolumeGrid(np.full((2, 2, 2, 1), 0.5, np.float32))}},
        },
    })


def assert_colored_slab_matches_prbvolpath():
    """Per channel, the slab's transmittance matches prbvolpath"""
    import numpy as np
    img_ref = np.array(mi.render(make_colored_slab_scene({'type': 'prbvolpath'}), spp=1024, seed=3))
    img_sm = np.array(mi.render(make_colored_slab_scene({'type': 'prbvolpath_sm'}), spp=1024, seed=3))
    assert np.allclose(img_sm.mean(axis=(0, 1)), img_ref.mean(axis=(0, 1)), rtol=0.05)


def test08_primal_colored_homogeneous(variants_all_ad_rgb_unpolarized):
    """Checks that tracking stays correct in a colored homogeneous medium when a heterogeneous
    medium is also in the scene, in RGB variants"""
    assert_colored_slab_matches_prbvolpath()


def test09_primal_colored_homogeneous_spectral(variants_all_ad_spectral):
    """Same as test08, in spectral variants"""
    if mi.is_polarized:
        pytest.skip('prbvolpath_sm and prbvolpath do not support polarized rendering')
    assert_colored_slab_matches_prbvolpath()
