# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
import drjit as dr
import misuka as mi

import numpy as np
import pytest
import os
import argparse

from os.path import join, exists

from misuka.scalar_acoustic.test.util import fresolver_append_path
from misuka.scalar_acoustic.test.util import find_resource
from misuka.scalar_acoustic import ScalarTransform4f as T

output_dir = find_resource('resources/data_acoustic/tests/integrators')

# Directory next to this test file where rendered ETCs and reference copies are
# written on failure, so they can be inspected/plotted (see plot_etcs.py).
tests_dir = os.path.dirname(os.path.abspath(__file__))


# -------------------------------------------------------------------
#              Cross-backend (LLVM <-> Metal) equivalence
# -------------------------------------------------------------------
#
# These helpers render the *same* scene on two backends and compare the
# resulting ETCs / gradients. Unlike the regression tests above (which compare a
# single variant against a stored .exr), they catch backend-specific divergences
# such as the Metal backend dropping the direct sound. They deliberately do NOT
# use the `variants_*` fixtures from conftest.py (which parametrize a single
# variant per test) -- instead each test drives both variants internally,
# rebuilding the scene under each because Mitsuba types are variant-specific.

# Non-`ad` pair for primal (forward) rendering, `ad` pair for gradients.
PRIMAL_PAIR = ('llvm_acoustic', 'metal_acoustic')
AD_PAIR     = ('llvm_ad_acoustic', 'metal_ad_acoustic')


def _backend_available(variant):
    """True if `variant` is compiled and its JIT backend is live at runtime."""
    if variant not in mi.variants():
        return False
    if variant.startswith('llvm'):
        return dr.has_backend(dr.JitBackend.LLVM)
    if variant.startswith('metal'):
        return dr.has_backend(dr.JitBackend.Metal)
    if variant.startswith('cuda'):
        return dr.has_backend(dr.JitBackend.CUDA)
    return True


def require_pair(pair):
    """Skip the test unless both variants of `pair` are usable."""
    missing = [v for v in pair if not _backend_available(v)]
    if missing:
        pytest.skip(f"cross-backend equivalence needs {pair}; unavailable: {missing}")


def _variant_cleanup():
    """Same teardown conftest.py runs between variant-scoped tests, so switching
    variants mid-test does not leak kernels/instances across backends."""
    dr.sync_thread()
    dr.flush_kernel_cache()
    dr.kernel_history_clear()
    dr.flush_malloc_cache()
    dr.detail.malloc_clear_statistics()
    dr.detail.clear_registry()
    dr.set_flag(dr.JitFlag.Default, True)


def render_on_variant(variant, build_and_render):
    """Set `variant`, run `build_and_render()` (which builds the scene + integrator,
    renders, and must `dr.eval()` its result before returning -- see below),
    copy the result to a host NumPy array, and clean up. All variant-specific
    objects are created and destroyed inside the chosen variant.

    `build_and_render` must force its result with `dr.eval()` *before*
    returning it, not leave it lazily pending: Mitsuba objects it creates as
    locals (the scene, in particular) are only kept alive by that function's
    own frame, and become eligible for garbage collection the moment it
    returns. A value still lazily pending at that point can end up evaluated
    later against a since-collected scene and crash."""
    mi.set_variant(variant)
    try:
        result = build_and_render()
        arr = np.array(result, dtype=np.float64)
    finally:
        _variant_cleanup()
    return arr


def _dump_pair(label, ref, test):
    """Write both ETCs next to the test file for inspection on failure."""
    safe = label.replace('/', '_').replace(' ', '_')
    for tag, arr in (('llvm_ref', ref), ('metal_test', test)):
        try:
            path = join(tests_dir, f"equiv_{safe}_{tag}.exr")
            mi.util.write_bitmap(path, mi.TensorXf(np.asarray(arr, dtype=np.float32)))
            print(f"-> wrote {path}")
        except Exception as e:  # dumping is best-effort
            print(f"-> could not write {tag}: {e}")


