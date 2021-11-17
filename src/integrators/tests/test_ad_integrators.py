"""
Overview
--------

This file defines a set of unit tests to assess the correctness of the
different adjoint integrators such as `rb`, `prb`, etc... All integrators will
be tested for their implementation of primal rendering, adjoint forward
rendering and adjoint backward rendering.

- For primal rendering, the output image will be compared to a groundtruth image
  precomputed in the `resources/data/tests/integrators` directory.
- Adjoint forward rendering will be compared against finite differences.
- Adjoint backward rendering will be compared against finite differences.

Those tests will be run on a set of configurations (scene description + metadata)
also provided in this file. More tests can easily be added by creating a new
configuration type and at it to the *_CONFIGS_LIST below.

By executing this script with python directly it is possible to regenerate the
reference data (e.g. for a new configurations). Please see the following command:

``python3 test_ad_integrators.py --help``

"""

import mitsuba
import enoki as ek

import pytest, sys, inspect, os, argparse
from os.path import join, realpath, exists

from mitsuba.python.test.util import fresolver_append_path

output_dir = realpath(join(os.path.dirname(__file__), '../../../resources/data/tests/integrators'))

# -------------------------------------------------------------------
#                          Test configs
# -------------------------------------------------------------------

mitsuba.set_variant('scalar_rgb')
from mitsuba.core import ScalarTransform4f as T

