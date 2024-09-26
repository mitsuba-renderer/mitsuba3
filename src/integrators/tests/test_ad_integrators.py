"""
Overview
--------

This file defines a set of unit tests to assess the correctness of the
different adjoint integrators such as `rb`, `prb`, etc... All integrators will
be tested for their implementation of primal rendering, adjoint forward
rendering and adjoint backward rendering.

- For primal rendering, the output image will be compared to a ground truth
  image precomputed in the `resources/data/tests/integrators` directory.
- Adjoint forward rendering will be compared against finite differences.
- Adjoint backward rendering will be compared against finite differences.

Those tests will be run on a set of configurations (scene description + metadata)
also provided in this file. More tests can easily be added by creating a new
configuration type and add it to the *_CONFIGS_LIST below.

By executing this script with python directly it is possible to regenerate the
reference data (e.g. for a new configurations). Please see the following command:

``python3 test_ad_integrators.py --help``

"""

import drjit as dr
import mitsuba as mi

import pytest, os, argparse
from os.path import join, exists

from mitsuba.scalar_rgb.test.util import fresolver_append_path
from mitsuba.scalar_rgb.test.util import find_resource

output_dir = find_resource('resources/data/tests/integrators')

# -------------------------------------------------------------------
#                          Test configs
# -------------------------------------------------------------------

from mitsuba.scalar_rgb import ScalarTransform4f as T