def assert_direct_sound_present(ref, test, *, label, min_ratio=0.5):
    """Assert the strongest bin of the reference ETC (the direct arrival) is also
    present, with comparable energy, on the test backend. This is the assertion
    that directly targets the reported Metal 'missing direct sound' symptom."""
    ref = np.asarray(ref, dtype=np.float64)
    test = np.asarray(test, dtype=np.float64)
    assert ref.shape == test.shape, \
        f"{label}: shape mismatch {test.shape} != {ref.shape}"
    idx = np.unravel_index(int(np.argmax(np.abs(ref))), ref.shape)
    ref_peak = float(ref[idx])
    test_val = float(test[idx])
    assert ref_peak > 0, f"{label}: reference ETC has no signal"
    if test_val <= min_ratio * ref_peak:
        _dump_pair(label, ref, test)
        pytest.fail(
            f"{label}: direct sound missing/attenuated on test backend at bin "
            f"{idx}: ref={ref_peak:.4g} test={test_val:.4g} "
            f"(need >= {min_ratio:g}x)")


def assert_etc_equivalent(ref, test, *, label, rtol=5e-2, atol_frac=2e-2):
    """Compare two ETCs bin-by-bin with a relative tolerance plus a floor
    (`atol_frac` * reference peak) so near-zero bins don't dominate."""
    ref = np.asarray(ref, dtype=np.float64)
    test = np.asarray(test, dtype=np.float64)
    assert ref.shape == test.shape, \
        f"{label}: shape mismatch {test.shape} != {ref.shape}"
    peak = float(np.max(np.abs(ref)))
    atol = atol_frac * peak
    if not np.allclose(test, ref, rtol=rtol, atol=atol):
        denom = np.maximum(np.abs(ref), atol)
        rel = np.abs(test - ref) / denom
        _dump_pair(label, ref, test)
        pytest.fail(
            f"{label}: ETCs differ (rel err max={rel.max():.3g} "
            f"mean={rel.mean():.3g}, peak={peak:.3g}, "
            f"rtol={rtol:g} atol={atol:.3g})")


def assert_grad_equivalent(ref, test, *, label, rtol=5e-2, atol=1e-6):
    """Compare gradients from two backends, failing on NaN/Inf or disagreement."""
    ref = np.asarray(ref, dtype=np.float64)
    test = np.asarray(test, dtype=np.float64)
    assert np.all(np.isfinite(ref)), f"{label}: llvm gradient not finite: {ref}"
    assert np.all(np.isfinite(test)), f"{label}: metal gradient not finite: {test}"
    if not np.allclose(test, ref, rtol=rtol, atol=atol):
        pytest.fail(
            f"{label}: gradients differ llvm={ref.ravel()} metal={test.ravel()} "
            f"(rtol={rtol:g} atol={atol:g})")

def test01_constructor_valid(variants_all_acoustic):
    """Test that the integrator can be constructed with valid parameters."""
    integrator = mi.load_dict({'type': 'acoustic_path', 'max_time': 1.0})
    assert integrator is not None

    integrator = mi.load_dict({
        'type': 'acoustic_path',
        'max_time': 5.0,
        'speed_of_sound': 100.0,
        'max_depth': 10,
        'rr_depth': 100000,
        'max_energy_loss': 60.0,
    })
    assert integrator is not None


def test02_constructor_max_time_invalid(variants_all_acoustic):
    """max_time <= 0 must raise an exception."""
    with pytest.raises(Exception):
        mi.load_dict({'type': 'acoustic_path', 'max_time': -1.0})
    with pytest.raises(Exception):
        mi.load_dict({'type': 'acoustic_path', 'max_time': 0.0})


def test03_constructor_speed_of_sound_invalid(variants_all_acoustic):
    """speed_of_sound <= 0 must raise an exception."""
    with pytest.raises(Exception):
        mi.load_dict({'type': 'acoustic_path', 'max_time': 1.0, 'speed_of_sound': 0.0})
    with pytest.raises(Exception):
        mi.load_dict({'type': 'acoustic_path', 'max_time': 1.0, 'speed_of_sound': -1.0})


