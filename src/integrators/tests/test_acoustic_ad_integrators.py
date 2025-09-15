"""
Overview
--------

This file defines a set of unit tests to assess the correctness of the
acoustic adjoint integrators  `acoustic_ad`, `acoustic_ad_threepoint`,
`acoustic_prb`, `acoustic_prb_threepoint`. All integrators will be tested for
their implementation of primal rendering, adjoint forward
rendering and adjoint backward rendering.

- For primal rendering, the output ETC will be compared to a ground truth
  ETC precomputed in the `resources/data_acoustic/tests/integrators` directory.
- Adjoint backward rendering will be compared against finite differences.
- Adjoint forward rendering will be compared against finite differences.

Those tests will be run on a set of configurations (scene description + metadata)
also provided in this file. More tests can easily be added by creating a new
configuration type and add it to the *_CONFIGS_LIST below.

By executing this script with python directly it is possible to regenerate the
reference data (e.g. for a new configurations). Please see the following command:

``python3 test_acoustic_ad_integrators.py --help``

"""

from mitsuba.scalar_acoustic import ScalarTransform4f as T
import drjit as dr
import mitsuba as mi

import pytest
import os
import argparse

from os.path import join, exists

from mitsuba.scalar_rgb.test.util import fresolver_append_path
from mitsuba.scalar_rgb.test.util import find_resource

output_dir = find_resource('resources/data_acoustic/tests/integrators')

# -------------------------------------------------------------------
#                    Helper functions for error checking
# -------------------------------------------------------------------

def check_image_error(actual, reference, config, integrator_name, epsilon=2e-2,
                      test_type='primal', save_error_image=False):
    """
    Check image comparison errors and report failures with detailed information.

    Args:
        actual: The computed image
        reference: The reference image
        config: Test configuration with error thresholds
        integrator_name: Name of the integrator being tested
        epsilon: Small value to avoid division by zero (default: 2e-2)
        test_type: Type of test ('primal', 'fwd', etc.) for naming output files
        save_error_image: Whether to save an error visualization image

    Returns:
        bool: True if test passed, False otherwise
    """
    error = dr.abs(actual - reference) / dr.maximum(dr.abs(reference), epsilon)
    error_mean = dr.mean(error, axis=None).item()
    error_max = dr.max(error, axis=None).item()

    if error_mean > config.error_mean_threshold or error_max > config.error_max_threshold:
        print(f"Failure in config: {config.name}, {integrator_name}")

        # Detailed error reporting with percentages
        mean_exceeded = error_mean > config.error_mean_threshold
        max_exceeded = error_max > config.error_max_threshold

        if mean_exceeded:
            mean_diff = error_mean - config.error_mean_threshold
            mean_percent = (error_mean/config.error_mean_threshold - 1)*100
            print(f"-> error mean: {error_mean:.6f} (threshold={config.error_mean_threshold}) "
                  f"- exceeds by {mean_diff:.4f} ({mean_percent:.1f}%)")
        else:
            print(f"-> error mean: {error_mean:.6f} (threshold={config.error_mean_threshold}) - OK")

        if max_exceeded:
            max_diff = error_max - config.error_max_threshold
            max_percent = (error_max/config.error_max_threshold - 1)*100
            print(f"-> error max: {error_max:.6f} (threshold={config.error_max_threshold}) "
                  f"- exceeds by {max_diff:.4f} ({max_percent:.1f}%)")
        else:
            print(f"-> error max: {error_max:.6f} (threshold={config.error_max_threshold}) - OK")

        # Save debug images
        filename = join(output_dir, f"test_{config.name}_image_{test_type}_ref.exr")
        print(f'-> reference image: {filename}')

        filename = join(os.getcwd(), f"test_{integrator_name}_{config.name}_image_{test_type}.exr")
        print(f'-> write current image: {filename}')
        mi.util.write_bitmap(filename, actual)

        if save_error_image:
            filename = join(os.getcwd(), f"test_{integrator_name}_{config.name}_image_error.exr")
            print(f'-> write error image: {filename}')
            mi.util.write_bitmap(filename, error)

        return False
    return True