class ConfigBase:
    """
    Base class to configure test scene and define the parameter to update
    """
    requires_discontinuities = False

    def __init__(self) -> None:
        self.spp = 1024
        self.res = 128
        self.error_mean_threshold = 0.05
        self.error_max_threshold = 0.5
        self.error_mean_threshold_bwd = 0.05
        self.ref_fd_epsilon = 1e-3

        self.integrator_dict = {
            'max_depth': 2,
        }

        self.sensor_dict = {
            'type': 'perspective',
            'to_world': T().look_at(origin=[0, 0, 4], target=[0, 0, 0], up=[0, 1, 0]),
            'film': {
                'type': 'hdrfilm',
                'rfilter': { 'type': 'gaussian', 'stddev': 0.5 },
                'width': self.res,
                'height': self.res,
                'sample_border': True,
                'pixel_format': 'rgb',
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

        self.sensor_dict['film']['width'] = self.res
        self.sensor_dict['film']['height'] = self.res
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
               f'  integrator = { self.integrator_dict },\n' \
               f'  spp = {self.spp},\n' \
               f'  key = {self.key if hasattr(self, "key") else "None"}\n' \
               f']'

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
                'to_world': T().scale(0.25),
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
                'to_world': T().scale(0.25)
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
                'to_world': T().translate([1.25, 0.0, 1.0]) @ T().rotate([0, 1, 0], -90),
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
                'to_world': T().scale(0.25),
            },
            'light': {
                'type': 'rectangle',
                'to_world': T().translate([1.25, 0.0, 1.0]) @ T().rotate([0, 1, 0], -90),
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
                'to_world': T().scale(0.25),
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
                'to_world': T().scale(0.25),
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
            'to_world': T().look_at(origin=[0, 0, 4], target=[0, 0, 0], up=[0, 1, 0]),
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
#            Test configs with discontinuities
# -------------------------------------------------------------------

# Translate shape base configuration
class TranslateShapeConfigBase(ConfigBase):
    requires_discontinuities = True

    def __init__(self) -> None:
        super().__init__()

    def initialize(self):
        super().initialize()
        self.initial_state = mi.Vector3f(dr.unravel(mi.Vector3f, self.params[self.key]))

    def update(self, theta):
        self.params[self.key] = dr.ravel(self.initial_state + mi.Vector3f(theta, 0.0, 0.0))
        self.params.update()
        dr.eval()

# Scale shape base configuration
class ScaleShapeConfigBase(ConfigBase):
    requires_discontinuities = True

    def __init__(self) -> None:
        super().__init__()

    def initialize(self):
        super().initialize()
        self.initial_state = mi.Vector3f(dr.unravel(mi.Vector3f, self.params[self.key]))

    def update(self, theta):
        self.params[self.key] = dr.ravel(self.initial_state * (1.0 + theta))
        self.params.update()
        dr.eval()

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
                'to_world': T().rotate(angle=-90, axis=[0, 1, 0]),
            },
            'light': { 'type': 'constant' }
        }
        self.ref_fd_epsilon = 1e-3
        self.error_mean_threshold = 0.04
        self.error_max_threshold = 0.6
        self.error_mean_threshold_bwd = 0.25
        self.integrator_dict = {
            'max_depth': 2,
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
        self.integrator_dict = {
            'max_depth': 2,
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
                'to_world': T().translate([1.25, 0.0, 0.0]),
            }
        }
        self.ref_fd_epsilon = 1e-3
        self.error_mean_threshold = 0.03
        self.error_max_threshold = 1.0
        self.error_mean_threshold_bwd = 0.2
        self.integrator_dict = {
            'max_depth': 2,
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
                'to_world': T().translate([1.25, 0.0, 0.0]) @ T().rotate(angle=180, axis=[0, 1, 0]),
            }
        }
        self.ref_fd_epsilon = 1e-4
        self.error_mean_threshold = 0.08
        self.error_max_threshold = 2
        self.error_mean_threshold_bwd = 0.15


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
        self.error_mean_threshold = 0.08
        self.error_max_threshold = 0.5
        self.error_mean_threshold_bwd = 0.1
        self.integrator_dict = {
            'max_depth': 3,
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
                'to_world': T().translate([2.0, 0.0, 2.0]) @ T().scale(0.25),
            },
            'light': {
                'type': 'obj',
                'filename': 'resources/data/common/meshes/sphere.obj',
                'emitter': {
                    'type': 'area',
                    'radiance': {'type': 'rgb', 'value': [1000.0, 1000.0, 1000.0]}
                },
                'to_world': T().translate([4.0, 0.0, 4.0]) @ T().scale(0.05)
            }
        }
        self.ref_fd_epsilon = 1e-3
        self.error_mean_threshold = 0.02
        self.error_max_threshold = 0.8
        self.error_mean_threshold_bwd = 0.25
        self.integrator_dict = {
            'max_depth': 2,
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
                'to_world': T().translate([2.0, 0.0, 2.0]) @ T().scale(0.25),
            },
            # 'light': {
            #     'type': 'obj',
            #     'filename': 'resources/data/common/meshes/sphere.obj',
            #     'emitter': {
            #         'type': 'area',
            #         'radiance': {'type': 'rgb', 'value': [1000.0, 1000.0, 1000.0]}
            #     },
            #     'to_world': T().translate([4.0, 0.0, 4.0]) @ T().scale(0.05)
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
                        'filename' : 'resources/data/common/textures/museum.exr',
                        'format' : 'variant'
                    }
                },
                'to_world': T().scale(2.0),
            },
            'light': { 'type': 'constant' }
        }
        self.res = 64
        self.ref_fd_epsilon = 1e-3
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
                'to_world': T().translate([-1, 0, 0.5]) @ T().rotate([0, 1, 0], 90) @ T().scale(1.0),
            },
            'light': {
                'type': 'point',
                'position': [-4, 0, 6],
                'intensity': {'type': 'rgb', 'value': [5.0, 0.0, 0.0]}
            },
            'light2': { 'type': 'constant', 'radiance': 0.1 },
        }
        self.error_mean_threshold = 0.06
        self.error_max_threshold = 0.7
        self.error_mean_threshold_bwd = 0.35
        self.integrator_dict = {
            'max_depth': 3,
        }

    def initialize(self):
        super().initialize()
        self.params.keep(['plane.vertex_positions', 'occluder.vertex_positions'])
        self.initial_state_0 = dr.unravel(mi.Vector3f, self.params['plane.vertex_positions'])
        self.initial_state_1 = dr.unravel(mi.Vector3f, self.params['occluder.vertex_positions'])

    def update(self, theta):
        self.params['plane.vertex_positions']    = dr.ravel(self.initial_state_0 + mi.Vector3f(theta, 0.0, 0.0))
        self.params['occluder.vertex_positions'] = dr.ravel(self.initial_state_1 + mi.Vector3f(theta, 0.0, 0.0))
        self.params.update()
        dr.eval()


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
                'to_world': T().translate([0, 1.5, 0]) @ T().rotate([1, 0, 0], -45) @ T().scale(4),
            },
            'sphere': {
                'type': 'obj',
                'bsdf': {
                    'type': 'diffuse',
                    'reflectance': {'type': 'rgb', 'value': [1.0, 0.5, 0.0]}
                },
                'filename': 'resources/data/common/meshes/sphere.obj',
                'to_world': T().translate([0.5, 2.0, 1.5]) @ T().scale(1.0),
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
        }