class ConfigBase:
    """
    Base class to configure test scene and define the parameter to update
    """
    require_reparameterization = False

    def __init__(self) -> None:
        self.spp = 1024
        self.res = 32
        self.max_depth = 3
        self.error_mean_threshold = 0.05
        self.error_max_threshold = 0.2
        self.ref_fd_epsilon = 1e-2

        self.sensor_dict = {
            'type': 'perspective',
            'to_world': T.look_at(origin=[0, 0, 4], target=[0, 0, 0], up=[0, 1, 0]),
            'film': {
                'type': 'hdrfilm',
                'rfilter': { 'type': 'gaussian', 'stddev': 0.5 },
                'width': self.res,
                'height': self.res,
                'high_quality_edges': True,
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
        from mitsuba.core import xml
        from mitsuba.python.util import traverse

        self.sensor_dict['film']['width'] = self.res
        self.sensor_dict['film']['height'] = self.res
        self.scene_dict['camera'] = self.sensor_dict

        @fresolver_append_path
        def create_scene():
            return xml.load_dict(self.scene_dict)
        self.scene = create_scene()
        self.params = traverse(self.scene)

        if hasattr(self, 'key'):
            self.params.keep([self.key])
            self.initial_state = type(self.params[self.key])(self.params[self.key])

    def update(self, theta):
        """
        This method update the scene parameter associated to this config
        """
        self.params[self.key] = self.initial_state + theta
        ek.set_label(self.params, 'params')
        self.params.update()
        ek.eval()

# BSDF albedo of a directly visible gray plane illuminated by a constant emitter
class DiffuseAlbedoConfig(ConfigBase):
    def __init__(self) -> None:
        super().__init__()
        self.key = 'plane.bsdf.reflectance.value'
        self.scene_dict = {
            'type': 'scene',
            'plane': {
                'type': 'rectangle',
                'bsdf': { 'type': 'diffuse' }
            },
            'sphere': {
                'type': 'sphere',
                'to_world': T.scale(0.25),
            },
            'light': { 'type': 'constant' }
        }

# BSDF albedo of a off camera plane blending onto a directly visible gray plane
class DiffuseAlbedoGIConfig(ConfigBase):
    def __init__(self) -> None:
        super().__init__()
        self.key = 'green.bsdf.reflectance.value'
        self.scene_dict = {
            'type': 'scene',
            'plane': { 'type': 'rectangle' },
            'sphere': {
                'type': 'sphere',
                'to_world': T.scale(0.25)
            },
            'green': {
                'type': 'rectangle',
                'bsdf': {
                    'type': 'diffuse',
                    'reflectance': {
                        'type': 'rgb',
                        'value': [0.1, 1.0, 0.1]
                    }
                },
                'to_world': T.translate([1.25, 0.0, 1.0]) * T.rotate([0, 1, 0], -90),
            },
            'light': { 'type': 'constant' }
        }

# Off camera area light illuminating a gray plane
class AreaLightRadianceConfig(ConfigBase):
    def __init__(self) -> None:
        super().__init__()
        self.key = 'light.emitter.radiance.value'
        self.scene_dict = {
            'type': 'scene',
            'plane': {
                'type': 'rectangle',
                'bsdf': { 'type': 'diffuse' }
            },
            'sphere': {
                'type': 'sphere',
                'to_world': T.scale(0.25),
            },
            'light': {
                'type': 'rectangle',
                'to_world': T.translate([1.25, 0.0, 1.0]) * T.rotate([0, 1, 0], -90),
                'emitter': {
                    'type': 'area',
                    'radiance': {'type': 'rgb', 'value': [1.0, 1.0, 1.0]}
                }
            }
        }

# Directly visible area light illuminating a gray plane
class DirectlyVisibleAreaLightRadianceConfig(ConfigBase):
    def __init__(self) -> None:
        super().__init__()
        self.key = 'light.emitter.radiance.value'
        self.scene_dict = {
            'type': 'scene',
            'light': {
                'type': 'rectangle',
                'emitter': {
                    'type': 'area',
                    'radiance': {'type': 'rgb', 'value': [1.0, 1.0, 1.0]}
                }
            }
        }

# Off camera point light illuminating a gray plane
class PointLightIntensityConfig(ConfigBase):
    def __init__(self) -> None:
        super().__init__()
        self.key = 'light.intensity.value'
        self.scene_dict = {
            'type': 'scene',
            'plane': {
                'type': 'rectangle',
                'bsdf': { 'type': 'diffuse' }
            },
            'sphere': {
                'type': 'sphere',
                'to_world': T.scale(0.25),
            },
            'light': {
                'type': 'point',
                'position': [1.25, 0.0, 1.0],
                'intensity': {'type': 'rgb', 'value': [1.0, 1.0, 1.0]}
            },
        }

# Instensity of a constant emitter illuminating a gray rectangle
class ConstantEmitterRadianceConfig(ConfigBase):
    def __init__(self) -> None:
        super().__init__()
        self.key = 'light.radiance.value'
        self.scene_dict = {
            'type': 'scene',
            'plane': {
                'type': 'rectangle',
                'bsdf': { 'type': 'diffuse' }
            },
            'sphere': {
                'type': 'sphere',
                'to_world': T.scale(0.25),
            },
            'light': { 'type': 'constant' }
        }

# Test crop offset and crop window on the film
class CropWindowConfig(ConfigBase):
    def __init__(self) -> None:
        super().__init__()
        self.key = 'plane.bsdf.reflectance.value'
        self.scene_dict = {
            'type': 'scene',
            'plane': {
                'type': 'rectangle',
                'bsdf': { 'type': 'diffuse' }
            },
            'light': { 'type': 'constant' }
        }
        self.res = 64
        self.sensor_dict = {
            'type': 'perspective',
            'to_world': T.look_at(origin=[0, 0, 4], target=[0, 0, 0], up=[0, 1, 0]),
            'film': {
                'type': 'hdrfilm',
                'rfilter': { 'type': 'gaussian', 'stddev': 0.5 },
                'width': self.res,
                'height': self.res,
                'high_quality_edges': True,
                "crop_width" : 32,
                "crop_height" : 32,
                "crop_offset_x" : 32,
                "crop_offset_y" : 20,
            }
        }

# -------------------------------------------------------------------
#            Test configs for reparameterized integrators
# -------------------------------------------------------------------

# Translate shape base configuration
class TranslateShapeConfigBase(ConfigBase):
    require_reparameterization = True

    def __init__(self) -> None:
        super().__init__()

    def initialize(self):
        from mitsuba.core import Vector3f
        super().initialize()
        self.initial_state = ek.unravel(Vector3f, self.params[self.key])

    def update(self, theta):
        from mitsuba.core import Vector3f
        self.params[self.key] = ek.ravel(self.initial_state + Vector3f(theta, 0.0, 0.0))
        self.params.update()
        ek.eval()


# Scale shape base configuration
class ScaleShapeConfigBase(ConfigBase):
    require_reparameterization = True

    def __init__(self) -> None:
        super().__init__()

    def initialize(self):
        from mitsuba.core import Vector3f
        super().initialize()
        self.initial_state = ek.unravel(Vector3f, self.params[self.key])

    def update(self, theta):
        from mitsuba.core import Vector3f
        self.params[self.key] = ek.ravel(self.initial_state * (1.0 + theta))
        self.params.update()
        ek.eval()


# Translate area emitter (rectangle) on black background
class TranslateRectangleEmitterOnBlackConfig(TranslateShapeConfigBase):
    def __init__(self) -> None:
        super().__init__()
        self.key = 'light.vertex_positions'
        self.scene_dict = {
            'type': 'scene',
            'light': {
                'type': 'obj',
                'filename': 'resources/data/common/meshes/rectangle.obj',
                'face_normals': True,
                'emitter': {
                    'type': 'area',
                    'radiance': {'type': 'rgb', 'value': [1.0, 1.0, 1.0]}
                },
                'to_world': T.translate([1.25, 0.0, 0.0]),
            }
        }
        self.error_mean_threshold = 0.6
        self.error_max_threshold = 6.0
        self.spp = 12000


# Translate area emitter (sphere) on black background
class TranslateSphereEmitterOnBlackConfig(TranslateShapeConfigBase):
    def __init__(self) -> None:
        super().__init__()
        self.key = 'light.vertex_positions'
        self.scene_dict = {
            'type': 'scene',
            'light': {
                'type': 'obj',
                'filename': 'resources/data/common/meshes/sphere.obj',
                'emitter': {
                    'type': 'area',
                    'radiance': {'type': 'rgb', 'value': [1.0, 1.0, 1.0]}
                },
                'to_world': T.translate([1.25, 0.0, 0.0]),
            }
        }
        self.error_mean_threshold = 0.6
        self.error_max_threshold = 6.0
        self.spp = 12000

# Scale area emitter (sphere) on black background
class ScaleSphereEmitterOnBlackConfig(ScaleShapeConfigBase):
    def __init__(self) -> None:
        super().__init__()
        self.key = 'light.vertex_positions'
        self.scene_dict = {
            'type': 'scene',
            'light': {
                'type': 'obj',
                'filename': 'resources/data/common/meshes/sphere.obj',
                'emitter': {
                    'type': 'area',
                    'radiance': {'type': 'rgb', 'value': [1.0, 1.0, 1.0]}
                },
            }
        }
        self.error_mean_threshold = 0.6
        self.error_max_threshold = 5.5

# Translate occluder (sphere) casting shadow on gray wall
class TranslateOccluderAreaLightConfig(TranslateShapeConfigBase):
    def __init__(self) -> None:
        super().__init__()
        self.key = 'occluder.vertex_positions'
        self.scene_dict = {
            'type': 'scene',
            'plane': {
                'type': 'rectangle',
                'bsdf': { 'type': 'diffuse' }
            },
            'occluder': {
                'type': 'obj',
                'filename': 'resources/data/common/meshes/sphere.obj',
                'to_world': T.translate([2.0, 0.0, 2.0]) * T.scale(0.25),
            },
            'light': {
                'type': 'obj',
                'filename': 'resources/data/common/meshes/sphere.obj',
                'emitter': {
                    'type': 'area',
                    'radiance': {'type': 'rgb', 'value': [10.0, 10.0, 10.0]}
                },
                'to_world': T.translate([4.0, 0.0, 4.0]) * T.scale(0.05)
            }
        }


# Translate textured plane
class TranslateTexturedPlaneConfig(TranslateShapeConfigBase):
    def __init__(self) -> None:
        super().__init__()
        self.key = 'plane.vertex_positions'
        self.scene_dict = {
            'type': 'scene',
            'plane': {
                'type': 'obj',
                'filename': 'resources/data/common/meshes/rectangle.obj',
                'bsdf': {
                    'type': 'diffuse',
                    'reflectance' : {
                        'type': 'bitmap',
                        # 'filename' : 'resources/data/common/textures/flower.bmp'
                        'filename' : 'resources/data/common/textures/museum.exr'
                    }
                },
                'to_world': T.scale(2.0),
            },
            'light': { 'type': 'constant' }
        }
        self.res = 64
        self.ref_fd_epsilon = 1e-3
        self.spp = 2500
        self.error_mean_threshold = 0.06
        self.error_max_threshold = 15.5


# Translate occluder casting shadow on itself
class TranslateSelfShadowAreaLightConfig(ConfigBase):
    def __init__(self) -> None:
        super().__init__()
        self.res = 64
        self.scene_dict = {
            'type': 'scene',
            'plane': {
                'type': 'obj',
                'filename': 'resources/data/common/meshes/rectangle.obj',
                'face_normals': True,
            },
            'occluder': {
                'type': 'obj',
                'filename': 'resources/data/common/meshes/rectangle.obj',
                'face_normals': True,
                # 'to_world': T.rotate([0, 1, 0], 45) * T.scale(0.5),
                # 'to_world': T.translate([2.0, 0.0, 2.0]) * T.scale(0.25),
                'to_world': T.translate([-1, 0, 0.5]) * T.rotate([0, 1, 0], 90) * T.scale(1.0),
            },
            'light': {
                'type': 'obj',
                'filename': 'resources/data/common/meshes/sphere.obj',
                'emitter': {
                    'type': 'area',
                    # 'radiance': {'type': 'rgb', 'value': [100.0, 100.0, 100.0]}
                    'radiance': {'type': 'rgb', 'value': [1.0, 1.0, 1.0]}
                },
                # 'to_world': T.translate([4.0, 0.0, 4.0]) * T.scale(0.025)
                'to_world': T.translate([4.0, 0.0, 4.0]) * T.scale(0.5)
            },
            # 'light2': { 'type': 'constant', 'radiance': 0.001 },
        }
        self.error_mean_threshold = 0.6
        self.error_max_threshold = 6.0
        self.spp = 4024

    def initialize(self):
        from mitsuba.core import Vector3f
        super().initialize()
        self.params.keep(['plane.vertex_positions', 'occluder.vertex_positions'])
        # self.params.keep(['plane.vertex_positions'])
        # self.params.keep(['occluder.vertex_positions'])
        self.initial_state_0 = ek.unravel(Vector3f, self.params['plane.vertex_positions'])
        self.initial_state_1 = ek.unravel(Vector3f, self.params['occluder.vertex_positions'])

    def update(self, theta):
        from mitsuba.core import Vector3f
        self.params['plane.vertex_positions']    = ek.ravel(self.initial_state_0 + Vector3f(theta, 0.0, 0.0))
        self.params['occluder.vertex_positions'] = ek.ravel(self.initial_state_1 + Vector3f(theta, 0.0, 0.0))
        self.params.update()
        ek.eval()


# -------------------------------------------------------------------
#                           List configs
# -------------------------------------------------------------------

BASIC_CONFIGS_LIST = [
    DiffuseAlbedoConfig,
    DiffuseAlbedoGIConfig,
    AreaLightRadianceConfig,
    DirectlyVisibleAreaLightRadianceConfig,
    PointLightIntensityConfig,
    ConstantEmitterRadianceConfig,
    TranslateTexturedPlaneConfig,
    CropWindowConfig,
]

REPARAM_CONFIGS_LIST = [
    # TranslateRectangleEmitterOnBlackConfig,
    # TranslateSphereEmitterOnBlackConfig,
    # ScaleSphereEmitterOnBlackConfig,
    # TranslateOccluderAreaLightConfig,
    # TranslateSelfShadowAreaLightConfig,
]

# List of integrators to test (also indicates whether it handles discontinuities)
INTEGRATORS = [
    ('path', False),
    ('rb', False),
    ('prb', False),
    # ('rbreparam', True),
    # ('prbreparam', True),
]

CONFIGS = []
for integrator_name, reparam in INTEGRATORS:
    todos = BASIC_CONFIGS_LIST + (REPARAM_CONFIGS_LIST if reparam else [])
    for config in todos:
        CONFIGS.append((integrator_name, config))

# -------------------------------------------------------------------
#                           Unit tests
# -------------------------------------------------------------------

@pytest.mark.slow
@pytest.mark.parametrize('integrator_name, config', CONFIGS)
def test01_rendering_primal(variants_all_ad_rgb, integrator_name, config):
    from mitsuba.core import xml, TensorXf, Bitmap
    from mitsuba.python.util import write_bitmap

    config = config()
    config.initialize()

    integrator = xml.load_dict({
        'type': integrator_name,
        'max_depth': config.max_depth
    })

    filename = join(output_dir, f"test_{config.name}_image_primal_ref.exr")
    image_primal_ref = TensorXf(Bitmap(filename))
    image = integrator.render(config.scene, seed=0, spp=config.spp)

    error = ek.abs(image - image_primal_ref) / ek.max(ek.abs(image_primal_ref), 2e-2)
    error_mean = ek.hmean(error)
    error_max = ek.hmax(error)

    if error_mean > config.error_mean_threshold  or error_max > config.error_max_threshold:
        print(f"Failure in config: {config.name}, {integrator_name}")
        print(f"-> error mean: {error_mean} (threshold={config.error_mean_threshold})")
        print(f"-> error max: {error_max} (threshold={config.error_max_threshold})")
        filename = f"test_{integrator_name}_{config.name}_image_primal.exr"
        write_bitmap(filename, image)
        assert False


@pytest.mark.slow
@pytest.mark.parametrize('integrator_name, config', CONFIGS)
def test02_rendering_forward(variants_all_ad_rgb, integrator_name, config):
    from mitsuba.core import xml, Float, TensorXf, Bitmap
    from mitsuba.python.util import write_bitmap

    # ek.set_flag(ek.JitFlag.PrintIR, True)
    # ek.set_flag(ek.JitFlag.LoopRecord, False)
    # ek.set_flag(ek.JitFlag.VCallRecord, False)

    config = config()
    config.initialize()

    integrator = xml.load_dict({
        'type': integrator_name,
        'max_depth': config.max_depth
    })

    filename = join(output_dir, f"test_{config.name}_image_fwd_ref.exr")
    image_fwd_ref = TensorXf(Bitmap(filename))

    theta = Float(0.0)
    ek.enable_grad(theta)
    ek.set_label(theta, 'theta')
    config.update(theta)
    ek.forward(theta, ek.ADFlag.ClearEdges)

    # ek.set_log_level(3)

    ek.set_label(config.params, 'params')
    image_fwd = integrator.render_forward(
        config.scene, config.params, seed=0, spp=config.spp)
    image_fwd = ek.detach(image_fwd)

    error = ek.abs(image_fwd - image_fwd_ref) / ek.max(ek.abs(image_fwd_ref), 2e-1)
    error_mean = ek.hmean(error)
    error_max = ek.hmax(error)

    if error_mean > config.error_mean_threshold or error_max > config.error_max_threshold:
        print(f"Failure in config: {config.name}, {integrator_name}")
        print(f"-> error mean: {error_mean} (threshold={config.error_mean_threshold})")
        print(f"-> error max: {error_max} (threshold={config.error_max_threshold})")
        filename = f"test_{integrator_name}_{config.name}_image_fwd.exr"
        write_bitmap(filename, image_fwd)
        filename = f"test_{integrator_name}_{config.name}_image_error.exr"
        write_bitmap(filename, error)

        # print(f"ek.hmean(image_fwd_ref): {ek.hmean(image_fwd_ref)}")
        # print(f"ek.hmean(image_fwd):     {ek.hmean(image_fwd)}")
        assert False


@pytest.mark.slow
@pytest.mark.parametrize('integrator_name, config', CONFIGS)
def test03_rendering_backward(variants_all_ad_rgb, integrator_name, config):
    from mitsuba.core import xml, Float, TensorXf, Bitmap

    # ek.set_flag(ek.JitFlag.LoopRecord, False)
    # ek.set_flag(ek.JitFlag.VCallRecord, False)

    config = config()
    config.initialize()

    integrator = xml.load_dict({
        'type': integrator_name,
        'max_depth': config.max_depth
    })

    filename = join(output_dir, f"test_{config.name}_image_fwd_ref.exr")
    image_fwd_ref = TensorXf(Bitmap(filename))

    image_adj = TensorXf(1.0, image_fwd_ref.shape)

    theta = Float(0.0)
    ek.enable_grad(theta)
    config.update(theta)

    # ek.set_flag(ek.JitFlag.KernelHistory, True)
    # ek.set_flag(ek.JitFlag.LaunchBlocking, True)
    # ek.set_log_level(3)

    integrator.render_backward(
        config.scene, config.params, image_adj, seed=0, spp=config.spp)

    grad = ek.grad(theta)[0] / ek.width(image_fwd_ref)
    grad_ref = ek.hmean(image_fwd_ref)

    error = ek.abs(grad - grad_ref) / ek.max(ek.abs(grad_ref), 1e-3)
    if error > config.error_mean_threshold:
        print(f"Failure in config: {config.name}, {integrator_name}")
        print(f"-> grad:     {grad}")
        print(f"-> grad_ref: {grad_ref}")
        print(f"-> error: {error} (threshold={config.error_mean_threshold})")
        print(f"-> ratio: {grad / grad_ref}")
        assert False


@pytest.mark.slow
def test04_render_custom_op(variants_all_ad_rgb):
    from mitsuba.core import xml, Float, TensorXf, Bitmap
    from mitsuba.python.util import write_bitmap
    from mitsuba.python.ad import render

    config = DiffuseAlbedoConfig()
    config.initialize()

    integrator = xml.load_dict({ 'type': 'rb' })

    filename = join(output_dir, f"test_{config.name}_image_primal_ref.exr")
    image_primal_ref = TensorXf(Bitmap(filename))

    filename = join(output_dir, f"test_{config.name}_image_fwd_ref.exr")
    image_fwd_ref = TensorXf(Bitmap(filename))

    theta = Float(0.0)
    ek.enable_grad(theta)
    config.update(theta)

    image_primal = render(config.scene, integrator, config.params, seed=0, spp=config.spp)

    error = ek.abs(ek.detach(image_primal) - image_primal_ref) / ek.max(ek.abs(image_primal_ref), 2e-2)
    error_mean = ek.hmean(error)
    error_max = ek.hmax(error)

    if error_mean > config.error_mean_threshold  or error_max > config.error_max_threshold:
        print(f"Failure in config: {config.name}, {integrator_name}")
        print(f"-> error mean: {error_mean} (threshold={config.error_mean_threshold})")
        print(f"-> error max: {error_max} (threshold={config.error_max_threshold})")
        filename = f"test_{integrator_name}_{config.name}_image_primal.exr"
        write_bitmap(filename, image_primal)
        assert False

    filename = f"test_render_custom_op_image_primal.exr"
    write_bitmap(filename, image_primal)

    obj = ek.hmean_async(image_primal)
    ek.backward(obj)

    grad = ek.grad(theta)[0]
    grad_ref = ek.hmean(image_fwd_ref)

    error = ek.abs(grad - grad_ref) / ek.max(ek.abs(grad_ref), 1e-3)
    if error > config.error_mean_threshold:
        print(f"Failure in config: {config.name}, {integrator_name}")
        print(f"-> grad:     {grad}")
        print(f"-> grad_ref: {grad_ref}")
        print(f"-> error: {error} (threshold={config.error_mean_threshold})")
        print(f"-> ratio: {grad / grad_ref}")
        assert False

    theta = Float(0.0)
    ek.enable_grad(theta)
    config.update(theta)

    image_primal = render(config.scene, integrator, config.params, seed=0, spp=config.spp)

    ek.forward(theta)

    image_fwd = ek.grad(image_primal)

    error = ek.abs(image_fwd - image_fwd_ref) / ek.max(ek.abs(image_fwd_ref), 2e-1)
    error_mean = ek.hmean(error)
    error_max = ek.hmax(error)

    if error_mean > config.error_mean_threshold or error_max > config.error_max_threshold:
        print(f"Failure in config: {config.name}, {integrator_name}")
        print(f"-> error mean: {error_mean} (threshold={config.error_mean_threshold})")
        print(f"-> error max: {error_max} (threshold={config.error_max_threshold})")
        filename = f"test_{integrator_name}_{config.name}_image_fwd.exr"
        write_bitmap(filename, image_fwd)
        filename = f"test_{integrator_name}_{config.name}_image_error.exr"
        write_bitmap(filename, error)
        assert False

# -------------------------------------------------------------------
#                      Generate reference images
# -------------------------------------------------------------------

if __name__ == "__main__":
    """
    Generate reference primal/forward images for all configs.
    """
    parser = argparse.ArgumentParser(prog='GenerateConfigReferenceImages')
    parser.add_argument('--spp', default=12000, type=int,
                        help='Samples per pixel. Default value: 12000')
    args = parser.parse_args()

    mitsuba.set_variant('cuda_ad_rgb')

    if not exists(output_dir):
        os.makedirs(output_dir)

    from mitsuba.core import xml, Float
    from mitsuba.python.util import write_bitmap

    for config in BASIC_CONFIGS_LIST + REPARAM_CONFIGS_LIST:
        config = config()
        print(f"name: {config.name}")

        config.initialize()

        integrator = xml.load_dict({
            'type': 'path',
            'max_depth': config.max_depth
        })

        image_ref = integrator.render(config.scene, seed=0, spp=args.spp)

        filename = join(output_dir, f"test_{config.name}_image_primal_ref.exr")
        write_bitmap(filename, image_ref)

        theta = Float(config.ref_fd_epsilon)
        config.update(theta)

        image_2 = integrator.render(config.scene, seed=0, spp=args.spp)
        image_fd = (image_2 - image_ref) / config.ref_fd_epsilon

        filename = join(output_dir, f"test_{config.name}_image_fwd_ref.exr")
        write_bitmap(filename, image_fd)
