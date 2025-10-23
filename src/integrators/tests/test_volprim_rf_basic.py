import pytest, os

import drjit as dr
import mitsuba as mi

from mitsuba.scalar_rgb import Transform4f as T
from mitsuba.testing import RenderingRegressionTest

class EllipsoidsFactory:
    '''
    Helper class to build Ellipsoids datasets for testing purposes
    '''
    def __init__(self):
        self.centers = []
        self.scales = []
        self.quaternions = []
        self.sigmats = []
        self.albedos = []

    def add(self, mean, scale, sigmat=1.0, albedo=1.0, euler=[0.0, 0.0, 0.0]):
        self.centers.append(mi.ScalarPoint3f(mean))
        self.scales.append(mi.ScalarVector3f(scale))
        quaternion = dr.slice(dr.euler_to_quat(dr.deg2rad(mi.ScalarPoint3f(euler))))
        self.quaternions.append(mi.ScalarQuaternion4f(quaternion))
        self.sigmats.append(sigmat)
        if isinstance(albedo, float):
            albedo = mi.ScalarColor3f(albedo)
        self.albedos.append(albedo)

    def build(self):
        import numpy as np
        num_gaussians = len(self.centers)
        centers = mi.TensorXf(np.ravel(np.array(self.centers)), shape=(num_gaussians, 3))
        scales  = mi.TensorXf(np.ravel(np.array(self.scales)), shape=(num_gaussians, 3))
        quats   = mi.TensorXf(np.ravel(np.array(self.quaternions)), shape=(num_gaussians, 4))
        sigmats = mi.TensorXf(self.sigmats, shape=(num_gaussians, 1))
        self.albedos = np.array(self.albedos).reshape((num_gaussians, -1))
        albedos = mi.TensorXf(np.array(self.albedos))
        return centers, scales, quats, sigmats, albedos


@pytest.mark.parametrize("shape_type", ['ellipsoids', 'ellipsoidsmesh'])
def test01_render_span(variants_all_ad_rgb, regression_test_options, shape_type):
    if 'double' in mi.variant():
        pytest.skip("Test needs to be adapted for double precision variants")

    factory = EllipsoidsFactory()
    N = 12
    for i in range(N):
        r = 2.5
        t = i / float(N)
        s = 0.05 + t * 0.2
        factory.add(
            mean=[r * dr.sin(t * dr.two_pi), r * dr.cos(t * dr.two_pi), 0.01 * t],
            scale=[s, 3*s, s],
            euler=[0.0, 0.0, -360 * t],
            albedo=[dr.abs(dr.sin(t * dr.pi)), 1.0 - t, t]
        )
    centers, scales, quaternions, sigmats, albedos = factory.build()

    test = RenderingRegressionTest(
        name=f'volprim_rf_basic_span[{shape_type}]',
        scene=mi.load_dict({
            'type': 'scene',
            'integrator': {
                'type': 'moment',
                'nested': {
                    'type': 'volprim_rf_basic',
                    'max_depth': 8,
                }
            },
            'sensor': {
                'type': 'perspective',
                'to_world': mi.ScalarTransform4f.look_at(
                    origin=[0, 0, -5],
                    target=[0, 0, 0],
                    up=[0, 1, 0]),
                'fov': 61.9275,
                'myfilm': {
                    'type': 'hdrfilm',
                    'rfilter': { 'type': 'tent' },
                    'width': 128, 'height': 128,
                    'pixel_format': 'rgb',
                },
            },
            'shape': {
                'type': shape_type,
                'centers': centers,
                'scales': scales,
                'quaternions': quaternions,
                'opacities': sigmats,
                'sh_coeffs': albedos,
            }
        }),
        test_spp=2048,
        pixel_success_rate=0.99,
        references_folder=os.path.join(os.path.dirname(__file__), '../../../resources/data/tests/scenes/volprim_rf_basic/refs'),
        **regression_test_options
    )

    assert test.run()


