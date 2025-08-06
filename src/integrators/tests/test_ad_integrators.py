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
                'rfilter': { 'type': 'box' },
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
            return mi.load_dict(self.scene_dict, optimize=False)
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
        self.error_mean_threshold = 0.015
        self.error_max_threshold = 0.25
        self.error_mean_threshold_bwd = 0.0005
        self.ref_fd_epsilon = 1e-3


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
        self.error_mean_threshold = 0.04
        self.error_max_threshold = 0.4
        self.error_mean_threshold_bwd = 0.0005
        self.ref_fd_epsilon = 1e-3


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
        self.error_mean_threshold = 0.02
        self.error_max_threshold = 0.4
        self.error_mean_threshold_bwd = 0.0005
        self.ref_fd_epsilon = 1e-3


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
        self.error_mean_threshold = 0.02
        self.error_max_threshold = 0.2
        self.error_mean_threshold_bwd = 0.02
        self.ref_fd_epsilon = 1e-3


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
        self.error_mean_threshold = 0.02
        self.error_max_threshold = 0.2
        self.error_mean_threshold_bwd = 0.002
        self.ref_fd_epsilon = 1e-3


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
        self.error_mean_threshold = 0.02
        self.error_max_threshold = 0.1
        self.error_mean_threshold_bwd = 0.02
        self.ref_fd_epsilon = 1e-3


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
        self.error_mean_threshold = 0.01
        self.error_max_threshold = 0.2
        self.error_mean_threshold_bwd = 0.002
        self.ref_fd_epsilon = 1e-3


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
        self.error_mean_threshold_bwd = 0.00002

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
#            Test configs with discontinuities
# -------------------------------------------------------------------

# Translate shape base configuration
class TranslateShapeConfigBase(ConfigBase):
    requires_discontinuities = True

    def __init__(self) -> None:
        super().__init__()

    def initialize(self):
        super().initialize()
        self.initial_state = mi.Vector3f(dr.unravel(mi.Vector3f, mi.Float(self.params[self.key])))

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
        self.initial_state = mi.Vector3f(dr.unravel(mi.Vector3f, mi.Float(self.params[self.key])))

    def update(self, theta):
        self.params[self.key] = dr.ravel(self.initial_state * (1.0 + theta))
        self.params.update()
        dr.eval()


