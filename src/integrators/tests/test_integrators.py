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
        'near_clip': 0.1,
        'far_clip': 1000.0,
        'to_world': T.look_at(origin=[0, 0, 4], target=[0, 0, 0], up=[0, 1, 0]),
        'film': {
            'type': 'hdrfilm',
            'rfilter': { 'type': 'gaussian' },
            'width': res,
            'height': res,
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

# Translate gray rectangle on black background
class TranslateRectangleOnBlack(Config):
    def __init__(self) -> None:
        super().__init__()
        self.name = 'translate_rect_on_black'
        self.key = None
        self.scene_dict = {
            'type': 'scene',
            'rect': {
                'type': 'obj',
                'filename': 'resources/data/common/meshes/rectangle.obj',
                'face_normals': True
            },
            'light': {
                'type': 'point',
                'position': [0.0, 0.0, 1.0],
                'intensity': {'type': 'rgb', 'value': [10.0, 10.0, 10.0]}
            },
        }
        self.initial_v_pos = None

    def update(self, params, theta):
        from mitsuba.core import Vector3f

        key = 'rect.vertex_positions'
        if not self.initial_v_pos:
            params.keep([key])
            self.initial_v_pos = ek.unravel(Vector3f, params[key])

        params[key] = ek.ravel(self.initial_v_pos + Vector3f(theta, 0.0, 0.0))
        params.update()

# Translate gray rectangle in front of constant emitter
class TranslateRectangleOnEmitterConfig(Config):
    def __init__(self) -> None:
        super().__init__()
        self.name = 'translate_rect_on_emitter'
        self.key = None
        self.scene_dict = {
            'type': 'scene',
            'rect': {
                'type': 'obj',
                'filename': 'resources/data/common/meshes/rectangle.obj',
                'face_normals': True
            },
            'light': { 'type': 'constant' }
        }
        self.initial_v_pos = None

    def update(self, params, theta):
        from mitsuba.core import Vector3f

        key = 'rect.vertex_positions'
        if not self.initial_v_pos:
            params.keep([key])
            self.initial_v_pos = ek.unravel(Vector3f, params[key])

        params[key] = ek.ravel(self.initial_v_pos + Vector3f(theta, 0.0, 0.0))
        params.update()

# -------------------------------------------------------------------
#                           Unit tests
# -------------------------------------------------------------------

BASIC_CONFIGS = [
    DiffuseAlbedoConfig,
    DiffuseAlbedoGIConfig,
    AreaLightRadianceConfig,
    DirectlyVisibleAreaLightRadianceConfig,
    PointLightIntensityConfig,
    ConstantEmitterRadianceConfig,
]

REPARAM_CONFIGS = [
    # TranslateRectangleOnBlack,
    # TranslateRectangleOnEmitterConfig,
]

# List of integrators to test (also indicates whether it handles discontinuities)
INTEGRATORS = [
    ('path', False),
    ('rb', False),
    ('prb', False),
    ('rbreparam', False),
]

CONFIGS = []
for integrator_name, reparam in INTEGRATORS:
    todos = BASIC_CONFIGS + (REPARAM_CONFIGS if reparam else [])
    for config in todos:
        CONFIGS.append((integrator_name, config))

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
        'max_depth': 3
    })

    filename = join(output_dir, f"test_{config.name}_image_primal_ref.exr")
    image_primal_ref = load_image(filename)

    image = integrator.render(config.scene, seed=0, spp=config.spp)

    error = ek.abs(image - image_primal_ref) / ek.max(image_primal_ref, 1e-1)
    error_mean = ek.hmean(error)
    error_max = ek.hmax(error)

    if error_mean > 0.05 or error_max > 0.1:
        print(f"Failure in config: {config.name}, {integrator_name}")
        print(f"-> error mean: {error_mean}")
        print(f"-> error max: {error_max}")
        filename = f"test_{integrator_name}_{config.name}_image_primal.exr"
        write_bitmap(filename, image)
        assert False


@pytest.mark.parametrize('integrator_name, config', CONFIGS)
def test02_rendering_forward(integrator_name, config):
    import mitsuba
    mitsuba.set_variant('cuda_ad_rgb')

    from mitsuba.core import xml, Float, TensorXf
    from mitsuba.python.util import write_bitmap

    # print(ek.graphviz_str(Float(1)))
    # ek.ad_whos()
    # ek.set_flag(ek.JitFlag.LoopRecord, False)
    # ek.set_flag(ek.JitFlag.VCallRecord, True)

    config = config()
    config.initialize()

    integrator = xml.load_dict({
        'type': integrator_name,
        'max_depth': 3
    })

    filename = join(output_dir, f"test_{config.name}_image_fwd_ref.exr")
    image_fwd_ref = load_image(filename)

    image_adj = TensorXf(
        ek.full(Float, 1.0, ek.width(image_fwd_ref)), image_fwd_ref.shape)

    theta = Float(0.0)
    ek.enable_grad(theta)
    ek.set_label(theta, 'theta')
    config.update(theta)
    ek.forward(theta, retain_graph=False, retain_grad=True)

    ek.set_label(config.params, 'params')
    image_fwd = integrator.render_forward(
        config.scene, config.params, image_adj, seed=0, spp=config.spp)

    filename = f"test_{config.name}_image_fwd.exr"
    write_bitmap(filename, image_fwd)

    error = ek.abs(image_fwd - image_fwd_ref) / ek.max(image_fwd_ref, 1e-1)
    error_mean = ek.hmean(error)
    error_max = ek.hmax(error)

    if error_mean > 0.05 or error_max > 0.2:
        print(f"Failure in config: {config.name}, {integrator_name}")
        print(f"-> error mean: {error_mean}")
        print(f"-> error max: {error_max}")
        filename = f"test_{integrator_name}_{config.name}_image_fwd.exr"
        write_bitmap(filename, image_fwd)
        assert False


@pytest.mark.parametrize('integrator_name, config', CONFIGS)
def test03_rendering_backward(integrator_name, config):
    import mitsuba
    mitsuba.set_variant('cuda_ad_rgb')

    from mitsuba.core import xml, Float, TensorXf
    from mitsuba.python.util import write_bitmap

    # ek.set_flag(ek.JitFlag.LoopRecord, False) # TODO should this work?

    config = config()
    config.initialize()

    integrator = xml.load_dict({
        'type': integrator_name,
        'max_depth': 3
    })

    filename = join(output_dir, f"test_{config.name}_image_fwd_ref.exr")
    image_fwd_ref = load_image(filename)

    image_adj = TensorXf(
        ek.full(Float, 1.0, ek.width(image_fwd_ref)), image_fwd_ref.shape)

    theta = Float(0.0)
    ek.enable_grad(theta)
    config.update(theta)

    integrator.render_backward(
        config.scene, config.params, image_adj, seed=0, spp=config.spp)

    grad = ek.grad(theta)[0] / ek.width(image_fwd_ref)
    grad_ref = ek.hsum_nested(image_fwd_ref) / ek.width(image_fwd_ref)

    error = ek.abs(grad - grad_ref) / ek.max(grad_ref, 1e-3)
    if error > 0.01:
        print(f"Failure in config: {config.name}, {integrator_name}")
        print(f"-> grad:     {grad}")
        print(f"-> grad_ref: {grad_ref}")
        print(f"-> error: {error}")
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
            'max_depth': 3
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