# Translate camera
class TranslateCameraConfig(ConfigBase):
    def __init__(self) -> None:
        super().__init__()
        self.key = 'sensor.to_world'
        self.scene_dict = {
            'type': 'scene',
            'sphere': {
                'type': 'obj',
                'filename': 'resources/data/common/meshes/sphere.obj',
            },
            'light': { 'type': 'constant' }
        }
        self.error_mean_threshold = 0.3
        self.error_max_threshold = 1.6
        self.error_mean_threshold_bwd = 1.3
        self.res = 16
        self.ref_fd_epsilon = 1e-3
        self.integrator_dict = {
            'max_depth': 2,
        }

    def initialize(self):
        super().initialize()
        self.params.keep([self.key])
        self.initial_state = mi.Transform4f(self.params[self.key])

    def update(self, theta):
        self.params[self.key] = mi.Transform4f(self.params[self.key]) @ mi.Transform4f.translate([theta, 0.0, 0.0])
        self.params.update()
        dr.eval()


# Rotate plane's shading normals (no discontinuities)
class RotateShadingNormalsPlaneConfig(ConfigBase):
    def __init__(self) -> None:
        super().__init__()
        self.key = 'plane.vertex_normals'
        self.scene_dict = {
            'type': 'scene',
            'plane': {
                'type': 'obj',
                'filename': 'resources/data/common/meshes/rectangle.obj',
                'bsdf': { 'type': 'diffuse' },
            },
            'light': {
                'type': 'rectangle',
                'to_world': T().translate([1.25, 0.0, 1.0]) @ T().rotate([0, 1, 0], -90),
                'emitter': {
                    'type': 'area',
                    'radiance': {'type': 'rgb', 'value': [3.0, 3.0, 3.0]}
                }
            }
        }
        self.integrator_dict = {
            'max_depth': 2,
        }
        self.error_mean_threshold = 0.02
        self.error_max_threshold = 0.3

    def initialize(self):
        super().initialize()
        self.params.keep([self.key])
        self.initial_state = mi.Float(self.params[self.key])

    def update(self, theta):
        self.params[self.key] = dr.ravel(
            mi.Transform4f().rotate(angle=theta, axis=[0.0, 1.0, 0.0]) @
            dr.unravel(mi.Normal3f, self.initial_state)
        )
        self.params.update()
        dr.eval()


# -------------------------------------------------------------------
#                           List configs
# -------------------------------------------------------------------

BASIC_CONFIGS_LIST = [
    DiffuseAlbedoConfig,
    DiffuseAlbedoGIConfig,
    AreaLightRadianceConfig,
    DirectlyVisibleAreaLightRadianceConfig,
    TranslateTexturedPlaneConfig,
    CropWindowConfig,
    RotateShadingNormalsPlaneConfig,

    # The next two configs have issues with Nvidia driver v545
    # PointLightIntensityConfig,
    # ConstantEmitterRadianceConfig,
]