# Translate textured plane (this is actually a continuous problem)
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
                    'reflectance': {
                        'type': 'bitmap',
                        'filename' : 'resources/data/common/textures/museum.exr',
                        'format': 'variant'
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


# Translate diffuse plane illuminated by envmap towards the sensor
# (this is actually a continuous problem, and should have 0 gradients)
class TranslatePlaneUnderEnvmapConfig(TranslateShapeConfigBase):
    def __init__(self) -> None:
        super().__init__()
        self.key = 'plane.vertex_positions'
        self.scene_dict = {
            'type': 'scene',
            'plane': {
                'type': 'obj',
                'filename': 'resources/data/common/meshes/rectangle.obj',
                'face_normals': True,
                'bsdf': { 'type': 'diffuse' },
                'to_world': T().scale(3.0)
            },
            'light': {
                'type': 'envmap',
                'filename' : 'resources/data/common/textures/museum.exr'
            }
        }
        self.res = 64
        self.ref_fd_epsilon = 1e-3
        self.error_mean_threshold = 0.03
        self.error_max_threshold = 0.2

    def update(self, theta):
        self.params[self.key] = dr.ravel(self.initial_state + mi.Vector3f(0.0, 0.0, theta))
        self.params.update()
        dr.eval()


# Translate diffuse plane illuminated by a projector towards the sensor
# (this is actually a continuous problem)
class TranslatePlaneUnderProjectorConfig(TranslateShapeConfigBase):
    def __init__(self) -> None:
        super().__init__()
        self.key = 'plane.vertex_positions'
        self.scene_dict = {
            'type': 'scene',
            'plane': {
                'type': 'obj',
                'filename': 'resources/data/common/meshes/rectangle.obj',
                'face_normals': True,
                'bsdf': { 'type': 'diffuse' },
                'to_world': T().scale(3.0)
            },
            'light': {
                'type': 'projector',
                'irradiance': {
                    'type': 'bitmap',
                    'filename': 'resources/data/common/textures/gradient.jpg',
                    'format' : 'variant',
                },
                'to_world': T().look_at(origin=[0, 0, 3], target=[0, 0, 0], up=[0, 1, 0]),
            }
        }
        self.res = 64
        self.ref_fd_epsilon = 1e-3
        self.error_mean_threshold = 0.03
        self.error_max_threshold = 0.2

    def update(self, theta):
        self.params[self.key] = dr.ravel(self.initial_state + mi.Vector3f(0.0, 0.0, theta))
        self.params.update()
        dr.eval()


# Translate glass plane in a lens system (this is actually a continuous problem)
class TranslateGlassPlaneLensConfig(TranslateShapeConfigBase):
    def __init__(self) -> None:
        super().__init__()
        self.key = 'shape.vertex_positions'
        self.scene_dict = {
            'type': 'scene',
            'rough_glass': {
                'type': 'roughdielectric',
                'distribution': 'beckmann',
                'alpha': 0.8,
                'int_ior': 1.001,
                'ext_ior': 1.1,
            },
            'glass1': {
                'type': 'obj',
                'filename': 'resources/data/common/meshes/rectangle.obj',
                'face_normals': True,
                'to_world': T().translate([0, 0, 1]) @ T().scale(4.0),
                'bsdf': { 'type': 'ref', 'id': 'rough_glass' }
            },
            'shape': {
                'type': 'obj',
                'filename': 'resources/data/common/meshes/rectangle.obj',
                'face_normals': True,
                'to_world': T().translate([0, 0, 0]) @ T().scale(4.0),
                'bsdf': { 'type': 'ref', 'id': 'rough_glass' }
            },
            'glass3': {
                'type': 'obj',
                'filename': 'resources/data/common/meshes/rectangle.obj',
                'face_normals': True,
                'to_world': T().translate([0, 0, -1]) @ T().scale(4.0),
                'bsdf': { 'type': 'ref', 'id': 'rough_glass' }
            },
            'light': {
                'type': 'obj',
                'filename': 'resources/data/common/meshes/rectangle.obj',
                'face_normals': True,
                'to_world': T().translate([0, 0, -2]) @ T().scale(0.7),
                'emitter': {
                    'type': 'area',
                    'radiance': {
                        'type': 'bitmap',
                        'filename': 'resources/data/common/textures/gradient.jpg',
                        'wrap_mode': 'clamp',
                    }
                }
            }
        }
        self.integrator_dict = {
            'max_depth': 4,
        }
        self.sensor_dict = {
            'type': 'perspective',
            'to_world': T().look_at(origin=[0, 0, 2], target=[0, 0, 0], up=[0, 1, 0]),
            'film': {
                'type': 'hdrfilm',
                'rfilter': { 'type': 'box' },
                'width': self.res,
                'height': self.res,
                'sample_border': True,
                'pixel_format': 'rgb',
                'component_format': 'float32',
            }
        }
        self.spp = 8192
        self.res = 32
        self.ref_fd_epsilon = 1e-3
        self.error_mean_threshold = 0.03
        self.error_max_threshold = 0.3

    def update(self, theta):
        self.params[self.key] = dr.ravel(self.initial_state + mi.Vector3f(0.0, 0.0, theta))
        self.params.update()
        dr.eval()


# Translate textured area emitter
# (actually continuous, the border of the emitter is black)
class TranslateTexturedAreaEmitterConfig(TranslateShapeConfigBase):
    def __init__(self) -> None:
        super().__init__()
        self.key = 'plane.vertex_positions'
        self.res = 64
        self.scene_dict = {
            'type': 'scene',
            'plane': {
                'type': 'obj',
                'filename': 'resources/data/common/meshes/rectangle.obj',
                'face_normals': True,
                'to_world':  T().rotate([1, 0, 0], 45) @ T().rotate([0, 0, 1], -90) @ T().scale(1.0),
                'emitter': {
                    'type': 'area',
                    'radiance': {
                        'type': 'bitmap',
                        'filename': 'resources/data/common/textures/gradient.jpg',
                    }
                }
            }
        }
        self.sensor_dict = {
            'type': 'perspective',
            'to_world': T().look_at(origin=[0, 0.2, 4], target=[0, 0.2, 0], up=[0, 1, 0]),
            'fov': 40,
            'film': {
                'type': 'hdrfilm',
                'rfilter': { 'type': 'box' },
                'width': self.res,
                'height': self.res,
                'sample_border': True,
                'pixel_format': 'rgb',
                'component_format': 'float32',
            }
        }
        self.ref_fd_epsilon = 1e-3
        self.error_mean_threshold = 0.003
        self.error_max_threshold = 0.4
        self.error_mean_threshold_bwd = 0.25

    def update(self, theta):
        self.params[self.key] = dr.ravel(self.initial_state + mi.Vector3f(0.0, 0.0, theta))
        self.params.update()
        dr.eval()

# Translate plane illuminated by a textured area emitter
# (actually continuous, the border of the emitter is black, and we don't directly see the plane borders)
class TranslatePlaneUnderTexturedAreaEmitterConfig(TranslateShapeConfigBase):
    def __init__(self) -> None:
        super().__init__()
        self.key = 'plane.vertex_positions'
        self.res = 64
        self.scene_dict = {
            'type': 'scene',
            'plane': {
                'type': 'obj',
                'filename': 'resources/data/common/meshes/rectangle.obj',
                'face_normals': True,
                'to_world':  T().rotate([1, 0, 0], 45) @ T().rotate([0, 0, 1], 90) @ T().scale(1),
            },
            'light': {
                'type': 'obj',
                'filename': 'resources/data/common/meshes/rectangle.obj',
                'face_normals': True,
                'to_world': T().translate([0, -1.5, 0]) @ T().rotate([0, 1, 0], -90) @ T().rotate([1, 0, 0], -90) @ T().scale(4.0),
                'emitter': {
                    'type': 'area',
                    'radiance': {
                        'type': 'bitmap',
                        'filename': 'resources/data/common/textures/gradient.jpg',
                        'wrap_mode': 'clamp',
                    }
                }
            }
        }
        self.sensor_dict = {
            'type': 'perspective',
            'to_world': T().look_at(origin=[0, 0.1, 4], target=[0, 0.1, 0], up=[0, 1, 0]),
            'fov': 18,
            'film': {
                'type': 'hdrfilm',
                'rfilter': { 'type': 'box' },
                'width': self.res,
                'height': self.res,
                'sample_border': True,
                'pixel_format': 'rgb',
                'component_format': 'float32',
            }
        }
        self.ref_fd_epsilon = 1e-3
        self.error_mean_threshold = 0.025
        self.error_max_threshold = 0.4
        self.error_mean_threshold_bwd = 0.25

    def update(self, theta):
        self.params[self.key] = dr.ravel(self.initial_state + mi.Vector3f(0.0, 0.0, theta))
        self.params.update()
        dr.eval()

# Plane illuminated by a moving textured area emitter
# (actually continuous, the border of the emitter is black, and we don't directly see the plane borders)
class TranslateTexturedAreaEmitterIlluminatingPlaneConfig(TranslateShapeConfigBase):
    def __init__(self) -> None:
        super().__init__()
        self.key = 'light.vertex_positions'
        self.res = 64
        self.scene_dict = {
            'type': 'scene',
            'plane': {
                'type': 'obj',
                'filename': 'resources/data/common/meshes/rectangle.obj',
                'face_normals': True,
                'to_world':  T().rotate([1, 0, 0], 45) @ T().rotate([0, 0, 1], 90) @ T().scale(1),
            },
            'light': {
                'type': 'obj',
                'filename': 'resources/data/common/meshes/rectangle.obj',
                'face_normals': True,
                'to_world': T().translate([0, -1.5, 0]) @ T().rotate([0, 1, 0], -90) @ T().rotate([1, 0, 0], -90) @ T().scale(4.0),
                'emitter': {
                    'type': 'area',
                    'radiance': {
                        'type': 'bitmap',
                        'filename': 'resources/data/common/textures/gradient.jpg',
                        'wrap_mode': 'clamp',
                    }
                }
            }
        }
        self.sensor_dict = {
            'type': 'perspective',
            'to_world': T().look_at(origin=[0, 0.1, 4], target=[0, 0.1, 0], up=[0, 1, 0]),
            'fov': 18,
            'film': {
                'type': 'hdrfilm',
                'rfilter': { 'type': 'box' },
                'width': self.res,
                'height': self.res,
                'sample_border': True,
                'pixel_format': 'rgb',
                'component_format': 'float32',
            }
        }
        self.ref_fd_epsilon = 1e-3
        self.error_mean_threshold = 0.025
        self.error_max_threshold = 0.4
        self.error_mean_threshold_bwd = 0.25

    def update(self, theta):
        self.params[self.key] = dr.ravel(self.initial_state + mi.Vector3f(0.0, 0.0, theta))
        self.params.update()
        dr.eval()



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


# Translate area emitter (disk) on gray background
class TranslateDiskEmitterOnGrayConfig(ConfigBase):
    def __init__(self) -> None:
        super().__init__()
        self.key = 'disk.to_world'
        self.scene_dict = {
            'type': 'scene',
            'disk': {
                'type': 'disk',
                'to_world': T().translate([0.2, 0, -1]) @ T().rotate([0, 1, 0], 60).scale(1.5),
                'emitter': {
                    'type': 'area',
                    'radiance': {'type': 'rgb', 'value': 2 }
                },
            },
            'light': {
                'type': 'constant',
                'radiance': {
                    'type': 'rgb',
                    'value': 0.2,
                }
            },
        }
        self.res = 64
        self.ref_fd_epsilon = 1e-3
        self.error_mean_threshold = 0.03
        self.error_max_threshold = 1.0
        self.error_mean_threshold_bwd = 0.2
        self.integrator_dict = {
            'max_depth': 1,
            'sppc': 0,
            'sppi': 0,
        }
        self.sensor_dict = {
            'type': 'perspective',
            'to_world': T().look_at(origin=[0, 0, 1], target=[0, 0, 0], up=[0, 1, 0]),
            'fov': 120,
            'near_clip': 0.01,
            'film': {
                'type': 'hdrfilm',
                'rfilter': {'type': 'box'},
                'width': self.res,
                'height': self.res,
                'sample_border': True,
                'pixel_format': 'rgb',
                'component_format': 'float32',
            }
        }

    def initialize(self):
        super().initialize()
        self.params.keep([self.key])
        self.initial_state = mi.Transform4f(self.params[self.key])

    def update(self, theta):
        self.params[self.key] = mi.Transform4f().translate([theta, 0, 0]) @ self.initial_state
        self.params.update()
        dr.eval()


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
        self.res = 64
        self.ref_fd_epsilon = 1e-3
        self.error_mean_threshold = 0.08
        self.error_max_threshold = 0.5
        self.error_mean_threshold_bwd = 0.1


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
        self.spp = 4192 * 4
        self.ref_fd_epsilon = 1e-3
        self.error_mean_threshold = 0.03
        self.error_max_threshold = 1.75
        self.error_mean_threshold_bwd = 0.25


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
        self.ref_fd_epsilon = 1e-4
        self.error_mean_threshold = 0.02
        self.error_max_threshold = 0.5
        self.error_mean_threshold_bwd = 0.25
        self.spp = 4096
        self.integrator_dict = {
            'max_depth': 2,
        }


# Plane illuminated by a textured area emitter: both are moving
class TranslatePlaneAndAreaEmitterConfig(ConfigBase):
    def __init__(self) -> None:
        super().__init__()
        self.key1 = 'plane.vertex_positions'
        self.key2 = 'light.vertex_positions'
        self.scene_dict = {
            'type': 'scene',
            'plane': {
                'type': 'obj',
                'filename': 'resources/data/common/meshes/rectangle.obj',
                'face_normals': True,
                'to_world': T().look_at(origin=[0,  0, -1], target=[0, -1,  0], up=[0, 1, 0]).scale(0.25),
            },
            'light': {
                'type': 'obj',
                'filename': 'resources/data/common/meshes/rectangle.obj',
                'face_normals': True,
                'to_world': T().look_at(origin=[0, -2, -1], target=[0, 0,  -1], up=[0, 0, 1]).scale(0.75),
                'emitter': {
                    'type': 'area',
                    'radiance': {
                        'type': 'rgb',
                        'value': 1.0,
                    }
                }
            },
        }
        self.ref_fd_epsilon = 1e-3
        self.error_mean_threshold = 0.01
        self.error_max_threshold = 0.1
        self.error_mean_threshold_bwd = 0.25
        self.spp = 4096
        self.integrator_dict = {
            'max_depth': 2,
            'guiding': 'grid',
        }
        self.sensor_dict = {
            'type': 'perspective',
            'to_world': T().look_at(origin=[0, 0,  0], target=[0, 0, -1], up=[0, 1, 0]),
            'film': {
                'type': 'hdrfilm',
                'rfilter': { 'type': 'box' },
                'width': self.res,
                'height': self.res,
                'sample_border': True,
                'pixel_format': 'rgb',
                'component_format': 'float32',
            }
        }

    def initialize(self):
        super().initialize()
        self.params.keep([self.key1, self.key2])
        self.initial_state1 = mi.Vector3f(dr.unravel(mi.Vector3f, mi.Float(self.params[self.key1])))
        self.initial_state2 = mi.Vector3f(dr.unravel(mi.Vector3f, mi.Float(self.params[self.key2])))

    def update(self, theta):
        self.params[self.key1] = dr.ravel(self.initial_state1 + mi.Vector3f(0.0, 0.0, theta))
        self.params[self.key2] = dr.ravel(self.initial_state2 + mi.Vector3f(0.0, 0.0, theta))
        self.params.update()
        dr.eval()


# Two perpendicular planes are moving with the same motion,
# an area emitter casts the shadow of one plane onto the other
class TranslateSelfShadowingRightAngleConfig(ConfigBase):
    def __init__(self) -> None:
        super().__init__()
        self.key1 = 'wall.vertex_positions'
        self.key2 = 'floor.vertex_positions'
        self.scene_dict = {
            'type': 'scene',
            'wall': {
                'type': 'obj',
                'filename': 'resources/data/common/meshes/rectangle.obj',
                'face_normals': True,
                'to_world': T().look_at(origin=[0,  0, -1], target=[0, 0,  0], up=[0, 1, 0]),
                'material': {
                    'type': 'diffuse',
                    'reflectance' : { 'type': 'rgb', 'value': [0.1, 0.1, 0.4] }
                }
            },
            'floor': {
                'type': 'obj',
                'filename': 'resources/data/common/meshes/rectangle.obj',
                'face_normals': True,
                'to_world': T().look_at(origin=[0, -1, -1], target=[0, 0,  -1], up=[0, 0, 1]),
                'material': {
                    'type': 'diffuse',
                    'reflectance' : { 'type': 'rgb', 'value': [0.4, 0.1, 0.1] }
                }
            },
            'light': {
                'type': 'rectangle',
                'to_world': T().translate(mi.ScalarPoint3f(0, 8, -4)).scale(mi.ScalarVector3f(2, 0.5, 1)),
                'emitter': {
                    'type': 'area',
                    'radiance': { 'type': 'rgb', 'value': 1 }
                }
            },
        }
        self.ref_fd_epsilon = 1e-3
        self.error_mean_threshold = 0.01
        self.error_max_threshold = 0.05
        self.error_mean_threshold_bwd = 0.25
        self.spp = 4096
        self.integrator_dict = {
            'max_depth': 2,
            'guiding': 'grid',
        }
        self.sensor_dict = {
            'type': 'perspective',
            'to_world': T().look_at(origin=[0, 0,  1], target=[0, 0, -1], up=[0, 1, 0]),
            'fov': 87,
            'film': {
                'type': 'hdrfilm',
                'rfilter': { 'type': 'box' },
                'width': self.res,
                'height': self.res,
                'sample_border': True,
                'pixel_format': 'rgb',
                'component_format': 'float32',
            }
        }

    def initialize(self):
        super().initialize()
        self.params.keep([self.key1, self.key2])
        self.initial_state1 = mi.Vector3f(dr.unravel(mi.Vector3f, mi.Float(self.params[self.key1])))
        self.initial_state2 = mi.Vector3f(dr.unravel(mi.Vector3f, mi.Float(self.params[self.key2])))

    def update(self, theta):
        self.params[self.key1] = dr.ravel(self.initial_state1 + mi.Vector3f(0.0, 0.0, theta))
        self.params[self.key2] = dr.ravel(self.initial_state2 + mi.Vector3f(0.0, 0.0, theta))
        self.params.update()
        dr.eval()


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
    CropWindowConfig,
    RotateShadingNormalsPlaneConfig
]