def test04_constructor_max_depth_invalid(variants_all_acoustic):
    """max_depth < -1 must raise an exception, but -1 and 0 are valid."""
    with pytest.raises(Exception):
        mi.load_dict({'type': 'acoustic_path', 'max_time': 1.0, 'max_depth': -2})

    # max_depth=-1 (infinite) is valid
    integrator = mi.load_dict({'type': 'acoustic_path', 'max_time': 1.0, 'max_depth': -1})
    assert integrator is not None

    # max_depth=0 is valid
    integrator = mi.load_dict({'type': 'acoustic_path', 'max_time': 1.0, 'max_depth': 0})
    assert integrator is not None


def test05_constructor_rr_depth_invalid(variants_all_acoustic):
    """rr_depth <= 0 must raise an exception, and Russian roulette is not yet
    implemented, so any non-default rr_depth must also raise."""
    with pytest.raises(Exception):
        mi.load_dict({'type': 'acoustic_path', 'max_time': 1.0, 'rr_depth': 0})
    with pytest.raises(Exception):
        mi.load_dict({'type': 'acoustic_path', 'max_time': 1.0, 'rr_depth': -1})
    with pytest.raises(Exception):
        mi.load_dict({'type': 'acoustic_path', 'max_time': 1.0, 'rr_depth': 50})


def test06_constructor_max_energy_loss_invalid(variants_all_acoustic):
    """max_energy_loss <= 0 (and not -1) must raise an exception."""
    with pytest.raises(Exception):
        mi.load_dict({'type': 'acoustic_path', 'max_time': 1.0, 'max_energy_loss': -2.0})
    with pytest.raises(Exception):
        mi.load_dict({'type': 'acoustic_path', 'max_time': 1.0, 'max_energy_loss': -0.5})

    # 0 is invalid
    with pytest.raises(Exception):
        mi.load_dict({'type': 'acoustic_path', 'max_time': 1.0, 'max_energy_loss': 0.0})

    # -1 (disabled) is valid
    integrator = mi.load_dict({'type': 'acoustic_path', 'max_time': 1.0, 'max_energy_loss': -1.0})
    assert integrator is not None

@pytest.mark.parametrize('rfilter', ['box', 'gaussian'])
def test07_hide_emitters(variants_all_acoustic, rfilter):
    speed_of_sound = 1
    max_time = 3
    sample_rate = 1
    time_bins = int(sample_rate * max_time)
    spp = 1


    scene = mi.load_dict({
        'type': 'scene',
        'microphone': {
            'type': 'microphone',
            'kappa': 1e12,
            'origin': [0, 0, 1],
            'direction': [0, 0, -1],
            'film': {
                'type': 'tape',
                'time_bins': time_bins,
                'frequencies': '1',
                'rfilter': {'type': rfilter},
            }
        },
        'rectangle': {
            'type': 'rectangle',
            'flip_normals': False,
            'emitter': {
                'type': 'area',
                'radiance': {'type': 'uniform',
                                'value': 1},
            }
        },
    })


    integrator = mi.load_dict({'type': 'acoustic_path',
                               'speed_of_sound': speed_of_sound,
                               'max_depth': 1,
                               'max_time': max_time, 'hide_emitters': True})
    assert integrator.hide_emitters
    etc_hide_emitter = integrator.render(scene, seed=0, spp=spp)
    print(etc_hide_emitter.numpy().shape)
    print(f'{etc_hide_emitter[:, 0, 0] = }')
    assert dr.allclose(etc_hide_emitter[:, 0, 0], 0)

    integrator = mi.load_dict({'type': 'acoustic_path',
                               'speed_of_sound': speed_of_sound,
                               'max_depth': 1,
                               'max_time': max_time, 'hide_emitters': False})
    assert not integrator.hide_emitters
    etc = integrator.render(scene, seed=0, spp=spp)
    print(f'{etc[:, 0, 0] = }')
    assert not dr.allclose(etc[:, 0, 0], 0)