@pytest.mark.parametrize("srgb_primitives", [True, False])
def test02_render_stack(variants_all_ad_rgb, regression_test_options, srgb_primitives):
    if 'double' in mi.variant():
        pytest.skip("Test needs to be adapted for double precision variants")

    factory = EllipsoidsFactory()
    N = 12
    for i in range(N):
        r = 1.0
        t = i / float(N)
        s = 0.15 + t * 0.15
        factory.add(
            mean=[r * dr.sin(t * dr.two_pi), r * dr.cos(t * dr.two_pi), 0.2 * t],
            scale=[s, 8*s, s],
            sigmat=0.5,
            euler=[0.0, 0.0, -360 * t + 45],
            albedo=[dr.abs(dr.sin(t * dr.pi)), 1.0 - t, t]
        )
    centers, scales, quaternions, sigmats, albedos = factory.build()

    test = RenderingRegressionTest(
        name=f'volprim_rf_basic_stack[srgb_primitives={srgb_primitives}]',
        scene=mi.load_dict({
            'type': 'scene',
            'integrator': {
                'type': 'moment',
                'nested': {
                    'type': 'volprim_rf_basic',
                    'max_depth': 32,
                    'srgb_primitives': srgb_primitives
                }
            },
            'sensor': {
                'type': 'perspective',
                'to_world': mi.ScalarTransform4f.look_at(
                    origin=[0, 0, -5],
                    target=[0, 0, 0],
                    up=[0, 1, 0]),
                'fov': 61.9275,
                'myfilm': {
                    'type': 'hdrfilm',
                    'rfilter': { 'type': 'tent' },
                    'width': 128, 'height': 128,
                    'pixel_format': 'rgb',
                },
            },
            'shape': {
                'type': 'ellipsoidsmesh',
                'shell': 'uv_sphere',
                'centers': centers,
                'scales': scales,
                'quaternions': quaternions,
                'opacities': sigmats,
                'sh_coeffs': albedos,
            },
        }),
        test_spp=2048,
        pixel_success_rate=0.99,
        references_folder=os.path.join(os.path.dirname(__file__), '../../../resources/data/tests/scenes/volprim_rf_basic/refs'),
        **regression_test_options
    )

    assert test.run()


@pytest.mark.parametrize("shape_type", ['ellipsoids', 'ellipsoidsmesh'])
def test03_render_depth(variants_all_rgb, regression_test_options, shape_type):
    if 'double' in mi.variant():
        pytest.skip("Test needs to be adapted for double precision variants")

    factory = EllipsoidsFactory()
    N = 12
    for i in range(N):
        r = 2.5
        t = i / float(N)
        s = 0.05 + t * 0.2
        factory.add(
            mean=[r * dr.sin(t * dr.two_pi), r * dr.cos(t * dr.two_pi), 0.01 * t],
            scale=[s, 3*s, s],
            euler=[0.0, 0.0, -360 * t],
            albedo=[dr.abs(dr.sin(t * dr.pi)), 1.0 - t, t]
        )
    centers, scales, quaternions, sigmats, albedos = factory.build()

    test = RenderingRegressionTest(
        name=f'volprim_rf_basic_depth[{shape_type}]',
        scene=mi.load_dict({
            'type': 'scene',
            'integrator': {
                'type': 'moment',
                'nested': {
                    'type': 'depth',
                }
            },
            'sensor': {
                'type': 'perspective',
                'to_world': mi.ScalarTransform4f.look_at(
                    origin=[0, 0, -5],
                    target=[0, 0, 0],
                    up=[0, 1, 0]),
                'fov': 61.9275,
                'sampler': {
                    'type': 'stratified'
                },
                'myfilm': {
                    'type': 'hdrfilm',
                    'rfilter': { 'type': 'box' },
                    'width': 128, 'height': 128,
                    'pixel_format': 'rgb',
                },
            },
            'shape': {
                'type': shape_type,
                'centers': centers,
                'scales': scales,
                'quaternions': quaternions,
                'opacities': sigmats,
                'sh_coeffs': albedos,
            }
        }),
        test_spp=128,
        ref_spp=128,
        pixel_success_rate=0.94,
        references_folder=os.path.join(os.path.dirname(__file__), '../../../resources/data/tests/scenes/volprim_rf_basic/refs'),
        **regression_test_options
    )

    assert test.run()
