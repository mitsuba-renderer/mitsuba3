# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
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

from misuka.scalar_acoustic import ScalarTransform4f as T
import drjit as dr
import misuka as mi

import numpy as np
import pytest
import os
import argparse

from os.path import join, exists

from misuka.scalar_rgb.test.util import fresolver_append_path
from misuka.scalar_rgb.test.util import find_resource

# Cross-backend equivalence helpers live in test_acoustic_path.py.
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from test_acoustic_path import (
    PRIMAL_PAIR, AD_PAIR, require_pair, render_on_variant,
    assert_direct_sound_present, assert_etc_equivalent, assert_grad_equivalent,
)

output_dir = find_resource('resources/data_acoustic/tests/integrators')

# Directory next to this test file where rendered ETCs and reference copies are
# written on failure, so they can be inspected/plotted (see plot_etcs.py).
tests_dir = os.path.dirname(os.path.abspath(__file__))


# -------------------------------------------------------------------
#                          initialization tests
# -------------------------------------------------------------------

# List of integrators to test

INTEGRATORS = [
    'acoustic_ad',
    'acoustic_ad_threepoint',
    'acoustic_prb',
    'acoustic_prb_threepoint'
]

@pytest.mark.parametrize('integrator_name', INTEGRATORS)
def test01_initialization(variants_all_jit_acoustic, integrator_name):
    integrator = mi.load_dict({'type': integrator_name,
                               'max_time': 1.0,
                               'speed_of_sound': 343.0}, parallel=False)
    assert isinstance(integrator, mi.CppADIntegrator)

    with pytest.raises(RuntimeError, match='max_time'):
        mi.load_dict({'type': integrator_name, 'max_time': -1})

@pytest.mark.parametrize('integrator_name', INTEGRATORS)
def test02_constructor_default_values(variants_all_jit_acoustic, integrator_name):
    """Test that default property values are set correctly."""
    integrator = mi.load_dict({'type': integrator_name, 'max_time': 1.0})

    assert integrator.speed_of_sound == 343.0
    assert integrator.max_time == 1.0
    assert integrator.is_detached
    assert not integrator.hide_emitters
    assert integrator.track_time_derivatives
    assert integrator.max_depth == 0xffffffff  # -1 maps to 2^32-1
    assert integrator.rr_depth == 100000
    assert dr.allclose(integrator.throughput_threshold, 10 ** (-60.0 / 10.0))

@pytest.mark.parametrize('integrator_name', INTEGRATORS)
def test03_constructor_custom_values(variants_all_jit_acoustic, integrator_name):
    """Test that custom property values are accepted and stored."""
    integrator = mi.load_dict({
        'type': integrator_name,
        'max_time': 5.0,
        'speed_of_sound': 100.0,
        'max_depth': 10,
        'rr_depth': 100000,
        'is_detached': False,
        'hide_emitters': True,
        'track_time_derivatives': False,
        'max_energy_loss': 60.0,
    })

    assert integrator.max_time == 5.0
    assert integrator.speed_of_sound == 100.0
    assert integrator.max_depth == 10
    assert integrator.rr_depth == 100000
    assert not integrator.is_detached
    assert integrator.hide_emitters
    assert not integrator.track_time_derivatives
    assert dr.allclose(integrator.throughput_threshold, 10 ** (-60.0 / 10.0))

@pytest.mark.parametrize('integrator_name', INTEGRATORS)
def test04_constructor_max_time_missing(variants_all_jit_acoustic, integrator_name):
    """max_time is required and must raise ValueError if missing."""
    with pytest.raises(RuntimeError, match='max_time'):
        mi.load_dict({'type': integrator_name})

@pytest.mark.parametrize('integrator_name', INTEGRATORS)
def test05_constructor_max_time_zero(variants_all_jit_acoustic, integrator_name):
    """max_time=0 must raise ValueError."""
    with pytest.raises(RuntimeError, match='max_time'):
        mi.load_dict({'type': integrator_name, 'max_time': 0.0})

