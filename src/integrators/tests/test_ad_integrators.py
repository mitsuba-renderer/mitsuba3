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
import importlib

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
        self.error_max_threshold = 0.5
        self.error_mean_threshold_bwd = 0.05
        self.ref_fd_epsilon = 1e-3

        self.integrator_dict = {
            'max_depth': 2,
        }

        self.sensor_dict = {
            'type': 'perspective',
            'to_world': T.look_at(origin=[0, 0, 4], target=[0, 0, 0], up=[0, 1, 0]),
            'film': {
                'type': 'hdrfilm',
                'rfilter': { 'type': 'gaussian', 'stddev': 0.5 },
                'width': self.res,
                'height': self.res,
                'sample_border': True,
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
        from mitsuba.core import load_dict
        from mitsuba.python.util import traverse

        self.sensor_dict['film']['width'] = self.res
        self.sensor_dict['film']['height'] = self.res
        self.scene_dict['camera'] = self.sensor_dict

        @fresolver_append_path
        def create_scene():
            return load_dict(self.scene_dict)
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
            'light': { 'type': 'constant', 'radiance': 3.0 }
        }
        self.integrator_dict = {
            'max_depth': 3,
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
                'bsdf': {
                    'type': 'diffuse',
                    'reflectance': {'type': 'rgb', 'value': [1.0, 1.0, 1.0]}
                }
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
                    'radiance': {'type': 'rgb', 'value': [3.0, 3.0, 3.0]}
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
                'bsdf': {
                    'type': 'diffuse',
                    'reflectance': {'type': 'rgb', 'value': [1.0, 1.0, 1.0]}
                }
            },
            'sphere': {
                'type': 'sphere',
                'to_world': T.scale(0.25),
            },
            'light': {
                'type': 'point',
                'position': [1.25, 0.0, 1.0],
                'intensity': {'type': 'rgb', 'value': [5.0, 5.0, 5.0]}
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
                'sample_border': True,
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

# Translate diffuse sphere under constant illumination
class TranslateDiffuseSphereConstantConfig(TranslateShapeConfigBase):
    def __init__(self) -> None:
        super().__init__()
        self.key = 'sphere.vertex_positions'
        self.scene_dict = {
            'type': 'scene',
            'sphere': {
                'type': 'obj',
                'filename': 'resources/data/common/meshes/sphere.obj',
            },
            'light': { 'type': 'constant' }
        }
        self.ref_fd_epsilon = 1e-3
        self.error_mean_threshold = 0.02
        self.error_max_threshold = 0.5
        self.error_mean_threshold_bwd = 0.25
        self.spp = 1024
        self.integrator_dict = {
            'max_depth': 2,
            'reparam_rays': 64,
            'reparam_kappa': 1e5,
        }

# Translate diffuse rectangle under constant illumination
class TranslateDiffuseRectangleConstantConfig(TranslateShapeConfigBase):
    def __init__(self) -> None:
        super().__init__()
        self.key = 'rectangle.vertex_positions'
        self.scene_dict = {
            'type': 'scene',
            'rectangle': {
                'type': 'obj',
                'filename': 'resources/data/common/meshes/rectangle.obj',
                'face_normals': True,
            },
            'light': { 'type': 'constant' }
        }
        self.ref_fd_epsilon = 8e-4
        self.error_mean_threshold = 0.02
        self.error_max_threshold = 0.5
        self.error_mean_threshold_bwd = 0.25
        self.spp = 1024
        self.integrator_dict = {
            'max_depth': 2,
            'reparam_rays': 64,
            'reparam_kappa': 1e5,
        }

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
        self.ref_fd_epsilon = 1e-3
        self.error_mean_threshold = 0.03
        self.error_max_threshold = 1.0
        self.error_mean_threshold_bwd = 0.2
        self.spp = 12000
        self.integrator_dict = {
            'max_depth': 2,
            'reparam_rays': 64,
            'reparam_kappa': 1e5,
        }


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
        self.error_mean_threshold = 0.02
        self.error_max_threshold = 0.5
        self.error_mean_threshold_bwd = 0.15
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
        self.ref_fd_epsilon = 1e-3
        self.error_mean_threshold = 0.04
        self.error_max_threshold = 0.5
        self.error_mean_threshold_bwd = 0.1
        self.integrator_dict = {
            'max_depth': 3,
            'reparam_rays': 64,
        }


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
                    'radiance': {'type': 'rgb', 'value': [1000.0, 1000.0, 1000.0]}
                },
                'to_world': T.translate([4.0, 0.0, 4.0]) * T.scale(0.05)
            }
        }
        self.ref_fd_epsilon = 1e-4
        self.error_mean_threshold = 0.02
        self.error_max_threshold = 0.5
        self.error_mean_threshold_bwd = 0.25
        self.spp = 2048
        self.integrator_dict = {
            'max_depth': 2,
            'reparam_rays': 64,
            'reparam_kappa': 5e5,
        }