DISCONTINUOUS_CONFIGS_LIST = [
    # TranslateDiffuseSphereConstantConfig,
    # TranslateDiffuseRectangleConstantConfig,
    # TranslateRectangleEmitterOnBlackConfig,
    TranslateSphereEmitterOnBlackConfig,
    ScaleSphereEmitterOnBlackConfig,
    TranslateOccluderAreaLightConfig,
    TranslateSelfShadowAreaLightConfig,
    # TranslateShadowReceiverAreaLightConfig,
    TranslateSphereOnGlossyFloorConfig,
    # TranslateCameraConfig
]

# List of configs that fail on integrators with depth less than three
INDIRECT_ILLUMINATION_CONFIGS_LIST = [
    DiffuseAlbedoGIConfig,
    TranslateSelfShadowAreaLightConfig,
    TranslateSphereOnGlossyFloorConfig
]

# List of integrators to test (also indicates whether it handles discontinuities)
INTEGRATORS = [
    ('path', False),
    ('prb', False),
    ('direct_projective', True),
    ('prb_projective', True)
]

CONFIGS = []
for integrator_name, handles_discontinuities in INTEGRATORS:
    todos = BASIC_CONFIGS_LIST + (DISCONTINUOUS_CONFIGS_LIST if handles_discontinuities else [])
    for config in todos:
        if (('direct' in integrator_name or 'projective' in integrator_name) and
            config in INDIRECT_ILLUMINATION_CONFIGS_LIST):
            continue
        CONFIGS.append((integrator_name, config))

# -------------------------------------------------------------------
#                           Unit tests
# -------------------------------------------------------------------

@pytest.mark.slow
@pytest.mark.parametrize('integrator_name, config', CONFIGS)
def test01_rendering_primal(variants_all_ad_rgb, integrator_name, config):
    config = config()
    config.initialize()

    config.integrator_dict['type'] = integrator_name
    integrator = mi.load_dict(config.integrator_dict, parallel=False)

    filename = join(output_dir, f"test_{config.name}_image_primal_ref.exr")
    image_primal_ref = mi.TensorXf(mi.Bitmap(filename))
    image = integrator.render(config.scene, seed=0, spp=config.spp)

    error = dr.abs(image - image_primal_ref) / dr.maximum(dr.abs(image_primal_ref), 2e-2)
    error_mean = dr.mean(error, axis=None)
    error_max = dr.max(error, axis=None)

    if error_mean > config.error_mean_threshold  or error_max > config.error_max_threshold:
        print(f"Failure in config: {config.name}, {integrator_name}")
        print(f"-> error mean: {error_mean} (threshold={config.error_mean_threshold})")
        print(f"-> error max: {error_max} (threshold={config.error_max_threshold})")
        print(f'-> reference image: {filename}')
        filename = join(os.getcwd(), f"test_{integrator_name}_{config.name}_image_primal.exr")
        print(f'-> write current image: {filename}')
        mi.util.write_bitmap(filename, image)
        pytest.fail("Radiance values exceeded configuration's tolerances!")


@pytest.mark.slow
@pytest.mark.skipif(os.name == 'nt', reason='Skip those memory heavy tests on Windows')
@pytest.mark.parametrize('integrator_name, config', CONFIGS)
def test02_rendering_forward(variants_all_ad_rgb, integrator_name, config):
    config = config()
    config.initialize()

    config.integrator_dict['type'] = integrator_name
    integrator = mi.load_dict(config.integrator_dict)
    if 'projective' in integrator_name:
        integrator.proj_seed_spp = 2048 * 2

    filename = join(output_dir, f"test_{config.name}_image_fwd_ref.exr")
    image_fwd_ref = mi.TensorXf(mi.Bitmap(filename))

    theta = mi.Float(0.0)
    dr.enable_grad(theta)
    dr.set_label(theta, 'theta')
    config.update(theta)
    # We call dr.forward() here to propagate the gradient from the latent variable into
    # the scene parameter. This prevents dr.forward_to() in integrator.render_forward()
    # to trace gradients outside the dr.Loop().
    dr.forward(theta, dr.ADFlag.ClearEdges)

    dr.set_label(config.params, 'params')
    image_fwd = integrator.render_forward(
        config.scene, seed=0, spp=config.spp, params=theta)
    image_fwd = dr.detach(image_fwd)

    error = dr.abs(image_fwd - image_fwd_ref) / dr.maximum(dr.abs(image_fwd_ref), 2e-1)
    error_mean = dr.mean(error, axis=None)
    error_max = dr.max(error, axis=None)

    if error_mean > config.error_mean_threshold or error_max > config.error_max_threshold:
        print(f"Failure in config: {config.name}, {integrator_name}")
        print(f"-> error mean: {error_mean} (threshold={config.error_mean_threshold})")
        print(f"-> error max: {error_max} (threshold={config.error_max_threshold})")
        print(f'-> reference image: {filename}')
        filename = join(os.getcwd(), f"test_{integrator_name}_{config.name}_image_fwd.exr")
        print(f'-> write current image: {filename}')
        mi.util.write_bitmap(filename, image_fwd)
        filename = join(os.getcwd(), f"test_{integrator_name}_{config.name}_image_error.exr")
        print(f'-> write error image: {filename}')
        mi.util.write_bitmap(filename, error)
        pytest.fail("Gradient values exceeded configuration's tolerances!")