@pytest.mark.parametrize('integrator_name', INTEGRATORS)
def test06_constructor_speed_of_sound_invalid(variants_all_jit_acoustic, integrator_name):
    """speed_of_sound <= 0 must raise ValueError."""
    with pytest.raises(RuntimeError, match='speed_of_sound'):
        mi.load_dict({'type': integrator_name, 'max_time': 1.0, 'speed_of_sound': 0.0})
    with pytest.raises(RuntimeError, match='speed_of_sound'):
            mi.load_dict({'type': integrator_name, 'max_time': 1.0, 'speed_of_sound': -1.0})

@pytest.mark.parametrize('integrator_name', INTEGRATORS)
def test07_constructor_max_depth_invalid(variants_all_jit_acoustic, integrator_name):
    """max_depth < -1 must raise an exception, but -1 and 0 are valid."""
    with pytest.raises(Exception):
        mi.load_dict({'type': integrator_name, 'max_time': 1.0, 'max_depth': -2})

    # max_depth=-1 (infinite) is valid
    integrator = mi.load_dict({'type': integrator_name, 'max_time': 1.0, 'max_depth': -1})
    assert integrator.max_depth == 0xffffffff

    # max_depth=0 is valid
    integrator = mi.load_dict({'type': integrator_name, 'max_time': 1.0, 'max_depth': 0})
    assert integrator.max_depth == 0

@pytest.mark.parametrize('integrator_name', INTEGRATORS)
def test08_constructor_rr_depth_invalid(variants_all_jit_acoustic, integrator_name):
    """rr_depth <= 0 must raise an exception, and Russian roulette is not yet
    implemented, so any non-default rr_depth must also raise."""
    with pytest.raises(Exception):
        mi.load_dict({'type': integrator_name, 'max_time': 1.0, 'rr_depth': 0})
    with pytest.raises(Exception):
            mi.load_dict({'type': integrator_name, 'max_time': 1.0, 'rr_depth': -1})
    with pytest.raises(Exception):
        mi.load_dict({'type': integrator_name, 'max_time': 1.0, 'rr_depth': 50})

@pytest.mark.parametrize('integrator_name', INTEGRATORS)
def test09_constructor_max_energy_loss_invalid(variants_all_jit_acoustic, integrator_name):
    """max_energy_loss < 0 (and not -1) must raise ValueError."""
    with pytest.raises(RuntimeError, match='max_energy_loss'):
        mi.load_dict({'type': integrator_name, 'max_time': 1.0, 'max_energy_loss': -2.0})
    with pytest.raises(RuntimeError, match='max_energy_loss'):
        mi.load_dict({'type': integrator_name, 'max_time': 1.0, 'max_energy_loss': 0.0})

    # -1 (disabled) is valid
    integrator = mi.load_dict({'type': integrator_name, 'max_time': 1.0, 'max_energy_loss': -1.0})
    assert integrator.throughput_threshold == 0.0
    integrator = mi.load_dict({'type': integrator_name, 'max_time': 1.0, 'max_energy_loss': 123})
    assert integrator.throughput_threshold == 10 ** (-123 / 10.0)


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
        self.speed_of_sound = 340
        self.max_time = 0.2
        self.max_depth = -1
        self.rr_depth = 100000
        self.max_energy_loss = 20
        self.sampling_rate = 1000.0
        self.frequencies = '250, 500'
        self.error_mean_threshold = 0.05
        self.error_max_threshold = 0.5
        self.error_mean_threshold_bwd = 0.05
        self.ref_fd_epsilon = 1e-3
        self.emitter_radius = 0.5

        self.integrator_dict = {
            'speed_of_sound': self.speed_of_sound,
            'max_depth': self.max_depth,
            'max_time': self.max_time,
            'max_energy_loss': self.max_energy_loss,
            'rr_depth': self.rr_depth,
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
                        'value': 1,
                    },
                },
            },
        }