# Translate shadow receiver
class TranslateShadowReceiverAreaLightConfig(TranslateShapeConfigBase):
    def __init__(self) -> None:
        super().__init__()
        self.key = 'plane.vertex_positions'
        self.scene_dict = {
            'type': 'scene',
            'plane': {
                'type': 'obj',
                'filename': 'resources/data/common/meshes/rectangle.obj',
                'face_normals': True,
                'bsdf': { 'type': 'diffuse' }
            },
            'occluder': {
                'type': 'obj',
                'filename': 'resources/data/common/meshes/sphere.obj',
                'to_world': T.translate([2.0, 0.0, 2.0]) * T.scale(0.25),
            },
            # 'light': {
            #     'type': 'obj',
            #     'filename': 'resources/data/common/meshes/sphere.obj',
            #     'emitter': {
            #         'type': 'area',
            #         'radiance': {'type': 'rgb', 'value': [1000.0, 1000.0, 1000.0]}
            #     },
            #     'to_world': T.translate([4.0, 0.0, 4.0]) * T.scale(0.05)
            # }
            'light': { 'type': 'constant' }
        }
        self.ref_fd_epsilon = 1e-3
        self.error_mean_threshold = 0.02
        self.error_max_threshold = 0.5
        self.error_mean_threshold_bwd = 0.25
        self.spp = 4096
        self.integrator_dict = {
            'max_depth': 2,
            'reparam_rays': 64,
            'reparam_kappa': 1e5,
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
                'face_normals': True,
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
        self.spp = 800
        self.error_mean_threshold = 0.1
        self.error_max_threshold = 56.0


# Translate occluder casting shadow on itself
class TranslateSelfShadowAreaLightConfig(ConfigBase):
    def __init__(self) -> None:
        super().__init__()
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
                'to_world': T.translate([-1, 0, 0.5]) * T.rotate([0, 1, 0], 90) * T.scale(1.0),
            },
            'light': {
                'type': 'point',
                'position': [-4, 0, 6],
                'intensity': {'type': 'rgb', 'value': [5.0, 0.0, 0.0]}
            },
            'light2': { 'type': 'constant', 'radiance': 0.1 },
        }
        self.error_mean_threshold = 0.03
        self.error_max_threshold = 0.7
        self.error_mean_threshold_bwd = 0.25
        self.spp = 4096
        self.integrator_dict = {
            'max_depth': 3,
            'reparam_rays': 64,
            'reparam_kappa': 1e5,
        }

    def initialize(self):
        from mitsuba.core import Vector3f
        super().initialize()
        self.params.keep(['plane.vertex_positions', 'occluder.vertex_positions'])
        self.initial_state_0 = ek.unravel(Vector3f, self.params['plane.vertex_positions'])
        self.initial_state_1 = ek.unravel(Vector3f, self.params['occluder.vertex_positions'])

    def update(self, theta):
        from mitsuba.core import Vector3f
        self.params['plane.vertex_positions']    = ek.ravel(self.initial_state_0 + Vector3f(theta, 0.0, 0.0))
        self.params['occluder.vertex_positions'] = ek.ravel(self.initial_state_1 + Vector3f(theta, 0.0, 0.0))
        self.params.update()
        ek.eval()