def check_gradient_error(grad, grad_ref, config, integrator_name, threshold_attr='error_mean_threshold_bwd',
                         epsilon=1e-3):
    """
    Check gradient comparison errors and report failures.

    Args:
        grad: The computed gradient
        grad_ref: The reference gradient
        config: Test configuration with error thresholds
        integrator_name: Name of the integrator being tested
        threshold_attr: Name of the threshold attribute in config (default: 'error_mean_threshold_bwd')
        epsilon: Small value to avoid division by zero (default: 1e-3)

    Returns:
        bool: True if test passed, False otherwise
    """
    error = dr.abs(grad - grad_ref) / dr.maximum(dr.abs(grad_ref), epsilon)
    threshold = getattr(config, threshold_attr)

    if error > threshold:
        print(f"Failure in config: {config.name}, {integrator_name}")
        print(f"-> grad:     {grad}")
        print(f"-> grad_ref: {grad_ref}")
        print(f"-> error: {error:.6f} (threshold={threshold})")

        # Calculate how much the threshold was exceeded
        error_diff = error - threshold
        error_percent = (error/threshold - 1)*100
        print(f"-> exceeds by {error_diff:.4f} ({error_percent:.1f}%)")
        print(f"-> ratio: {grad / grad_ref}")
        return False
    return True

# -------------------------------------------------------------------
#                          Test configs
# -------------------------------------------------------------------