CONTINUOUS_BUT_NON_STATIC_GEOM_CONFIGS_LIST = [
    TranslateTexturedPlaneConfig,
    TranslatePlaneUnderEnvmapConfig,
    TranslatePlaneUnderProjectorConfig,
    TranslateGlassPlaneLensConfig,
    TranslateTexturedAreaEmitterConfig,
    TranslatePlaneUnderTexturedAreaEmitterConfig,
    TranslateTexturedAreaEmitterIlluminatingPlaneConfig
]

DISCONTINUOUS_CONFIGS_LIST = [
    TranslateDiffuseRectangleConstantConfig,
    TranslateRectangleEmitterOnBlackConfig,
    TranslateDiskEmitterOnGrayConfig,
    TranslateSphereEmitterOnBlackConfig,
    ScaleSphereEmitterOnBlackConfig,
    TranslateOccluderAreaLightConfig,
    TranslateShadowReceiverAreaLightConfig,
    TranslatePlaneAndAreaEmitterConfig,
    TranslateSelfShadowingRightAngleConfig

#    TranslateCameraConfig
]

# List of configs that fail on integrators with depth less than three
INDIRECT_ILLUMINATION_CONFIGS_LIST = [
    DiffuseAlbedoGIConfig,
    TranslateGlassPlaneLensConfig
]

