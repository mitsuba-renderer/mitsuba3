import time

import pytest
import drjit as dr
import mitsuba as mi
import numpy as np

from mitsuba.scalar_rgb.test.util import fresolver_append_path
from .test_renders import z_test, bitmap_extract, xyz_to_rgb_bmp


def make_test_scene(resx, resy, simple=False, integrator=None, **kwargs):
    def color_to_dict(color):
        if isinstance(color, (float, int)):
            return color
        return {'type': 'rgb', 'value': color}
    def checkerboard(color0, color1=None):
        d = {
            'type': 'checkerboard',
            'to_uv': mi.ScalarTransform4f.scale((4, 4, 4)),
        }
        if color0 is not None:
            d['color0'] = color_to_dict(color0)
        if color1 is not None:
            d['color1'] = color_to_dict(color1)
        return d

    scene = dict({
        'type': 'scene',
        'shape1': {
            'type': 'sphere',
            'to_world': mi.ScalarTransform4f.translate([1.0, 0, 0]),
            'bsdf': {
                'type': 'dielectric',
            }
        },
        'shape2': {
            'type': 'cylinder',
            'to_world': (
                mi.ScalarTransform4f.translate([-0.85, 1.5, 0])
                @ mi.ScalarTransform4f.scale((1, 3, 1))
                @ mi.ScalarTransform4f.rotate(axis=(1, 0, 0), angle=90)
            ),
            'bsdf': {
                'type': 'roughconductor',
                'alpha': checkerboard(0.1, 0.01),
            }
        },
        'shape3': {
            'type': 'disk',
            'to_world': (
                mi.ScalarTransform4f.translate([-2.5, 0, 1])
                @ mi.ScalarTransform4f.rotate(axis=(1, 0, 0), angle=180)
            ),
            'bsdf': {
                'type': 'diffuse',
                'reflectance': checkerboard([0, 0, 1]),
            }
        },
        'shape4': {
            'type': 'cube',
            'to_world': (
                mi.ScalarTransform4f.translate([2.5, 0, 1])
                @ mi.ScalarTransform4f.rotate(axis=(1, 0, 0), angle=180)
            ),
            'bsdf': {
                'type': 'diffuse',
                'reflectance': checkerboard([0, 1, 1]),
            }
            # 'bsdf': {
            #     'type': 'dielectric',
            # },
        },
        'shape5': {
            'type': 'rectangle',
            'to_world': (
                mi.ScalarTransform4f.translate([-1.5, 1.0, -0.5])
                @ mi.ScalarTransform4f.rotate(axis=(1, 0, 0), angle=205)
            ),
            'bsdf': {
                'type': 'diffuse',
                'reflectance': checkerboard([1, 1, 0]),
            }
        },
        'shape6': {
            'type': 'obj',
            'filename': 'resources/data/scenes/cbox/meshes/cbox_smallbox.obj',
            'to_world': (
                mi.ScalarTransform4f.translate([0, -2.5, -0.5])
                @ mi.ScalarTransform4f.scale((0.01, 0.01, 0.01))
                @ mi.ScalarTransform4f.translate([-150, 0, 0])
            ),
            'bsdf': {
                'type': 'diffuse',
                'reflectance': checkerboard([1, 0, 1]),
            }
        },
        'sensor': {
            'type': 'perspective',
            'fov': 60,
            'to_world': mi.ScalarTransform4f.look_at(
                origin=[0, 0, -10],
                target=[0, 0, 0],
                up=[0, 1, 0],
            ),
            'film': {
                'type': 'hdrfilm',
                'width': resx,
                'height': resy,
            }
        },
        'integrator': {
            # Moment integrator to estimate variance for the z-test
            'type': 'moment',
            'sub_integrator': {
                'type': 'path',
                'max_depth': 8,
            },
        },
        'emitter': {
            'type': 'envmap',
            'filename': 'resources/data/common/textures/museum.exr',
        }
    }, **kwargs)

    if simple:
        del scene['shape2'], scene['shape3'], scene['shape4'], scene['shape5'], scene['shape6']
    if integrator is not None:
        scene['integrator'] = integrator

    return mi.load_dict(scene)


@fresolver_append_path
# def test01_bypass_correctness(variant_scalar_rgb):
def test01_bypass_correctness(variants_all_backends_once):
    """Rendering with and without acceleration data structures should result in the same images."""
    # Adapted from test_renders.test_render()
    significance_level = 0.01
    resx, resy = (207, 101)
    # resx, resy = (103, 51)

    # Compute spp budget
    sample_budget = int(1e6)
    pixel_count = resx * resy
    spp = sample_budget // pixel_count

    results = {}
    for bypass in (True, False):
        scene = make_test_scene(resx, resy, use_naive_intersection=bypass)
        # Render the scene, including a variance estimate
        scene.integrator().render(scene, seed=0, spp=spp)

        bmp = scene.sensors()[0].film().bitmap(raw=False)
        img, var_img = bitmap_extract(bmp, require_variance=True)
        results[bypass] = (img, var_img)

        # TODO: remove this
        # b = mi.Bitmap(img, mi.Bitmap.PixelFormat.XYZ)
        # breakpoint()
        # xyz_to_rgb_bmp(img).write(
        bmp.split()[0][1].write(
            f'test_{mi.variant()}_{"naive" if bypass else "accel"}.exr')

    # Compute Z-test p-value
    p_value = z_test(results[0][0], spp, results[1][0], results[1][1])

    # Apply the Sidak correction term, since we'll be conducting multiple independent
    # hypothesis tests. This accounts for the fact that the probability of a failure
    # increases quickly when several hypothesis tests are run in sequence.
    alpha = 1.0 - (1.0 - significance_level) ** (1.0 / pixel_count)

    success = (p_value > alpha)
    if (np.count_nonzero(success) / 3) >= (0.9975 * pixel_count):
        print(f'Accepted the null hypothesis (min(p-value) = {np.min(p_value)}, '
              f'significance level = {alpha})')
    else:
        print(f'Rejected the null hypothesis (min(p-value) = {np.min(p_value)}, '
              f'significance level = {alpha})')

        # Note: images are in the XYZ color space by default
        for bypass in results:
            xyz_to_rgb_bmp(results[bypass][0]).write(
                f'test_{mi.variant()}_{"naive" if bypass else "accel"}.exr')

        assert False, 'Z-test failed'


# Useful to investigate performance, but probably not reliable enough
# to run on the CI.
# @fresolver_append_path
# def test02_speed(variant_llvm_ad_rgb):
@pytest.mark.skip
@fresolver_append_path
def test02_speed(variants_all_backends_once):
    log_level = mi.Thread.thread().logger().log_level()
    mi.set_log_level(mi.LogLevel.Warn)

    results = {}
    for bypass in (True, False):
        results[bypass] = []
        scene = make_test_scene(
            512, 256, simple=True,
            integrator={'type': 'direct'},
            use_naive_intersection=bypass)

        for i in range(12):
            dr.eval()
            dr.sync_thread()
            t0 = time.time()
            img = mi.render(scene, spp=32)
            dr.eval(img)
            dr.sync_thread()

            if i >= 2:
                elapsed = time.time() - t0
                results[bypass].append(elapsed)

            if i == 0:
                mi.Bitmap(img).write(f'test_speed_{mi.variant()}_{bypass}.exr')

    print(f'\n--- {mi.variant()} ---')
    for bypass, values in results.items():
        print(f'{"naive" if bypass else "accel"}: {1000 * np.mean(values):.6f} ms')
        print(values)
    print(f'------------------\n\n')

    mi.set_log_level(log_level)