# Translate sphere reflecting on glossy floor
class TranslateSphereOnGlossyFloorConfig(TranslateShapeConfigBase):
    def __init__(self) -> None:
        super().__init__()
        self.key = 'sphere.vertex_positions'
        self.scene_dict = {
            'type': 'scene',
            'floor': {
                'type': 'rectangle',
                'bsdf': {
                    'type': 'roughconductor',
                    'alpha': 0.025,
                },
                'to_world': T.translate([0, 1.5, 0]) *  T.rotate([1, 0, 0], -45) * T.scale(4),
            },
            'sphere': {
                'type': 'obj',
                'bsdf': {
                    'type': 'diffuse',
                    'reflectance': {'type': 'rgb', 'value': [1.0, 0.5, 0.0]}
                },
                'filename': 'resources/data/common/meshes/sphere.obj',
                'to_world': T.translate([0.5, 2.0, 1.5]) * T.scale(1.0),
            },
            'light': { 'type': 'constant', 'radiance': 1.0 },
        }
        self.res = 32
        self.ref_fd_epsilon = 1e-3
        self.error_mean_threshold = 0.25
        self.error_max_threshold = 5.0
        self.error_mean_threshold_bwd = 0.2
        self.spp = 2048
        self.integrator_dict = {
            'max_depth': 3,
            'reparam_rays': 64,
            'reparam_kappa': 2e5,
        }


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
    # TranslateDiffuseSphereConstantConfig,
    # TranslateDiffuseRectangleConstantConfig,
    TranslateRectangleEmitterOnBlackConfig,
    TranslateSphereEmitterOnBlackConfig,
    ScaleSphereEmitterOnBlackConfig,
    TranslateOccluderAreaLightConfig,
    TranslateSelfShadowAreaLightConfig,
    # TranslateShadowReceiverAreaLightConfig,
    TranslateSphereOnGlossyFloorConfig
]

