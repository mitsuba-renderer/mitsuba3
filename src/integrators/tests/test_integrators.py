"""
This file describes a set of test scene to validate the correctness of our
various integrators and the adjoint implementation.
"""
import pytest

import mitsuba
import enoki as ek

import os
from os.path import join, realpath, exists
import argparse

from mitsuba.python.test.util import fresolver_append_path

def create_sensor(res):
    from mitsuba.core import ScalarTransform4f as T
    return {
        'type': 'perspective',
        'to_world': T.look_at(origin=[0, 0, 4], target=[0, 0, 0], up=[0, 1, 0]),
        'film': {
            'type': 'hdrfilm',
            'rfilter': { 'type': 'gaussian', 'stddev': 0.5 },
            # 'rfilter': { 'type': 'tent' },
            'width': res,
            'height': res,
            'high_quality_edges': True,
        }
    }

output_dir = realpath(join(os.path.dirname(__file__), '../../../resources/data/tests/integrators'))

def load_image(filename):
    import numpy as np
    from mitsuba.core import TensorXf, Bitmap
    return TensorXf(np.array(Bitmap(filename)))

# -------------------------------------------------------------------
#                          Test configs
# -------------------------------------------------------------------

mitsuba.set_variant('scalar_rgb')
from mitsuba.core import ScalarTransform4f as T

class Config:
    """Base class to configure test scene and define the parameter to update"""
    def __init__(self) -> None:
        self.spp = 1024
        self.res = 32
        self.max_depth = 3
        self.error_mean_threshold = 0.05
        self.error_max_threshold = 0.2

    def initialize(self):
        from mitsuba.core import xml
        from mitsuba.python.util import traverse
        self.scene_dict['camera'] = create_sensor(res=self.res)

        @fresolver_append_path
        def create_scene():
            return xml.load_dict(self.scene_dict)
        self.scene = create_scene()

        self.params = traverse(self.scene)
        self.params.keep([self.key])

        self.initial_state = type(self.params[self.key])(self.params[self.key])

    def update(self, theta):
        """This method update the scene parameter associated to this config"""
        self.params[self.key] = self.initial_state + theta
        ek.set_label(self.params, 'params')
        self.params.update()
        ek.eval()

# BSDF albedo of a directly visible gray plane illuminated by a constant emitter
class DiffuseAlbedoConfig(Config):
    def __init__(self) -> None:
        super().__init__()
        self.name = 'diffuse_albedo'
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
class DiffuseAlbedoGIConfig(Config):
    def __init__(self) -> None:
        super().__init__()
        self.name = 'diffuse_albedo_global_illumination'
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
class AreaLightRadianceConfig(Config):
    def __init__(self) -> None:
        super().__init__()
        self.name = 'area_light_radiance'
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
class DirectlyVisibleAreaLightRadianceConfig(Config):
    def __init__(self) -> None:
        super().__init__()
        self.name = 'directly_visible_area_light_radiance'
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
class PointLightIntensityConfig(Config):
    def __init__(self) -> None:
        super().__init__()
        self.name = 'point_light_intensity'
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
class ConstantEmitterRadianceConfig(Config):
    def __init__(self) -> None:
        super().__init__()
        self.name = 'constant_emitter_radiance'
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


# Translate shape base configuration
class TranslateShapeConfig(Config):
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
class ScaleShapeConfig(Config):
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
class TranslateRectangleEmitterOnBlackConfig(TranslateShapeConfig):
    def __init__(self) -> None:
        super().__init__()
        self.name = 'translate_rect_emitter_on_black'
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
        self.spp = 12000
        self.error_mean_threshold = 0.1
        self.error_max_threshold = 1.0


# Translate area emitter (sphere) on black background
class TranslateSphereEmitterOnBlackConfig(TranslateShapeConfig):
    def __init__(self) -> None:
        super().__init__()
        self.name = 'translate_sphere_emitter_on_black'
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
        self.spp = 12000
        self.error_mean_threshold = 0.1
        self.error_max_threshold = 1.0

# Scale area emitter (sphere) on black background
class ScaleSphereEmitterOnBlackConfig(ScaleShapeConfig):
    def __init__(self) -> None:
        super().__init__()
        self.name = 'scale_sphere_emitter_on_black'
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
        self.spp = 12000
        self.error_mean_threshold = 0.1
        self.error_max_threshold = 1.5


# Translate occluder (sphere) casting shadow on gray wall
class TranslateOccluderPointLightConfig(TranslateShapeConfig):
    def __init__(self) -> None:
        super().__init__()
        self.name = 'translate_occluder_pointlight'
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
                'type': 'point',
                'position': [4.0, 0.0, 4.0],
            }
        }
        self.spp = 12000


# Scale occluder (sphere) casting shadow on gray wall
class ScaleOccluderPointLightConfig(ScaleShapeConfig):
    def __init__(self) -> None:
        super().__init__()
        self.name = 'scale_occluder_pointlight'
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
                'type': 'point',
                'position': [4.0, 0.0, 4.0],
            }
        }
        self.spp = 12000
        self.error_mean_threshold = 0.2
        self.error_max_threshold = 1.0


# Translate occluder (sphere) casting shadow on gray wall
class TranslateOccluderAreaLightConfig(TranslateShapeConfig):
    def __init__(self) -> None:
        super().__init__()
        self.name = 'translate_occluder_arealight'
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


# -------------------------------------------------------------------
#                           Unit tests
# -------------------------------------------------------------------

