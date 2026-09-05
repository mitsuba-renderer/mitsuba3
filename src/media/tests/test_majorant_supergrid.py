"""
Tests for the majorant supergrid (`majorant_resolution_factor`).

The supergrid only changes how free-flight distances are sampled (a
spatially-varying majorant with DDA traversal instead of a single global
majorant); it must not change what is being estimated. Primal renders and
adjoint gradients must therefore agree with the global-majorant baseline
within Monte Carlo noise.
"""

import pytest
import drjit as dr
import mitsuba as mi


def make_grid():
    import numpy as np
    rng = np.random.default_rng(0)
    grid = rng.uniform(0.2, 0.8, size=(8, 8, 8, 1)).astype(np.float32)
    # An outlier voxel: with a global majorant this taxes every ray in the
    # volume; the supergrid must localize it (and stay unbiased).
    grid[1, 1, 1, 0] = 4.0
    return grid


def make_scene(grid, factor):
    import numpy as np
    medium = {
        'type': 'heterogeneous',
        'sigma_t': {
            'type': 'gridvolume',
            'grid': mi.VolumeGrid(grid),
            'to_world': mi.ScalarTransform4f().translate([-1, -1, -1]).scale(2.0),
        },
        'albedo': 0.8, 'scale': 2.0,
    }
    if factor:
        medium['majorant_resolution_factor'] = factor
        medium['majorant_factor'] = 1.01
    return mi.load_dict({
        'type': 'scene',
        'integrator': {'type': 'volpath', 'max_depth': 16},
        'light': {'type': 'constant', 'radiance': 1.0},
        'sensor': {
            'type': 'perspective', 'fov': 45,
            'to_world': mi.ScalarTransform4f().look_at([0, 0, 4], [0, 0, 0], [0, 1, 0]),
            'film': {'type': 'hdrfilm', 'width': 16, 'height': 16,
                     'rfilter': {'type': 'box'}, 'pixel_format': 'rgb'},
            'sampler': {'type': 'independent', 'sample_count': 16},
        },
        'medium_box': {'type': 'cube', 'bsdf': {'type': 'null'}, 'interior': medium},
    })


@pytest.mark.parametrize('factor', [1, 2, 4])
def test01_primal_parity(variants_vec_backends_once_rgb, factor):
    import numpy as np
    grid = make_grid()
    means = {}
    for f in (0, factor):
        imgs = [np.array(mi.render(make_scene(grid, f), spp=64, seed=s))
                for s in range(4)]
        img = np.mean(imgs, axis=0)
        assert np.isfinite(img).all()
        means[f] = img.mean()
    assert np.allclose(means[0], means[factor], rtol=0.03), \
        f'supergrid factor={factor} changed the primal render: {means}'


def test02_gradient_parity(variants_all_ad_rgb_unpolarized):
    import numpy as np
    grid = make_grid()

    def dirderiv(factor, n=4):
        scene = make_scene(grid, factor)
        # volpath is not differentiable; use the AD integrator
        integrator = mi.load_dict({'type': 'prbvolpath', 'max_depth': 16})
        params = mi.traverse(scene)
        key = [k for k in params.keys() if k.endswith('sigma_t.data')][0]
        vals = []
        for s in range(n):
            dr.enable_grad(params[key]); params.update()
            img = mi.render(scene, params, integrator=integrator,
                            spp=32, seed=10 + s, seed_grad=1000 + s)
            dr.backward(dr.mean(img, axis=None))
            g = np.array(dr.grad(params[key])).ravel()
            dr.disable_grad(params[key])
            assert np.isfinite(g).all()
            vals.append(float((g * grid.ravel()).sum()))
        return np.array(vals)

    v0, v2 = dirderiv(0), dirderiv(2)
    sigma = (v0.std() + v2.std()) / 2.0
    assert abs(v0.mean() - v2.mean()) < max(4 * sigma, 0.15 * abs(v0.mean())), \
        f'gradients diverge: off={v0.mean():.4e} on={v2.mean():.4e}'