# -------------------------------------------------------------------
#                          Test configs
# -------------------------------------------------------------------
class ConfigBase:
    """
    Base class to configure test scene and define the parameter to update
    """
    requires_discontinuities = False

    def __init__(self) -> None:
        self.spp = 2**20
        self.seed = 0
        self.speed_of_sound = 340
        self.max_time = 0.2
        self.max_energy_loss = 20
        self.sampling_rate = 1000.0
        self.frequencies = '250, 500'
        self.error_mean_threshold = 0.05
        self.error_max_threshold = 0.5
        self.error_mean_threshold_bwd = 0.05
        self.emitter_radius = 0.5
        self.max_depth = -1
        self.hide_emitters = False

        self.integrator_dict = {
            'max_depth': self.max_depth,
            'speed_of_sound': self.speed_of_sound,
            'max_time': self.max_time,
            'max_energy_loss': self.max_energy_loss,
            'hide_emitters': self.hide_emitters,
        }

        self.sensor_dict = {
            'type': 'microphone',
            'origin': [-2, 0, 0],
            'direction': [1, 0, 0],
            'kappa': 0,
            'film': {
                'type': 'tape',
                'time_bins': int(self.sampling_rate * self.max_time),
                'frequencies': self.frequencies,
                'rfilter': {'type': 'gaussian', 'stddev': 0.25},
                'sample_border': False,
                'pixel_format': 'MultiChannel',
                'component_format': 'float32',
            }
        }

        # Set the config name based on the type
        import re
        self.name = re.sub(r'(?<!^)(?=[A-Z])', '_', self.__class__.__name__[:-6]).lower()

    def initialize(self):
        """
        Initialize the configuration, loading the Mitsuba scene and storing a
        copy of the scene parameters to compute gradients for.
        """

        self.sensor_dict['film']['frequencies'] = self.frequencies
        self.sensor_dict['film']['time_bins'] = int(self.sampling_rate * self.max_time)
        self.scene_dict['sensor'] = self.sensor_dict

        @fresolver_append_path
        def create_scene():
            return mi.load_dict(self.scene_dict)
        self.scene = create_scene()
        self.params = mi.traverse(self.scene)

        if hasattr(self, 'key'):
            self.params.keep([self.key])
            self.initial_state = type(self.params[self.key])(self.params[self.key])

    def update(self, theta):
        """
        This method update the scene parameter associated to this config
        """
        self.params[self.key] = self.initial_state + theta
        dr.set_label(self.params, 'params')
        self.params.update()
        dr.eval()

    def __repr__(self) -> str:
        return f'{self.name}[\n' \
            f'  integrator = {self.integrator_dict},\n' \
            f'  spp = {self.spp},\n' \
            f'  key = {self.key if hasattr(self, "key") else "None"}\n' \
            f']'


class ShoeboxConfig(ConfigBase):
    """
    Rendering of a simple shoebox room.
    """

    def __init__(self) -> None:
        super().__init__()
        self.scene_dict = {
            'type': 'scene',
            'shoebox': {
                'type': 'cube',
                'flip_normals': True,
                'to_world': T().scale([7, 5, 3]),
                'bsdf': {
                    'type': 'acousticbsdf',
                    'specular_lobe_width': 0.001,
                    'absorption': {
                        'type': 'spectrum', 'value': [(250, 0.5), (500, 0.5)],
                    },
                    'scattering': {
                        'type': 'spectrum', 'value': [(250, 0.1), (500, 0.9)]
                    },
                },
            },
            'spherical_emitter': {
                'type': 'sphere',
                'radius': self.emitter_radius,
                'center': [2, 0, 0],
                'emitter': {
                    'type': 'area',
                    'radiance': {
                        'type': 'uniform',
                        'value': 1,
                    },
                },
            },
        }


# -------------------------------------------------------------------
#                           List configs
# -------------------------------------------------------------------
# List of integrators to test
INTEGRATORS = [
    'acoustic_path',
]

CONFIGS_LIST = [
    ShoeboxConfig,
]


CONFIGS_PRIMAL = []
for integrator_name in INTEGRATORS:
    todos = CONFIGS_LIST
    for config in todos:
        CONFIGS_PRIMAL.append((integrator_name, config))


# -------------------------------------------------------------------
#                          Test rendering results
# -------------------------------------------------------------------