@pytest.mark.slow
@pytest.mark.skipif(os.name == 'nt', reason='Skip those memory heavy tests on Windows')
@pytest.mark.parametrize('integrator_name, config', CONFIGS)
def test03_rendering_backward(variants_all_ad_rgb, integrator_name, config):
    config = config()
    config.initialize()

    config.integrator_dict['type'] = integrator_name

    integrator = mi.load_dict(config.integrator_dict)
    if 'projective' in integrator_name:
        integrator.proj_seed_spp = 2048 * 2

    filename = join(output_dir, f"test_{config.name}_image_fwd_ref.exr")
    image_fwd_ref = mi.TensorXf(mi.Bitmap(filename))

    grad_in = 0.001
    image_adj = dr.full(mi.TensorXf, grad_in, image_fwd_ref.shape)

    theta = mi.Float(0.0)
    dr.enable_grad(theta)
    config.update(theta)

    # Higher spp will run into single-precision accumulation issues
    integrator.render_backward(
        config.scene, grad_in=image_adj, seed=0, spp=256, params=theta)

    grad = dr.grad(theta) / dr.width(image_fwd_ref)
    grad_ref = dr.mean(image_fwd_ref, axis=None) * grad_in

    error = dr.abs(grad - grad_ref) / dr.maximum(dr.abs(grad_ref), 1e-3)
    if error > config.error_mean_threshold_bwd:
        print(f"Failure in config: {config.name}, {integrator_name}")
        print(f"-> grad:     {grad}")
        print(f"-> grad_ref: {grad_ref}")
        print(f"-> error: {error} (threshold={config.error_mean_threshold_bwd})")
        print(f"-> ratio: {grad / grad_ref}")
        pytest.fail("Gradient values exceeded configuration's tolerances!")