def test03_homogeneous_rejects_supergrid(variants_all_rgb_unpolarized):
    with pytest.raises(RuntimeError, match='majorant'):
        mi.load_dict({'type': 'homogeneous', 'sigma_t': 1.0,
                      'majorant_resolution_factor': 4})


def check_bounds_extinction(n_channels):
    """The supergrid must bound sigma_t everywhere, or delta tracking is not
    just slow but wrong: a majorant below the extinction makes sigma_n
    negative and the null-collision weights meaningless.

    Three channels is the interesting case. In spectral variants a
    three-channel grid is not stored as extinction at all -- gridvolume
    replaces each voxel by the (c0, c1, c2, scale) coefficients of the sRGB
    spectral model, and only `scale` bounds the reconstructed spectrum.
    Pooling the polynomial coefficients along with it yields a number that
    bounds nothing.
    """
    import numpy as np
    # Structure, not noise. Uniform noise puts a near-maximal voxel in every
    # cell, so every local majorant equals the global one and the test passes
    # without exercising anything. A thin shell in empty space makes the cells
    # genuinely differ, which is also the case the supergrid exists for.
    n = 16
    c = (np.arange(n) + 0.5) / n - 0.5
    zz, yy, xx = np.meshgrid(c, c, c, indexing='ij')
    r = np.sqrt(xx ** 2 + yy ** 2 + zz ** 2)
    shell = np.where(np.abs(r - 0.3) < 0.06, 4.0, 0.01).astype(np.float32)
    grid = np.repeat(shell[..., None], n_channels, axis=3)
    if n_channels == 3:  # make the channels disagree
        grid[..., 1] *= 0.5
        grid[..., 2] *= 0.25
    medium = mi.load_dict({
        'type': 'heterogeneous',
        'sigma_t': {'type': 'gridvolume', 'grid': mi.VolumeGrid(grid)},
        'albedo': 0.5, 'scale': 3.0,
        'majorant_resolution_factor': 4, 'majorant_factor': 1.0,
    })

    n = 8192
    rng = np.random.default_rng(1)
    p = rng.uniform(0.001, 0.999, size=(n, 3)).astype(np.float32)
    mei = dr.zeros(mi.MediumInteraction3f, n)
    mei.p = mi.Point3f(mi.Float(p[:, 0]), mi.Float(p[:, 1]), mi.Float(p[:, 2]))
    mei.wi = mi.Vector3f(0, 0, 1)
    mei.sh_frame = mi.Frame3f(mei.wi)
    if mi.is_spectral:
        # UnpolarizedSpectrum, not Spectrum: wavelengths are never
        # polarized, and in a polarized variant `Spectrum` is the Mueller
        # matrix type, which the field does not accept.
        n_wav = dr.size_v(mi.UnpolarizedSpectrum)
        w = rng.uniform(360.0, 830.0, size=(n, n_wav)).astype(np.float32)
        mei.wavelengths = mi.UnpolarizedSpectrum(
            *[mi.Float(w[:, i]) for i in range(n_wav)])
    mei.medium = mi.MediumPtr(medium)

    kappa = medium.get_majorant(mei)
    _, _, sigma_t = medium.get_scattering_coefficients(mei)
    # axis=None: without it the reduction only collapses the channel axis and
    # leaves one entry per lane, which is not a bool.
    slack = dr.min(kappa - sigma_t, axis=None)
    assert dr.all(kappa >= sigma_t - 1e-4, axis=None), \
        f'majorant below the extinction by {-slack} ({n_channels} channels)'

    # The supergrid must also be doing something: if every cell carried the
    # global maximum the bound above would hold trivially.
    lo = dr.min(kappa, axis=None)
    hi = dr.max(kappa, axis=None)
    assert hi > 2 * lo, \
        f'majorant is nearly constant ({lo} .. {hi}); the supergrid degenerated'


@pytest.mark.parametrize('n_channels', [1, 3])
def test04_bounds_extinction_rgb(variants_vec_backends_once_rgb, n_channels):
    check_bounds_extinction(n_channels)


@pytest.mark.parametrize('n_channels', [1, 3])
def test05_bounds_extinction_spectral(variants_vec_backends_once_spectral,
                                      n_channels):
    check_bounds_extinction(n_channels)