class ConfigBase:
    """
    Base class to configure test scene and define the parameter to update
    """
    requires_discontinuities = False

    def __init__(self) -> None:
        self.spp = 2**22
        self.speed_of_sound = 1
        self.max_time = 20
        self.sampling_rate = 10.0
        self.frequencies = '250, 500'
        self.error_mean_threshold = 0.05
        self.error_max_threshold = 0.5
        self.error_mean_threshold_bwd = 0.05
        self.ref_fd_epsilon = 1e-3
        self.emitter_radius = 1
        self.max_depth = 4

        self.integrator_dict = {
            'max_depth': self.max_depth,
            'speed_of_sound': self.speed_of_sound,
            'max_time': self.max_time,

        }

        self.sensor_dict = {
            'type': 'microphone',
            'origin': [-2, 0, 0],
            'direction': [1, 0, 0],
            'kappa': 0,
            'film': {
                'type': 'tape',
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


class SphericalEmitterRadianceConfig(ConfigBase):
    """
    Radiance of a spherical emitter
    """

    def __init__(self) -> None:
        super().__init__()
        self.key = 'spherical_emitter.emitter.radiance.value'
        self.scene_dict = {
            'type': 'scene',
            'spherical_emitter': {
                'type': 'sphere',
                'radius': self.emitter_radius,
                'center': [2, 0, 0],
                'emitter': {
                    'type': 'area',
                    'radiance': {
                        'type': 'uniform',
                        'value': 1000,
                    },
                },
            },
        }


# -------------------------------------------------------------------
#            Test configs with discontinuities
# -------------------------------------------------------------------

# -------------------------------------------------------------------
#                           List configs
# -------------------------------------------------------------------
BASIC_CONFIGS_LIST = [
    SphericalEmitterRadianceConfig,
]

DISCONTINUOUS_CONFIGS_LIST = [
]

# List of configs that fail on integrators with depth less than three
INDIRECT_ILLUMINATION_CONFIGS_LIST = [
]

# List of integrators to test
# (Name, handles discontinuities, has render_backward, has render_forward)
INTEGRATORS = [
    ('acoustic_ad',             False,  True,   True),
    ('acoustic_prb',            False,  True,   False),
    ('acoustic_ad_threepoint',  True,   True,   True),
    ('acoustic_prb_threepoint', True,   True,   False)
]

CONFIGS_PRIMAL   = []
CONFIGS_BACKWARD = []
CONFIGS_FORWARD  = []
for integrator_name, handles_discontinuities, has_render_backward, has_render_forward in INTEGRATORS:
    todos = BASIC_CONFIGS_LIST + (DISCONTINUOUS_CONFIGS_LIST if handles_discontinuities else [])
    for config in todos:
        if (('direct' in integrator_name or 'projective' in integrator_name) and
                config in INDIRECT_ILLUMINATION_CONFIGS_LIST):
            continue

        CONFIGS_PRIMAL.append((integrator_name, config))
        if has_render_backward:
            CONFIGS_BACKWARD.append((integrator_name, config))
        if has_render_forward:
            CONFIGS_FORWARD.append((integrator_name, config))



# -------------------------------------------------------------------
#                           Unit tests
# -------------------------------------------------------------------


@pytest.mark.slow
@pytest.mark.parametrize('integrator_name, config', CONFIGS_PRIMAL)
def test01_rendering_primal(variants_all_ad_acoustic, integrator_name, config):
    config = config()
    config.initialize()

    config.integrator_dict['type'] = integrator_name
    integrator = mi.load_dict(config.integrator_dict, parallel=False)

    filename = join(output_dir, f"test_{config.name}_etc_primal_ref.exr")
    etc_primal_ref = mi.TensorXf(mi.Bitmap(filename))
    etc = integrator.render(config.scene, seed=0, spp=config.spp) / config.spp

    error = dr.abs(etc - etc_primal_ref) / dr.maximum(dr.abs(etc_primal_ref), 2e-2)
    error_mean = dr.mean(error, axis=None)
    error_max = dr.max(error, axis=None)

    if error_mean > config.error_mean_threshold or error_max > config.error_max_threshold:
        print(f"Failure in config: {config.name}, {integrator_name}")
        print(f"-> error mean: {error_mean} (threshold={config.error_mean_threshold})")
        print(f"-> error max: {error_max} (threshold={config.error_max_threshold})")
        print(f'-> reference image: {filename}')
        filename = join(os.getcwd(), f"test_{integrator_name}_{config.name}_etc_primal.exr")
        print(f'-> write current image: {filename}')
        mi.util.write_bitmap(filename, etc)
        pytest.fail("ETC values exceeded configuration's tolerances!")


@pytest.mark.slow
@pytest.mark.skipif(os.name == 'nt', reason='Skip those memory heavy tests on Windows')
@pytest.mark.parametrize('integrator_name, config', CONFIGS_FORWARD)
def test02_rendering_forward(variants_all_ad_acoustic, integrator_name, config):
    config = config()
    config.initialize()

    config.integrator_dict['type'] = integrator_name
    integrator = mi.load_dict(config.integrator_dict)


    filename = join(output_dir, f"test_{config.name}_etc_fwd_ref.exr")
    etc_fwd_ref = mi.TensorXf(mi.Bitmap(filename))

    theta = mi.Float(0.0)
    dr.enable_grad(theta)
    dr.set_label(theta, 'theta')
    config.update(theta)
    # We call dr.forward() here to propagate the gradient from the latent variable into
    # the scene parameter. This prevents dr.forward_to() in integrator.render_forward()
    # to trace gradients outside the dr.Loop().
    dr.forward(theta, dr.ADFlag.ClearEdges)

    dr.set_label(config.params, 'params')
    etc_fwd = integrator.render_forward(
        config.scene, seed=0, spp=config.spp, params=theta) / config.spp
    etc_fwd = dr.detach(etc_fwd)

    error = dr.abs(etc_fwd - etc_fwd_ref) / dr.maximum(dr.abs(etc_fwd_ref), 2e-1)
    error_mean = dr.mean(error, axis=None)
    error_max = dr.max(error, axis=None)

    if error_mean > config.error_mean_threshold or error_max > config.error_max_threshold:
        print(f"Failure in config: {config.name}, {integrator_name}")
        print(f"-> error mean: {error_mean} (threshold={config.error_mean_threshold})")
        print(f"-> error max: {error_max} (threshold={config.error_max_threshold})")
        print(f'-> reference image: {filename}')
        filename = join(os.getcwd(), f"test_{integrator_name}_{config.name}_image_fwd.exr")
        print(f'-> write current image: {filename}')
        mi.util.write_bitmap(filename, etc_fwd)
        filename = join(os.getcwd(), f"test_{integrator_name}_{config.name}_image_error.exr")
        print(f'-> write error image: {filename}')
        mi.util.write_bitmap(filename, error)
        pytest.fail("Gradient values exceeded configuration's tolerances!")


@pytest.mark.slow
@pytest.mark.skipif(os.name == 'nt', reason='Skip those memory heavy tests on Windows')
@pytest.mark.parametrize('integrator_name, config', CONFIGS_BACKWARD)
def test03_rendering_backward(variants_all_ad_acoustic, integrator_name, config):
    config = config()
    config.initialize()

    config.integrator_dict['type'] = integrator_name
    integrator = mi.load_dict(config.integrator_dict)

    filename = join(output_dir, f"test_{config.name}_etc_fwd_ref.exr")
    etc_fwd_ref = mi.TensorXf(mi.Bitmap(filename))

    grad_in = 0.001
    etc_adj = dr.full(mi.TensorXf, grad_in, etc_fwd_ref.shape)

    theta = mi.Float(0.0)
    dr.enable_grad(theta)
    dr.set_label(theta, 'theta')
    config.update(theta)

    integrator.render_backward(config.scene, grad_in=etc_adj, seed=0, spp=config.spp, params=theta)

    grad = dr.grad(theta) / dr.width(etc_fwd_ref) / config.spp
    grad_ref = dr.mean(etc_fwd_ref, axis=None) * grad_in

    error = dr.abs(grad - grad_ref) / dr.maximum(dr.abs(grad_ref), 1e-3)
    if error > config.error_mean_threshold_bwd:
        print(f"Failure in config: {config.name}, {integrator_name}")
        print(f"-> grad:     {grad}")
        print(f"-> grad_ref: {grad_ref}")
        print(f"-> error: {error} (threshold={config.error_mean_threshold_bwd})")
        print(f"-> ratio: {grad / grad_ref}")
        pytest.fail("Gradient values exceeded configuration's tolerances!")

# -------------------------------------------------------------------
#                      Generate reference images
# -------------------------------------------------------------------

if __name__ == "__main__":
    """
    Generate reference primal/forward ETCs for all configs.
    """
    parser = argparse.ArgumentParser(prog='GenerateConfigReferenceETCs')
    parser.add_argument('--spp', default=2147483647, type=int,
                        help='Samples per pixel. Default value: floor(2**32-1 / 2), which maxes out the spp')
    args = parser.parse_args()

    mi.set_variant('cuda_ad_acoustic', 'llvm_ad_acoustic')

    if not exists(output_dir):
        os.makedirs(output_dir)

    for config in BASIC_CONFIGS_LIST + DISCONTINUOUS_CONFIGS_LIST:
        config = config()
        print(f"name: {config.name}")

        config.initialize()

        integrator_path = mi.load_dict({
            'type': 'acoustic_ad',  # TODO: change this to acousticpath once implemented
            'speed_of_sound': config.speed_of_sound,
            'max_depth': config.integrator_dict['max_depth'],
            'max_time': config.max_time,
        })

        # Primal render
        etc_ref = integrator_path.render(config.scene, seed=0, spp=args.spp) / args.spp

        filename = join(output_dir, f"test_{config.name}_etc_primal_ref.exr")
        mi.util.write_bitmap(filename, etc_ref)

        # Finite difference
        theta = mi.Float(-0.5 * config.ref_fd_epsilon)
        config.update(theta)
        etc_1 = integrator_path.render(config.scene, seed=0, spp=args.spp) / args.spp
        dr.eval(etc_1)

        theta = mi.Float(0.5 * config.ref_fd_epsilon)
        config.update(theta)
        etc_2 = integrator_path.render(config.scene, seed=0, spp=args.spp) / args.spp
        dr.eval(etc_2)

        image_fd = (etc_2 - etc_1) / config.ref_fd_epsilon

        filename = join(output_dir, f"test_{config.name}_etc_fwd_ref.exr")
        mi.util.write_bitmap(filename, image_fd)