# List of integrators to test they are triplets:
# (integrator type, handles continuous derivaites w/ moving geometry, handle discontinuities)
INTEGRATORS = [
    ('path', False, False),
    ('prb', True, False),
    ('direct_projective', True, True),
    ('prb_projective', True, True),
]

CONFIGS = []
for integrator_name, handles_moving_geom, handles_discontinuities in INTEGRATORS:
    todos = (
        BASIC_CONFIGS_LIST +
        (CONTINUOUS_BUT_NON_STATIC_GEOM_CONFIGS_LIST if handles_moving_geom else []) +
        (DISCONTINUOUS_CONFIGS_LIST if handles_discontinuities else [])
    )
    for config in todos:
        if (('direct' in integrator_name) and config in INDIRECT_ILLUMINATION_CONFIGS_LIST):
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
    image_primal_ref = mi.TensorXf32(mi.Bitmap(filename))
    image = integrator.render(config.scene, seed=0, spp=config.spp)

    if not check_image_error(image, image_primal_ref, config, integrator_name,
                             epsilon=2e-2, test_type='primal'):
        pytest.fail("Radiance values exceeded configuration's tolerances!")


@pytest.mark.slow
@pytest.mark.skipif(os.name == 'nt', reason='Skip those memory heavy tests on Windows')
@pytest.mark.parametrize('integrator_name, config', CONFIGS)
def test02_rendering_forward(variants_all_ad_rgb, integrator_name, config):
    if mi.is_polarized:
        pytest.skip('Test must be adapted to polarized rendering.')

    config = config()
    config.initialize()

    config.integrator_dict['type'] = integrator_name
    integrator = mi.load_dict(config.integrator_dict)
    if 'projective' in integrator_name:
        integrator.proj_seed_spp = 4192

    filename = join(output_dir, f"test_{config.name}_image_fwd_ref.exr")
    image_fwd_ref = mi.TensorXf32(mi.Bitmap(filename))

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

    if not check_image_error(image_fwd, image_fwd_ref, config, integrator_name,
                             epsilon=2e-1, test_type='fwd', save_error_image=True):
        pytest.fail("Gradient values exceeded configuration's tolerances!")