@pytest.mark.slow
@pytest.mark.parametrize('integrator_name, config', CONFIGS_PRIMAL)
def test08_rendering_primal(variants_all_acoustic, integrator_name, config):
    config = config()
    config.initialize()

    config.integrator_dict['type'] = integrator_name
    integrator = mi.load_dict(config.integrator_dict, parallel=False)

    filename = join(output_dir, f"test_{config.name}_primal_ref.exr")
    etc_ref = mi.TensorXf(mi.Bitmap(filename))
    etc = integrator.render(config.scene, seed=0, spp=config.spp)

    #FIXME: Remove normalization here once integrator outputs properly scaled ETCs.
    etc /= dr.max(dr.abs(etc))
    etc_ref /= dr.max(dr.abs(etc_ref))

    error = dr.abs(etc - etc_ref) / dr.maximum(dr.abs(etc_ref), 2e-2)
    error_mean = dr.mean(error, axis=None)
    error_max = dr.max(error, axis=None)

    if error_mean > config.error_mean_threshold or error_max > config.error_max_threshold:
        print(f"Failure in config: {config.name}, {integrator_name}")
        print(f"-> error mean: {error_mean} (threshold={config.error_mean_threshold})")
        print(f"-> error max: {error_max} (threshold={config.error_max_threshold})")
        print(f'-> reference image: {filename}')
        filename = join(tests_dir, f"test_{integrator_name}-{config.name}-{mi.variant()}-primal.exr")
        filename_ref = join(tests_dir, f"test_{integrator_name}-{config.name}-{mi.variant()}-ref.exr")
        print(f'-> write current image: {filename}')
        mi.util.write_bitmap(filename, etc)
        mi.util.write_bitmap(filename_ref, etc_ref)
        pytest.fail("ETC values exceeded configuration's tolerances!")


# -------------------------------------------------------------------
#            LLVM <-> Metal forward-rendering equivalence
# -------------------------------------------------------------------

@pytest.mark.slow
@pytest.mark.parametrize('config', CONFIGS_LIST)
def test09_llvm_metal_equivalence_primal(config):
    """`acoustic_path` must produce the same ETC on the LLVM and Metal backends,
    including the direct sound (strongest arrival)."""
    require_pair(PRIMAL_PAIR)

    seed = 0
    spp = 2 ** 16
    name = config().name

    def build_and_render():
        cfg = config()
        cfg.initialize()
        cfg.integrator_dict['type'] = 'acoustic_path'
        integrator = mi.load_dict(cfg.integrator_dict, parallel=False)
        result = integrator.render(cfg.scene, seed=seed, spp=spp)
        dr.eval(result)
        return result

    ref  = render_on_variant(PRIMAL_PAIR[0], build_and_render)  # llvm
    test = render_on_variant(PRIMAL_PAIR[1], build_and_render)  # metal

    label = f"acoustic_path/{name}"
    assert_direct_sound_present(ref, test, label=label)
    assert_etc_equivalent(ref, test, label=label)


# -------------------------------------------------------------------
#                      Generate reference images
# -------------------------------------------------------------------

if __name__ == "__main__":
    """
    Generate reference ETCs for all configs.
    """
    parser = argparse.ArgumentParser(prog='GenerateConfigReferenceETCs')
    parser.add_argument('--spp', default=2**30, type=int,
                        help='Samples per pixel. Default value: 2**30')
    args = parser.parse_args()

    mi.set_variant('cuda_acoustic', 'llvm_acoustic')

    if not exists(output_dir):
        os.makedirs(output_dir)

    for config in CONFIGS_LIST:
        config = config()
        print(f"name: {config.name}")

        config.initialize()

        integrator_path = mi.load_dict({
            'type': 'acoustic_path',
            'speed_of_sound': config.speed_of_sound,
            'max_depth': config.integrator_dict['max_depth'],
            'max_time': config.max_time,
            'max_energy_loss': config.max_energy_loss,
        }, parallel=False)

        # Primal render
        etc_ref = integrator_path.render(config.scene, seed=config.seed, spp=args.spp)

        filename = join(output_dir, f"test_{config.name}_primal_ref.exr")
        mi.util.write_bitmap(filename, etc_ref)