class ShoeboxAbsorptionConfig(ConfigBase):
    """
    Absorption coefficient of the shoebox room.
    """

    def __init__(self) -> None:
        super().__init__()
        self.key = 'shoebox.bsdf.absorption.values'
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
                        'type': 'spectrum',
                        'value': [(250, 0.4), (500, 0.6)],
                    },
                    'scattering': {
                        'type': 'spectrum',
                        'value': [(250, 0.1), (500, 0.9)],
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


class ShoeboxScatteringConfig(ConfigBase):
    """
    Scattering coefficient of the shoebox room.
    """

    def __init__(self) -> None:
        super().__init__()
        self.key = 'shoebox.bsdf.scattering.values'
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
                        'type': 'spectrum',
                        'value': [(250, 0.4), (500, 0.6)],
                    },
                    'scattering': {
                        'type': 'spectrum',
                        'value': [(250, 0.1), (500, 0.9)],
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
#            Test configs with discontinuities
# -------------------------------------------------------------------

# -------------------------------------------------------------------
#                           List configs
# -------------------------------------------------------------------
BASIC_CONFIGS_LIST = [
    ShoeboxAbsorptionConfig,
    ShoeboxScatteringConfig,
    SphericalEmitterRadianceConfig,
]

DISCONTINUOUS_CONFIGS_LIST = [
]

# List of configs that fail on integrators with depth less than three
INDIRECT_ILLUMINATION_CONFIGS_LIST = [
]

# List of integrators to test
# (Name, handles discontinuities, has render_backward, has render_forward)
INTEGRATORS_RENDER = [
    ('acoustic_ad',             False,  True,   True),
    ('acoustic_prb',            False,  True,   False),
    ('acoustic_ad_threepoint',  True,   True,   True),
    ('acoustic_prb_threepoint', True,   True,   False)
]

CONFIGS_PRIMAL   = []
CONFIGS_BACKWARD = []
CONFIGS_FORWARD  = []
for integrator_name, handles_discontinuities, has_render_backward, has_render_forward in INTEGRATORS_RENDER:
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

# Pairs of (AD integrator, matching PRB integrator). They are two
# implementations of the same gradient estimator and must agree numerically
# when driven with the same scene, seed, and spp.
AD_PRB_PAIRS = [
    ('acoustic_ad',            'acoustic_prb'),
    ('acoustic_ad_threepoint', 'acoustic_prb_threepoint'),
]

CONFIGS_AD_PRB = [
    (ad, prb, cfg)
    for ad, prb in AD_PRB_PAIRS
    for cfg in BASIC_CONFIGS_LIST
]



# -------------------------------------------------------------------
#                           Unit tests
# -------------------------------------------------------------------


@pytest.mark.slow
@pytest.mark.parametrize('integrator_name, config', CONFIGS_PRIMAL)
def test10_rendering_primal(variants_all_ad_acoustic, integrator_name, config):
    config = config()
    config.initialize()

    config.integrator_dict['type'] = integrator_name
    integrator = mi.load_dict(config.integrator_dict, parallel=False)
    filename = join(output_dir, f"test_{config.name}_primal_ref.exr")
    etc_primal_ref = mi.TensorXf(mi.Bitmap(filename))
    etc = integrator.render(config.scene, seed=0, spp=config.spp)

    # FIXME: once the integrators normalize by spp, remove this normalization.
    etc /= dr.max(dr.abs(etc))
    etc_primal_ref /= dr.max(dr.abs(etc_primal_ref))

    error = dr.abs(etc - etc_primal_ref) / dr.maximum(dr.abs(etc_primal_ref), 2e-2)
    error_mean = dr.mean(error, axis=None)
    error_max = dr.max(error, axis=None)

    if error_mean > config.error_mean_threshold or error_max > config.error_max_threshold:
        print(f"Failure in config: {config.name}, {integrator_name}")
        print(f"-> error mean: {error_mean} (threshold={config.error_mean_threshold})")
        print(f"-> error max: {error_max} (threshold={config.error_max_threshold})")
        print(f'-> reference image: {filename}')
        filename = join(tests_dir, f"test_{integrator_name}_{config.name}_primal.exr")
        filename_ref = join(tests_dir, f"test_{integrator_name}_{config.name}_ref.exr")
        print(f'-> write current image: {filename}')
        mi.util.write_bitmap(filename, etc)
        mi.util.write_bitmap(filename_ref, etc_primal_ref)
        pytest.fail("ETC values exceeded configuration's tolerances!")


def _finite_depth_scene(scattering, time_bins=40):
    """A small enclosed shoebox with a flat (mesh-based) emitter and an
    omnidirectional microphone, used to exercise the direct sound and
    first-order reflections at small, *finite* path depths.

    Deliberately uses a ``rectangle`` emitter rather than a ``sphere``: while
    investigating this bug, driving the JIT (``llvm_ad_acoustic``) acoustic
    integrators against a *sphere* emitter (Embree "user geometry", traced via
    the per-shape ``ray_intersect_preliminary_packet`` callback in
    ``src/render/embree.h``) triggered an intermittent, pre-existing
    segfault inside Embree's packet intersector for user geometry. That crash:
      * reproduces with this fix stashed out (i.e. it predates and is
        unrelated to the ordering bug fixed here),
      * lives entirely in generic, unmodified upstream Mitsuba/Embree
        infrastructure (``src/render/embree.h``, ``src/shapes/sphere.cpp``
        have no misuka-specific changes),
      * appears to be a timing-sensitive data race (its trigger rate was not a
        clean function of scene parameters), consistent with concurrent
        access to a shape from multiple Embree/nanothread worker threads.
    A ``rectangle`` is internally a two-triangle ``Mesh`` and is intersected
    through Embree's native triangle geometry path instead, which does not
    exercise the affected callback; it reproduces the reported bug just as
    well (any smooth emitter shape triggers the missing-emitter-hit bug) and
    was empirically stable across many repeated runs. The sphere/Embree crash
    is a separate, serious finding that deserves its own follow-up
    investigation and is out of scope for the specular/direct-sound fixes
    made here.
    """
    return {
        'type': 'scene',
        'shoebox': {
            'type': 'cube',
            'flip_normals': True,
            'to_world': T().scale([7, 5, 3]),
            'bsdf': {
                'type': 'acousticbsdf',
                'specular_lobe_width': 0.001,
                'absorption': {'type': 'spectrum', 'value': [(250, 0.1), (500, 0.1)]},
                'scattering': {'type': 'spectrum', 'value': [(250, scattering), (500, scattering)]},
            },
        },
        'rect_emitter': {
            'type': 'rectangle',
            'to_world': T().translate([2, 0, 0]).rotate(axis=[0, 1, 0], angle=90).scale(0.5),
            'flip_normals': True,  # face the rectangle towards the microphone
            'emitter': {'type': 'area', 'radiance': {'type': 'uniform', 'value': 1}},
        },
        'sensor': {
            'type': 'microphone',
            'origin': [-2, 0, 0],
            'direction': [1, 0, 0],
            'kappa': 0,
            'film': {
                'type': 'tape',
                'rfilter': {'type': 'box'},
                'sample_border': False,
                'pixel_format': 'MultiChannel',
                'component_format': 'float32',
                'frequencies': '250, 500',
                'time_bins': time_bins,
            },
        },
    }


def test10b_finite_depth_emitter_hits(variant_llvm_ad_acoustic):
    """Regression test: the AD/PRB integrators must record the direct-emission
    contribution (``Le``) whenever a traced ray hits an emitter, *including* at
    the terminal path depth.

    A bug in the PRB integrators gated the ``Le`` splat on the ``active_next``
    mask *after* it had been reduced by ``depth + 1 < max_depth``. That silently
    dropped:
      * the direct (line-of-sight) sound when ``max_depth == 1``, and
      * specular reflections captured by BSDF sampling at the final bounce.
        Specular energy reaches a point microphone almost exclusively through
        such emitter hits (next-event estimation contributes ~nothing for a
        near-delta lobe), so the specular component of the ETC went missing.

    The existing primal tests only use ``max_depth == -1`` (infinite), where the
    terminal depth is never reached, so they could not catch this. Here we use
    small finite depths and compare against the reference ``acoustic_path``
    integrator, which handles these contributions correctly.

    The check is restricted to a single JIT backend on purpose: the bug lives in
    backend-independent Python code, and driving several of these Python
    integrators across multiple variants in one process trips an unrelated,
    pre-existing crash (see the notes in the accompanying fix).
    """
    spp = 2 ** 14
    seed = 0

    def render(itype, max_depth, scattering):
        scene = mi.load_dict(_finite_depth_scene(scattering))
        integrator = mi.load_dict({
            'type': itype,
            'speed_of_sound': 340,
            'max_time': 0.2,
            'max_depth': max_depth,
            'max_energy_loss': -1,
        }, parallel=False)
        return integrator.render(scene, seed=seed, spp=spp)

    # Reference contributions from the (correct) acoustic_path integrator.
    ref_direct = dr.sum(render('acoustic_path', 1, 0.05), axis=None)   # direct sound
    ref_spec   = dr.sum(render('acoustic_path', 2, 0.05), axis=None)   # + specular refl.

    # The specular first-order reflection must add meaningful energy on top of
    # the direct sound, otherwise there is nothing to detect as "missing".
    assert ref_spec > 1.1 * ref_direct

    for integrator_name in ('acoustic_prb', 'acoustic_prb_threepoint'):
        # max_depth == 1: direct sound only. Must be non-zero (was 0.0) and
        # match the reference.
        etc_direct = render(integrator_name, 1, 0.05)
        assert dr.max(dr.abs(etc_direct)) > 0.0, \
            f"{integrator_name}: direct sound missing at max_depth=1"
        assert dr.allclose(dr.sum(etc_direct, axis=None), ref_direct, rtol=1e-2), \
            f"{integrator_name}: direct sound energy disagrees with acoustic_path"

        # max_depth == 2, low scattering: the first-order reflection is almost
        # purely specular and is captured via an emitter hit at the terminal
        # bounce. Must match the reference (was ~missing before the fix).
        etc_spec = dr.sum(render(integrator_name, 2, 0.05), axis=None)
        assert dr.allclose(etc_spec, ref_spec, rtol=2e-2), \
            f"{integrator_name}: specular first reflection disagrees with acoustic_path"
        assert etc_spec > 1.1 * dr.sum(etc_direct, axis=None), \
            f"{integrator_name}: specular first reflection contributes no energy"


@pytest.mark.slow
@pytest.mark.skip(reason="Gradient estimation will be tested in a future PR.")
@pytest.mark.skipif(os.name == 'nt', reason='Skip those memory heavy tests on Windows')
@pytest.mark.parametrize('integrator_name, config', CONFIGS_FORWARD)
def test11_rendering_forward(variants_all_ad_acoustic, integrator_name, config):
    config = config()
    config.initialize()

    config.integrator_dict['type'] = integrator_name
    integrator = mi.load_dict(config.integrator_dict)


    filename = join(output_dir, f"test_{config.name}_fwd_ref.exr")
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
        filename = join(tests_dir, f"test_{integrator_name}_{config.name}_image_fwd.exr")
        print(f'-> write current image: {filename}')
        mi.util.write_bitmap(filename, etc_fwd)
        filename = join(tests_dir, f"test_{integrator_name}_{config.name}_image_error.exr")
        print(f'-> write error image: {filename}')
        mi.util.write_bitmap(filename, error)
        pytest.fail("Gradient values exceeded configuration's tolerances!")


@pytest.mark.slow
@pytest.mark.skip(reason="Gradient estimation will be tested in a future PR.")
@pytest.mark.skipif(os.name == 'nt', reason='Skip those memory heavy tests on Windows')
@pytest.mark.parametrize('integrator_name, config', CONFIGS_BACKWARD)
def test12_rendering_backward(variants_all_ad_acoustic, integrator_name, config):
    config = config()
    config.initialize()
    config.integrator_dict['type'] = integrator_name
    integrator = mi.load_dict(config.integrator_dict)

    filename = join(output_dir, f"test_{config.name}_fwd_ref.exr")
    etc_fwd_ref = mi.TensorXf(mi.Bitmap(filename))

    # FIXME: remove this normalization once the integrators normalize by spp.
    #FIXME: use value used to generate the reference ETC in the config, not necessarily 2**30.
    etc_fwd_ref *= config.spp / 2**30

    grad_in = 0.001
    etc_adj = dr.full(mi.TensorXf, grad_in, etc_fwd_ref.shape)

    theta = mi.Float(0.0)
    dr.enable_grad(theta)
    dr.set_label(theta, 'theta')
    config.update(theta)

    integrator.render_backward(config.scene, grad_in=etc_adj, seed=0, spp=config.spp, params=theta)

    grad = dr.grad(theta)
    grad_ref = dr.mean(etc_fwd_ref, axis=None) * grad_in
    if dr.isnan(grad):
        print(f"Failure in config: {config.name}, {integrator_name}")
        print(f"-> grad: {grad}")
        pytest.fail("Gradient is NaN!")

    if dr.isinf(grad):
        print(f"Failure in config: {config.name}, {integrator_name}")
        print(f"-> grad: {grad}")
        pytest.fail("Gradient is Inf!")
    error = dr.abs(grad - grad_ref) / dr.maximum(dr.abs(grad_ref), 1e-3)
    if error > config.error_mean_threshold_bwd:
        print(f"Failure in config: {config.name}, {integrator_name}")
        print(f"-> grad:     {grad}")
        print(f"-> grad_ref: {grad_ref}")
        print(f"-> error: {error} (threshold={config.error_mean_threshold_bwd})")
        print(f"-> ratio: {grad / grad_ref}")
        pytest.fail("Gradient values exceeded configuration's tolerances!")


@pytest.mark.skip(reason="Gradient estimation will be tested in a future PR.")
@pytest.mark.slow
@pytest.mark.skipif(os.name == 'nt', reason='Skip those memory heavy tests on Windows')
@pytest.mark.parametrize('ad_name, prb_name, config', CONFIGS_AD_PRB)
def test13_ad_prb_equivalence(variants_all_ad_acoustic, ad_name, prb_name, config):
    """AD and PRB integrators should compute the same gradient for the same
    scene, seed, and spp."""

    # Same shape as the reference ETC used by test12 — we only need it to size
    # the adjoint buffer, not its values.
    config_cls = config
    ref_filename = join(output_dir, f"test_{config_cls().name}_fwd_ref.exr")
    etc_fwd_ref = mi.TensorXf(mi.Bitmap(ref_filename))

    spp = 1
    seed = 0
    grad_in = 0.001
    etc_adj = dr.full(mi.TensorXf, grad_in, etc_fwd_ref.shape)

    def run(integrator_name):
        cfg = config_cls()
        cfg.spp = spp
        cfg.initialize()
        cfg.integrator_dict['type'] = integrator_name
        integrator = mi.load_dict(cfg.integrator_dict)

        theta = mi.Float(0.0)
        dr.enable_grad(theta)
        dr.set_label(theta, 'theta')
        cfg.update(theta)

        integrator.render_backward(
            cfg.scene, grad_in=etc_adj, seed=seed, spp=spp, params=theta)
        return dr.grad(theta)

    grad_ad  = run(ad_name)
    grad_prb = run(prb_name)

    if not dr.allclose(grad_ad, grad_prb, rtol=1e-4, atol=1e-6):
        print(f"Failure in config: {config_cls().name}")
        print(f"-> {ad_name}  grad: {grad_ad}")
        print(f"-> {prb_name} grad: {grad_prb}")
        diff = dr.abs(grad_ad - grad_prb)
        rel  = diff / dr.maximum(dr.abs(grad_ad), 1e-12)
        print(f"-> abs diff: {diff}")
        print(f"-> rel diff: {rel}")
        pytest.fail(f"{ad_name} and {prb_name} disagree on gradient!")


# -------------------------------------------------------------------
#            LLVM <-> Metal equivalence (forward, then backward)
# -------------------------------------------------------------------

# Integrators whose primal render / backward gradient are compared across
# backends. Forward-mode is compared for `acoustic_ad` only (see below).
EQUIV_INTEGRATORS = ['acoustic_ad', 'acoustic_prb']

EQUIV_SEED = 0
EQUIV_SPP  = 2 ** 16
EQUIV_GRAD_IN = 0.001


@pytest.mark.slow
@pytest.mark.parametrize('integrator_name', EQUIV_INTEGRATORS)
@pytest.mark.parametrize('config', BASIC_CONFIGS_LIST)
def test14_llvm_metal_equivalence_primal(integrator_name, config):
    """Primal ETCs must match across LLVM and Metal, direct sound included."""
    require_pair(AD_PAIR)
    name = config().name

    def build_and_render():
        cfg = config()
        cfg.initialize()
        cfg.integrator_dict['type'] = integrator_name
        integrator = mi.load_dict(cfg.integrator_dict)
        result = integrator.render(cfg.scene, seed=EQUIV_SEED, spp=EQUIV_SPP)
        dr.eval(result)
        return result

    ref  = render_on_variant(AD_PAIR[0], build_and_render)  # llvm
    test = render_on_variant(AD_PAIR[1], build_and_render)  # metal

    label = f"{integrator_name}/{name} (primal)"
    assert_direct_sound_present(ref, test, label=label)
    assert_etc_equivalent(ref, test, label=label)


@pytest.mark.slow
@pytest.mark.parametrize('integrator_name', EQUIV_INTEGRATORS)
@pytest.mark.parametrize('config', BASIC_CONFIGS_LIST)
def test15_llvm_metal_equivalence_backward(integrator_name, config):
    """Backward gradients w.r.t. the scene parameter must match across backends."""
    require_pair(AD_PAIR)
    name = config().name

    def compute_grad():
        cfg = config()
        cfg.spp = EQUIV_SPP
        cfg.initialize()
        cfg.integrator_dict['type'] = integrator_name
        integrator = mi.load_dict(cfg.integrator_dict)

        # Size the adjoint buffer from a cheap primal render.
        shape = integrator.render(cfg.scene, seed=EQUIV_SEED, spp=1).shape
        etc_adj = dr.full(mi.TensorXf, EQUIV_GRAD_IN, shape)

        theta = mi.Float(0.0)
        dr.enable_grad(theta)
        dr.set_label(theta, 'theta')
        cfg.update(theta)

        integrator.render_backward(cfg.scene, grad_in=etc_adj, seed=EQUIV_SEED,
                                   spp=EQUIV_SPP, params=theta)
        grad = dr.grad(theta)
        dr.eval(grad)
        return grad

    grad_llvm  = render_on_variant(AD_PAIR[0], compute_grad)
    grad_metal = render_on_variant(AD_PAIR[1], compute_grad)

    assert_grad_equivalent(grad_llvm, grad_metal,
                           label=f"{integrator_name}/{name} (backward)")


@pytest.mark.slow
@pytest.mark.parametrize('config', BASIC_CONFIGS_LIST)
def test16_llvm_metal_equivalence_forward(config):
    """Forward-mode gradient ETCs must match across backends (acoustic_ad only;
    acoustic_prb has no render_forward)."""
    require_pair(AD_PAIR)
    name = config().name

    def compute_forward():
        cfg = config()
        cfg.spp = EQUIV_SPP
        cfg.initialize()
        cfg.integrator_dict['type'] = 'acoustic_ad'
        integrator = mi.load_dict(cfg.integrator_dict)

        theta = mi.Float(0.0)
        dr.enable_grad(theta)
        dr.set_label(theta, 'theta')
        cfg.update(theta)
        # Push the gradient from theta into the scene parameters before
        # render_forward so the forward AD traversal inside the loop sees it
        # (mirrors the wiring of the skipped test11).
        dr.forward(theta, dr.ADFlag.ClearEdges)

        etc_fwd = integrator.render_forward(cfg.scene, seed=EQUIV_SEED,
                                            spp=EQUIV_SPP, params=theta)
        result = dr.detach(etc_fwd)
        dr.eval(result)
        return result

    ref  = render_on_variant(AD_PAIR[0], compute_forward)  # llvm
    test = render_on_variant(AD_PAIR[1], compute_forward)  # metal

    # Forward-mode gradients are signed and may contain near-cancellations, so
    # compare with the relative+floor ETC helper (looser than primal) rather than
    # the direct-sound peak check.
    assert_etc_equivalent(ref, test, label=f"acoustic_ad/{name} (forward)",
                          rtol=1e-1, atol_frac=5e-2)


# -------------------------------------------------------------------
#                      Generate reference images
# -------------------------------------------------------------------

if __name__ == "__main__":
    """
    Generate reference primal/forward ETCs for all configs.
    """
    parser = argparse.ArgumentParser(prog='GenerateConfigReferenceETCs')
    parser.add_argument('--spp', default=2**30, type=int,
                        help='Samples per pixel. Default value: 2**30.')
    args = parser.parse_args()

    if args.spp != 2**30:
        raise ValueError("Normalization is hardcoded in the tests. If you want to generate reference ETCs with a different spp, please update the normalization in the tests accordingly. This will be fixed once the integrators normalize by spp.")

    mi.set_variant('cuda_acoustic', 'llvm_acoustic')

    if not exists(output_dir):
        os.makedirs(output_dir)

    for config in BASIC_CONFIGS_LIST + DISCONTINUOUS_CONFIGS_LIST:
        config = config()
        print(f"name: {config.name}")

        config.initialize()

        integrator_path = mi.load_dict({
            'type': 'acoustic_path',
            'speed_of_sound': config.speed_of_sound,
            'max_depth': config.max_depth,
            'max_time': config.max_time,
            'max_energy_loss': config.max_energy_loss,
        })

        # Primal render
        etc_ref = integrator_path.render(config.scene, seed=0, spp=args.spp)

        filename = join(output_dir, f"test_{config.name}_primal_ref.exr")
        mi.util.write_bitmap(filename, etc_ref)

        # Finite difference
        theta = mi.Float(-0.5 * config.ref_fd_epsilon)
        config.update(theta)
        etc_1 = integrator_path.render(config.scene, seed=0, spp=args.spp)
        dr.eval(etc_1)

        theta = mi.Float(0.5 * config.ref_fd_epsilon)
        config.update(theta)
        etc_2 = integrator_path.render(config.scene, seed=0, spp=args.spp)
        dr.eval(etc_2)

        etc_fd = (etc_2 - etc_1) / config.ref_fd_epsilon

        filename = join(output_dir, f"test_{config.name}_fwd_ref.exr")
        mi.util.write_bitmap(filename, etc_fd)