@pytest.mark.slow
@pytest.mark.skipif(os.name == 'nt', reason='Skip those memory heavy tests on Windows')
@pytest.mark.parametrize('integrator_name, config', CONFIGS)
def test03_rendering_backward(variants_all_ad_rgb, integrator_name, config):
    if mi.is_polarized:
        pytest.skip('Test must be adapted to polarized rendering.')

    config = config()
    config.initialize()

    config.integrator_dict['type'] = integrator_name

    integrator = mi.load_dict(config.integrator_dict)
    if 'projective' in integrator_name:
        integrator.proj_seed_spp = 4192

    filename = join(output_dir, f"test_{config.name}_image_fwd_ref.exr")
    image_fwd_ref = mi.TensorXf32(mi.Bitmap(filename))

    grad_in = 0.001
    image_adj = dr.full(mi.TensorXf, grad_in, image_fwd_ref.shape)

    theta = mi.Float(0.0)
    dr.enable_grad(theta)
    config.update(theta)

    # Higher spp will run into single-precision accumulation issues
    # Use 256 for TranslateTexturedPlaneConfig due to high variance, 128 for others
    backward_spp = 256 if isinstance(config, TranslateTexturedPlaneConfig) else 128
    integrator.render_backward(
        config.scene, grad_in=image_adj, seed=0, spp=backward_spp, params=theta)

    grad = dr.grad(theta) / dr.width(image_fwd_ref)
    grad_ref = dr.mean(image_fwd_ref, axis=None) * grad_in

    if not check_gradient_error(grad, grad_ref, config, integrator_name):
        pytest.fail("Gradient values exceeded configuration's tolerances!")