@pytest.mark.slow
@pytest.mark.skipif(os.name == 'nt', reason='Skip those memory heavy tests on Windows')
def test04_render_custom_op(variants_all_ad_rgb):
    config = DiffuseAlbedoConfig()
    config.initialize()

    integrator = mi.load_dict({
        'type': 'prb',
        'max_depth': config.integrator_dict['max_depth']
    })

    filename = join(output_dir, f"test_{config.name}_image_primal_ref.exr")
    image_primal_ref = mi.TensorXf(mi.Bitmap(filename))

    filename = join(output_dir, f"test_{config.name}_image_fwd_ref.exr")
    image_fwd_ref = mi.TensorXf(mi.Bitmap(filename))

    theta = mi.Float(0.0)
    dr.enable_grad(theta)
    config.update(theta)

    # Higher spp will run into single-precision accumulation issues
    image_primal = mi.render(config.scene, config.params, integrator=integrator, seed=0, spp=256)

    error = dr.abs(dr.detach(image_primal) - image_primal_ref) / dr.maximum(dr.abs(image_primal_ref), 2e-2)
    error_mean = dr.mean(error, axis=None)
    error_max = dr.max(error, axis=None)

    if error_mean > config.error_mean_threshold  or error_max > config.error_max_threshold:
        print(f"Failure in config: {config.name}, {integrator_name}")
        print(f"-> error mean: {error_mean} (threshold={config.error_mean_threshold})")
        print(f"-> error max: {error_max} (threshold={config.error_max_threshold})")
        print(f'-> reference image: {filename}')
        filename = join(os.getcwd(), f"test_{integrator_name}_{config.name}_image_primal.exr")
        print(f'-> write current image: {filename}')
        mi.util.write_bitmap(filename, image_primal)
        pytest.fail("Radiance values exceeded configuration's tolerances!")

    # Backward comparison
    obj = dr.mean(image_primal, axis=None)
    dr.backward(obj)

    grad = dr.grad(theta)[0]
    grad_ref = dr.mean(image_fwd_ref, axis=None)

    error = dr.abs(grad - grad_ref) / dr.maximum(dr.abs(grad_ref), 1e-3)
    if error > config.error_mean_threshold:
        print(f"Failure in config: {config.name}, {integrator_name}")
        print(f"-> grad:     {grad}")
        print(f"-> grad_ref: {grad_ref}")
        print(f"-> error: {error} (threshold={config.error_mean_threshold})")
        print(f"-> ratio: {grad / grad_ref}")
        pytest.fail("Gradient values exceeded configuration's tolerances!")

    # Forward comparison
    theta = mi.Float(0.0)
    dr.enable_grad(theta)
    config.update(theta)

    image_primal = mi.render(config.scene, config.params, integrator=integrator, seed=0, spp=config.spp)

    dr.forward(theta)

    image_fwd = dr.grad(image_primal)

    error = dr.abs(image_fwd - image_fwd_ref) / dr.maximum(dr.abs(image_fwd_ref), 2e-1)
    error_mean = dr.mean(error, axis=None)
    error_max = dr.max(error, axis=None)

    if error_mean > config.error_mean_threshold or error_max > config.error_max_threshold:
        print(f"Failure in config: {config.name}, {integrator_name}")
        print(f"-> error mean: {error_mean} (threshold={config.error_mean_threshold})")
        print(f"-> error max: {error_max} (threshold={config.error_max_threshold})")
        filename = join(os.getcwd(), f"test_{integrator_name}_{config.name}_image_fwd.exr")
        mi.util.write_bitmap(filename, image_fwd)
        filename = join(os.getcwd(), f"test_{integrator_name}_{config.name}_image_error.exr")
        mi.util.write_bitmap(filename, error)
        pytest.fail("Gradient values exceeded configuration's tolerances!")

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

    mi.set_variant('cuda_ad_rgb', 'llvm_ad_rgb')

    if not exists(output_dir):
        os.makedirs(output_dir)

    for config in BASIC_CONFIGS_LIST + DISCONTINUOUS_CONFIGS_LIST:
        config = config()
        print(f"name: {config.name}")

        config.initialize()

        integrator_path = mi.load_dict({
            'type': 'path',
            'max_depth': config.integrator_dict['max_depth']
        })

        # Primal render
        image_ref = integrator_path.render(config.scene, seed=0, spp=args.spp)

        filename = join(output_dir, f"test_{config.name}_image_primal_ref.exr")
        mi.util.write_bitmap(filename, image_ref)

        # Finite difference
        theta = mi.Float(-0.5 * config.ref_fd_epsilon)
        config.update(theta)
        image_1 = integrator_path.render(config.scene, seed=0, spp=args.spp)
        dr.eval(image_1)

        theta = mi.Float(0.5 * config.ref_fd_epsilon)
        config.update(theta)
        image_2 = integrator_path.render(config.scene, seed=0, spp=args.spp)
        dr.eval(image_2)

        image_fd = (image_2 - image_1) / config.ref_fd_epsilon

        filename = join(output_dir, f"test_{config.name}_image_fwd_ref.exr")
        mi.util.write_bitmap(filename, image_fd)