BASIC_CONFIGS = [
    # DiffuseAlbedoConfig,
    # DiffuseAlbedoGIConfig,
    # AreaLightRadianceConfig,
    # DirectlyVisibleAreaLightRadianceConfig,
    # PointLightIntensityConfig,
    # ConstantEmitterRadianceConfig,
]

REPARAM_CONFIGS = [
    # TranslateRectangleEmitterOnBlackConfig,
    # TranslateSphereEmitterOnBlackConfig,
    ScaleSphereEmitterOnBlackConfig,
    # TranslateOccluderPointLightConfig,
    # ScaleOccluderPointLightConfig,
    # TranslateOccluderAreaLightConfig,
]

# List of integrators to test (also indicates whether it handles discontinuities)
INTEGRATORS = [
    # ('path', False),
    # ('rb', False),
    # ('prb', False),
    ('rbreparam', True),
    # ('prbreparam', True),
]

CONFIGS = []
for integrator_name, reparam in INTEGRATORS:
    todos = BASIC_CONFIGS + (REPARAM_CONFIGS if reparam else [])
    for config in todos:
        CONFIGS.append((integrator_name, config))


@pytest.mark.slow
@pytest.mark.parametrize('integrator_name, config', CONFIGS)
def test01_rendering_primal(integrator_name, config):
    import mitsuba
    mitsuba.set_variant('cuda_ad_rgb')

    from mitsuba.core import xml
    from mitsuba.python.util import write_bitmap

    config = config()
    config.initialize()

    integrator = xml.load_dict({
        'type': integrator_name,
        'max_depth': config.max_depth
    })

    filename = join(output_dir, f"test_{config.name}_image_primal_ref.exr")
    image_primal_ref = load_image(filename)

    image = integrator.render(config.scene, seed=0, spp=config.spp)

    error = ek.abs(image - image_primal_ref) / ek.max(ek.abs(image_primal_ref), 1e-1)
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
def test02_rendering_forward(integrator_name, config):
    import mitsuba
    mitsuba.set_variant('cuda_ad_rgb')
    # mitsuba.set_variant('llvm_ad_rgb')

    from mitsuba.core import xml, Float, TensorXf
    from mitsuba.python.util import write_bitmap

    config = config()
    config.initialize()

    integrator = xml.load_dict({
        'type': integrator_name,
        'max_depth': config.max_depth
    })

    filename = join(output_dir, f"test_{config.name}_image_fwd_ref.exr")
    image_fwd_ref = load_image(filename)

    image_adj = TensorXf(1.0, image_fwd_ref.shape)

    theta = Float(0.0)
    ek.enable_grad(theta)
    ek.set_label(theta, 'theta')
    config.update(theta)
    ek.forward(theta, retain_graph=False, retain_grad=True)

    # ek.set_log_level(3)

    ek.set_label(config.params, 'params')
    image_fwd = integrator.render_forward(
        config.scene, config.params, image_adj, seed=0, spp=config.spp)
    image_fwd = ek.detach(image_fwd)

    error = ek.abs(image_fwd - image_fwd_ref) / ek.max(ek.abs(image_fwd_ref), 5e-1)
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
def test03_rendering_backward(integrator_name, config):
    import mitsuba
    mitsuba.set_variant('cuda_ad_rgb')

    from mitsuba.core import xml, Float, TensorXf

    # ek.set_flag(ek.JitFlag.LoopRecord, False) # TODO should this work?

    config = config()
    config.initialize()

    integrator = xml.load_dict({
        'type': integrator_name,
        'max_depth': config.max_depth
    })

    filename = join(output_dir, f"test_{config.name}_image_fwd_ref.exr")
    image_fwd_ref = load_image(filename)

    image_adj = TensorXf(1.0, image_fwd_ref.shape)

    theta = Float(0.0)
    ek.enable_grad(theta)
    config.update(theta)

    # ek.set_log_level(3)

    integrator.render_backward(
        config.scene, config.params, image_adj, seed=0, spp=config.spp)

    grad = ek.grad(theta)[0] / ek.width(image_fwd_ref)
    grad_ref = ek.hmean(image_fwd_ref)

    error = ek.abs(grad - grad_ref) / ek.max(grad_ref, 1e-3)
    if error > config.error_mean_threshold:
        print(f"Failure in config: {config.name}, {integrator_name}")
        print(f"-> grad:     {grad}")
        print(f"-> grad_ref: {grad_ref}")
        print(f"-> error: {error} (threshold={config.error_mean_threshold})")
        assert False

# -------------------------------------------------------------------
#                      Generate reference images
# -------------------------------------------------------------------

if __name__ == "__main__":
    """
    Generate reference primal/forward images for all configs.
    """
    parser = argparse.ArgumentParser(prog='GenerateConfigReferenceImages')
    parser.add_argument('--spp', default=32, type=int,
                        help='Samples per pixel. Default value: 32')
    args = parser.parse_args()

    mitsuba.set_variant('cuda_ad_rgb')

    if not exists(output_dir):
        os.makedirs(output_dir)

    from mitsuba.core import xml, Float
    from mitsuba.python.util import write_bitmap

    fd_epsilon = 1e-2

    for config in BASIC_CONFIGS + REPARAM_CONFIGS:
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

        theta = Float(fd_epsilon)
        config.update(theta)

        image_2 = integrator.render(config.scene, seed=0, spp=args.spp)
        image_fd = (image_2 - image_ref) / fd_epsilon

        filename = join(output_dir, f"test_{config.name}_image_fwd_ref.exr")
        write_bitmap(filename, image_fd)