@pytest.mark.slow
@pytest.mark.skipif(os.name == 'nt', reason='Skip those memory heavy tests on Windows')
def test04_render_custom_op(variants_all_ad_rgb):
    if mi.is_polarized:
        pytest.skip('Test must be adapted to polarized rendering.')

    config = DiffuseAlbedoConfig()
    config.initialize()

    integrator = mi.load_dict({
        'type': 'prb',
        'max_depth': config.integrator_dict['max_depth']
    })

    filename = join(output_dir, f"test_{config.name}_image_primal_ref.exr")
    image_primal_ref = mi.TensorXf32(mi.Bitmap(filename))

    filename = join(output_dir, f"test_{config.name}_image_fwd_ref.exr")
    image_fwd_ref = mi.TensorXf32(mi.Bitmap(filename))

    theta = mi.Float(0.0)
    dr.enable_grad(theta)
    config.update(theta)

    # Higher spp will run into single-precision accumulation issues
    image_primal = mi.render(config.scene, config.params, integrator=integrator, seed=0, spp=256)

    integrator_name = 'prb'  # Fix missing integrator_name
    if not check_image_error(dr.detach(image_primal), image_primal_ref, config, integrator_name,
                           epsilon=2e-2, test_type='primal'):
        pytest.fail("Radiance values exceeded configuration's tolerances!")

    # Backward comparison
    obj = dr.mean(image_primal, axis=None)
    dr.backward(obj)

    grad = dr.grad(theta)[0]
    grad_ref = dr.mean(image_fwd_ref, axis=None)

    if not check_gradient_error(grad, grad_ref, config, integrator_name,
                              threshold_attr='error_mean_threshold'):
        pytest.fail("Gradient values exceeded configuration's tolerances!")

    # Forward comparison
    theta = mi.Float(0.0)
    dr.enable_grad(theta)
    config.update(theta)

    image_primal = mi.render(config.scene, config.params, integrator=integrator, seed=0, spp=config.spp)

    dr.forward(theta)

    image_fwd = dr.grad(image_primal)

    if not check_image_error(image_fwd, image_fwd_ref, config, integrator_name,
                           epsilon=2e-1, test_type='fwd', save_error_image=True):
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

    all_configs = (
        BASIC_CONFIGS_LIST +
        CONTINUOUS_BUT_NON_STATIC_GEOM_CONFIGS_LIST +
        DISCONTINUOUS_CONFIGS_LIST
    )
    for config in all_configs:
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