# List of integrators to test (also indicates whether it handles discontinuities)
INTEGRATORS = [
    ('path', False),
    ('prb', False),
    ('prb_reparam', True),
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
    from mitsuba.core import load_dict, TensorXf, Bitmap
    from mitsuba.python.util import write_bitmap

    config = config()
    config.initialize()

    # ek.set_flag(ek.JitFlag.VCallRecord, False)
    # ek.set_flag(ek.JitFlag.LoopRecord, False)

    importlib.reload(mitsuba.python.ad.integrators)
    config.integrator_dict['type'] = integrator_name
    integrator = load_dict(config.integrator_dict)

    filename = join(output_dir, f"test_{config.name}_image_primal_ref.exr")
    image_primal_ref = TensorXf(Bitmap(filename))
    image = integrator.render(config.scene, seed=0, spp=config.spp)

    error = ek.abs(image - image_primal_ref) / ek.max(ek.abs(image_primal_ref), 2e-2)
    error_mean = ek.hmean(error)
    error_max = ek.hmax(error)

    # filename = join(os.getcwd(), f"test_{integrator_name}_{config.name}_image_primal.exr")
    # print(f'-> write current image: {filename}')
    # write_bitmap(filename, image)

    if error_mean > config.error_mean_threshold  or error_max > config.error_max_threshold:
        print(f"Failure in config: {config.name}, {integrator_name}")
        print(f"-> error mean: {error_mean} (threshold={config.error_mean_threshold})")
        print(f"-> error max: {error_max} (threshold={config.error_max_threshold})")
        print(f'-> reference image: {filename}')
        filename = join(os.getcwd(), f"test_{integrator_name}_{config.name}_image_primal.exr")
        print(f'-> write current image: {filename}')
        write_bitmap(filename, image)
        assert False


@pytest.mark.slow
@pytest.mark.skipif(os.name == 'nt', reason='Skip those memory heavy tests on Windows')
@pytest.mark.parametrize('integrator_name, config', CONFIGS)
def test02_rendering_forward(variants_all_ad_rgb, integrator_name, config):
    from mitsuba.core import load_dict, Float, TensorXf, Bitmap
    from mitsuba.python.util import write_bitmap

    # ek.set_flag(ek.JitFlag.PrintIR, True)

    # ek.set_flag(ek.JitFlag.LoopRecord, False)
    # ek.set_flag(ek.JitFlag.VCallRecord, False)

    # ek.set_flag(ek.JitFlag.LoopOptimize, False)
    # ek.set_flag(ek.JitFlag.VCallOptimize, False)
    # ek.set_flag(ek.JitFlag.VCallInline, True)
    # ek.set_flag(ek.JitFlag.ValueNumbering, False)
    # ek.set_flag(ek.JitFlag.ConstProp, False)

    config = config()
    config.initialize()

    importlib.reload(mitsuba.python.ad.integrators)
    config.integrator_dict['type'] = integrator_name
    integrator = load_dict(config.integrator_dict)

    filename = join(output_dir, f"test_{config.name}_image_fwd_ref.exr")
    image_fwd_ref = TensorXf(Bitmap(filename))

    theta = Float(0.0)
    ek.enable_grad(theta)
    ek.set_label(theta, 'theta')
    config.update(theta)
    ek.forward(theta, ek.ADFlag.ClearEdges)

    ek.set_label(config.params, 'params')
    image_fwd = integrator.render_forward(
        config.scene, seed=0, spp=config.spp, params=theta)
    image_fwd = ek.detach(image_fwd)

    error = ek.abs(image_fwd - image_fwd_ref) / ek.max(ek.abs(image_fwd_ref), 2e-1)
    error_mean = ek.hmean(error)
    error_max = ek.hmax(error)

    # filename = join(os.getcwd(), f"test_{integrator_name}_{config.name}_image_fwd.exr")
    # print(f'-> write current image: {filename}')
    # write_bitmap(filename, image_fwd)

    if error_mean > config.error_mean_threshold or error_max > config.error_max_threshold:
        print(f"Failure in config: {config.name}, {integrator_name}")
        print(f"-> error mean: {error_mean} (threshold={config.error_mean_threshold})")
        print(f"-> error max: {error_max} (threshold={config.error_max_threshold})")
        print(f'-> reference image: {filename}')
        filename = join(os.getcwd(), f"test_{integrator_name}_{config.name}_image_fwd.exr")
        print(f'-> write current image: {filename}')
        write_bitmap(filename, image_fwd)
        filename = join(os.getcwd(), f"test_{integrator_name}_{config.name}_image_error.exr")
        print(f'-> write error image: {filename}')
        write_bitmap(filename, error)

        # print(f"ek.hmean(image_fwd_ref): {ek.hmean(image_fwd_ref)}")
        # print(f"ek.hmean(image_fwd):     {ek.hmean(image_fwd)}")
        assert False


@pytest.mark.slow
@pytest.mark.skipif(os.name == 'nt', reason='Skip those memory heavy tests on Windows')
@pytest.mark.parametrize('integrator_name, config', CONFIGS)
def test03_rendering_backward(variants_all_ad_rgb, integrator_name, config):
    from mitsuba.core import load_dict, Float, TensorXf, Bitmap

    # ek.set_flag(ek.JitFlag.LoopRecord, False)
    # ek.set_flag(ek.JitFlag.VCallRecord, False)

    config = config()
    config.initialize()

    importlib.reload(mitsuba.python.ad.integrators)
    config.integrator_dict['type'] = integrator_name
    integrator = load_dict(config.integrator_dict)

    filename = join(output_dir, f"test_{config.name}_image_fwd_ref.exr")
    image_fwd_ref = TensorXf(Bitmap(filename))

    image_adj = TensorXf(1.0, image_fwd_ref.shape)

    theta = Float(0.0)
    ek.enable_grad(theta)
    config.update(theta)

    # ek.set_flag(ek.JitFlag.KernelHistory, True)
    # ek.set_log_level(3)

    integrator.render_backward(
        config.scene, grad_in=image_adj, seed=0, spp=config.spp, params=theta)

    grad = ek.grad(theta)[0] / ek.width(image_fwd_ref)
    grad_ref = ek.hmean(image_fwd_ref)

    error = ek.abs(grad - grad_ref) / ek.max(ek.abs(grad_ref), 1e-3)
    if error > config.error_mean_threshold_bwd:
        print(f"Failure in config: {config.name}, {integrator_name}")
        print(f"-> grad:     {grad}")
        print(f"-> grad_ref: {grad_ref}")
        print(f"-> error: {error} (threshold={config.error_mean_threshold_bwd})")
        print(f"-> ratio: {grad / grad_ref}")
        assert False


@pytest.mark.slow
@pytest.mark.skipif(os.name == 'nt', reason='Skip those memory heavy tests on Windows')
def test04_render_custom_op(variants_all_ad_rgb):
    from mitsuba.core import load_dict, Float, TensorXf, Bitmap
    from mitsuba.python.util import write_bitmap
    from mitsuba.python.ad import render

    config = DiffuseAlbedoConfig()
    config.initialize()

    importlib.reload(mitsuba.python.ad.integrators)
    integrator = load_dict({
        'type': 'prb',
        'max_depth': config.integrator_dict['max_depth']
    })

    filename = join(output_dir, f"test_{config.name}_image_primal_ref.exr")
    image_primal_ref = TensorXf(Bitmap(filename))

    filename = join(output_dir, f"test_{config.name}_image_fwd_ref.exr")
    image_fwd_ref = TensorXf(Bitmap(filename))

    theta = Float(0.0)
    ek.enable_grad(theta)
    config.update(theta)

    image_primal = render(config.scene, config.params, integrator=integrator, seed=0, spp=config.spp)

    error = ek.abs(ek.detach(image_primal) - image_primal_ref) / ek.max(ek.abs(image_primal_ref), 2e-2)
    error_mean = ek.hmean(error)
    error_max = ek.hmax(error)

    if error_mean > config.error_mean_threshold  or error_max > config.error_max_threshold:
        print(f"Failure in config: {config.name}, {integrator_name}")
        print(f"-> error mean: {error_mean} (threshold={config.error_mean_threshold})")
        print(f"-> error max: {error_max} (threshold={config.error_max_threshold})")
        print(f'-> reference image: {filename}')
        filename = join(os.getcwd(), f"test_{integrator_name}_{config.name}_image_primal.exr")
        print(f'-> write current image: {filename}')
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

    image_primal = render(config.scene, config.params, integrator=integrator, seed=0, spp=config.spp)

    ek.forward(theta)

    image_fwd = ek.grad(image_primal)

    error = ek.abs(image_fwd - image_fwd_ref) / ek.max(ek.abs(image_fwd_ref), 2e-1)
    error_mean = ek.hmean(error)
    error_max = ek.hmax(error)

    if error_mean > config.error_mean_threshold or error_max > config.error_max_threshold:
        print(f"Failure in config: {config.name}, {integrator_name}")
        print(f"-> error mean: {error_mean} (threshold={config.error_mean_threshold})")
        print(f"-> error max: {error_max} (threshold={config.error_max_threshold})")
        filename = join(os.getcwd(), f"test_{integrator_name}_{config.name}_image_fwd.exr")
        write_bitmap(filename, image_fwd)
        filename = join(os.getcwd(), f"test_{integrator_name}_{config.name}_image_error.exr")
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

    from mitsuba.core import load_dict, Float, Bitmap
    from mitsuba.python.util import write_bitmap

    for config in BASIC_CONFIGS_LIST + REPARAM_CONFIGS_LIST:
        config = config()
        print(f"name: {config.name}")

        config.initialize()

        if False:
            boundary_test_integrator = load_dict({
                'type': 'aov',
                'aovs': 'X:boundary_test',
            })
            image_boundary_test = boundary_test_integrator.render(config.scene, seed=0, spp=args.spp)
            print(f"image_boundary_test: {image_boundary_test}")

            filename = join(output_dir, f"test_{config.name}_image_boundary_test.exr")
            Bitmap(image_boundary_test[:, :, 3]).write(filename)

        integrator = load_dict({
            'type': 'path',
            'max_depth': config.integrator_dict['max_depth']
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
