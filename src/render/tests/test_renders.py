import pytest
import drjit as dr
import mitsuba as mi
import os
import argparse
import glob
import numpy as np

from os.path import join, dirname, basename, splitext, exists
from drjit.scalar import ArrayXf as Float
from mitsuba.scalar_rgb.test.util import find_resource

color_modes = ['mono', 'rgb', 'spectral_polarized', 'spectral']

# List all the XML scenes in the tests/scenes folder
TEST_SCENE_DIR = find_resource('resources/data/tests/scenes')
SCENES = glob.glob(join(TEST_SCENE_DIR, '*', '*.xml'))

# List of test scene folders to exclude
EXCLUDE_FOLDERS = [
]

# List of test scene folders to exclude for JIT modes
JIT_EXCLUDE_FOLDERS = [
]

# List of test scene folders that are only enabled with virtual call recordings
# and loop recordings in JIT variants
JIT_FORCE_RECORD_FOLDERS = [
    'participating_media',
]

POLARIZED_EXCLUDE_FOLDERS = {
    'participating_media',
}

POLARIZED_EXCLUDE_INTEGRATORS = {
    'prb','prbvolpath', 'direct_projective', 'prb_projective'
}

# Every scene that has an integrator which is in this mapping's keys will also
# be rendered with the integrator(s) listed in the corresponding mapping's
# values. The new integrators are only executed in JIT modes with the default
# set flags.
INTEGRATOR_MAPPING = {
    'direct' : ['direct_projective'],
    'path' : ['prb','prb_projective'],
    'volpath' : ['volpathmis', 'prbvolpath'],
}

if hasattr(dr, 'JitFlag'):
    JIT_FLAG_OPTIONS = {
        'scalar': {},
        'jit_flag_option_0' : {dr.JitFlag.SymbolicCalls : 0, dr.JitFlag.OptimizeCalls : 0, dr.JitFlag.SymbolicLoops : 0},
        'jit_flag_option_1' : {dr.JitFlag.SymbolicCalls : 1, dr.JitFlag.OptimizeCalls : 0, dr.JitFlag.SymbolicLoops : 0},
        'jit_flag_option_2' : {dr.JitFlag.SymbolicCalls : 1, dr.JitFlag.OptimizeCalls : 1, dr.JitFlag.SymbolicLoops : 0},
        'jit_flag_option_3' : {dr.JitFlag.SymbolicCalls : 1, dr.JitFlag.OptimizeCalls : 1, dr.JitFlag.SymbolicLoops : 1},
    }

def list_all_render_test_configs():
    """
    Return a list of (variant, scene_fname, integrator_type, jit_flags) for all
    the valid render tests given the list of exclusion above.

    Scene file names need to be suffixed with the integrator type (e.g.,
    "scene_path.xml", "scene_ptracer.xml") for this method to generate valid
    test configurations.
    """
    configs = []
    for variant in mi.variants():
        is_jit = "cuda" in variant or "llvm" in variant
        is_polarized = "polarized" in variant

        for scene_fname in SCENES:
            if any(ex in scene_fname for ex in EXCLUDE_FOLDERS):
                continue
            if is_jit and any(ex in scene_fname for ex in JIT_EXCLUDE_FOLDERS):
                continue
            if is_polarized and any(ex in scene_fname for ex in POLARIZED_EXCLUDE_FOLDERS):
                continue
            scene_integrator_type = os.path.splitext(scene_fname)[0].rsplit('_', 1)[-1]

            if not is_jit:
                configs.append((variant, scene_fname, scene_integrator_type, 'scalar'))
            else:
                for k, v in JIT_FLAG_OPTIONS.items():
                    if k == 'scalar':
                        continue

                    has_vcall_recording = v.get(dr.JitFlag.SymbolicCalls, 0)
                    has_loop_recording = v.get(dr.JitFlag.SymbolicLoops, 0)
                    if any(
                        folder in scene_fname for folder in JIT_FORCE_RECORD_FOLDERS
                    ) and (not has_loop_recording or not has_vcall_recording):
                        continue

                    configs.append((variant, scene_fname, scene_integrator_type, k))

                for integrator_type in INTEGRATOR_MAPPING.get(scene_integrator_type, []):
                    if is_polarized and integrator_type in POLARIZED_EXCLUDE_INTEGRATORS:
                        continue
                    configs.append((variant, scene_fname, integrator_type, 'jit_flag_option_3'))

    return configs


def get_ref_fname(scene_fname):
    """Give a scene filename, return the path to the reference and variance images"""
    for color_mode in color_modes:
        if color_mode in mi.variant():
            ref_fname = join(dirname(scene_fname), 'refs', splitext(
                basename(scene_fname))[0].rsplit('_', 1)[0] + '_ref_' + color_mode + '.exr')
            var_fname = ref_fname.replace('.exr', '_var.exr')
            return ref_fname, var_fname
    pytest.fail("Could not find reference images for the given scene!")


def xyz_to_rgb_bmp(arr):
    """Convert an XYZ image to RGB"""
    xyz_bmp = mi.Bitmap(arr, mi.Bitmap.PixelFormat.XYZ)
    return xyz_bmp.convert(mi.Bitmap.PixelFormat.RGB, mi.Struct.Type.Float32, False)


def read_rgb_bmp_to_xyz(fname):
    """Load and convert RGB image to XYZ bitmap"""
    return mi.Bitmap(fname).convert(mi.Bitmap.PixelFormat.XYZ, mi.Struct.Type.Float32, False)


def bitmap_extract(bmp, require_variance=True):
    """Extract different channels from moment integrator AOVs"""
    # AVOs from the moment integrator are in XYZ (float32)
    split = bmp.split()
    if len(split) == 1:
        if require_variance:
            raise RuntimeError(
                'Could not extract variance image from bitmap. '
                'Did you wrap the integrator into a `moment` integrator?\n{}'.format(bmp))
        b_root = split[0][1]
        if b_root.channel_count() >= 3 and b_root.pixel_format() != mi.Bitmap.PixelFormat.XYZ:
            b_root = b_root.convert(mi.Bitmap.PixelFormat.XYZ, mi.Struct.Type.Float32, False)
        img = np.array(b_root, copy=True)

        if len(img.shape) == 2:
            img = img[..., np.newaxis]

        return img, None
    else:
        img    = np.array(split[1][1], copy=False)
        img_m2 = np.array(split[2][1], copy=False)

        if len(img.shape) == 2:
            img = img[..., np.newaxis]
        if len(img_m2.shape) == 2:
            img_m2 = img_m2[..., np.newaxis]

        return img, img_m2 - img * img


def z_test(mean, sample_count, reference, reference_var):
    """Implementation of the Z-test statistical test"""
    # Sanitize the variance images
    reference_var = np.maximum(reference_var, 1e-4)

    # Compute Z statistic
    z_stat = np.abs(mean - reference) * np.sqrt(sample_count / reference_var)

    # Cumulative distribution function of the standard normal distribution
    def stdnormal_cdf(x):
        shape = x.shape
        cdf = (1.0 - dr.erf(-Float(x.flatten()) / dr.sqrt(2.0))) * 0.5
        return np.array(cdf).reshape(shape)

    # Compute p-value
    p_value = 2.0 * (1.0 - stdnormal_cdf(z_stat))

    return p_value


@pytest.mark.slow
@pytest.mark.parametrize("variant, scene_fname, integrator_type, jit_flags_key", list_all_render_test_configs())
def test_render(variant, scene_fname, integrator_type, jit_flags_key):
    mi.set_variant(variant)

    if 'cuda' in variant or 'llvm' in variant:
        dr.flush_malloc_cache()
        for k, v in JIT_FLAG_OPTIONS[jit_flags_key].items():
            dr.set_flag(k, bool(v))

    ref_fname, ref_var_fname = get_ref_fname(scene_fname)
    if not (exists(ref_fname) and exists(ref_var_fname)):
        pytest.skip("Missing reference data:\n- Reference image: {}\n- Variance image: {}".format(
            ref_fname, ref_var_fname))

    if os.name == 'nt' and 'test_various_emitters' in ref_fname and 'cuda' in variant:
        pytest.skip('Skipping flaky test (likely an OptiX miscompilation) on Windows')

    ref_bmp = read_rgb_bmp_to_xyz(ref_fname)
    ref_img = np.array(ref_bmp, copy=False)

    ref_var_bmp = read_rgb_bmp_to_xyz(ref_var_fname)
    ref_var_img = np.array(ref_var_bmp, copy=False)

    significance_level = 0.01

    # Compute spp budget
    sample_budget = int(2e6)
    pixel_count = dr.prod(ref_bmp.size())
    spp = sample_budget // pixel_count

    # Load and render
    scene = mi.load_file(scene_fname, spp=spp, integrator=integrator_type)
    scene.integrator().render(scene, seed=0, spp=spp, develop=True)

    # Compute variance image
    bmp = scene.sensors()[0].film().bitmap(raw=False)
    img, var_img = bitmap_extract(bmp, require_variance=False)

    # Compute Z-test p-value
    p_value = z_test(img, spp, ref_img, ref_var_img)

    # Apply the Sidak correction term, since we'll be conducting multiple independent
    # hypothesis tests. This accounts for the fact that the probability of a failure
    # increases quickly when several hypothesis tests are run in sequence.
    alpha = 1.0 - (1.0 - significance_level) ** (1.0 / pixel_count)

    success = (p_value > alpha)

    if (np.count_nonzero(success) / 3) >= (0.9975 * pixel_count):
        print('Accepted the null hypothesis (min(p-value) = %f, significance level = %f)' %
              (np.min(p_value), alpha))
    else:
        print('Reject the null hypothesis (min(p-value) = %f, significance level = %f)' %
              (np.min(p_value), alpha))

        output_dir = join(dirname(scene_fname), 'error_output')

        if not exists(output_dir):
            os.makedirs(output_dir)

        output_prefix = join(output_dir, splitext(
            basename(scene_fname))[0] + '_' + mi.variant())

        img_rgb_bmp = xyz_to_rgb_bmp(img)
        ref_img_rgb_bmp = xyz_to_rgb_bmp(ref_img)

        fname = output_prefix + '_img.exr'
        img_rgb_bmp.write(fname)
        print('Saved rendered image to: ' + fname)

        fname = output_prefix + '_ref.exr'
        ref_img_rgb_bmp.write(fname)
        print('Saved reference image to: ' + fname)

        if var_img is not None:
            var_fname = output_prefix + '_var.exr'
            xyz_to_rgb_bmp(var_img).write(var_fname)
            print('Saved variance image to: ' + var_fname)

        err_fname = output_prefix + '_error.exr'
        err_img = 0.02 * np.array(img_rgb_bmp)
        err_img[~success] = 1.0
        mi.Bitmap(err_img).write(err_fname)
        print('Saved error image to: ' + err_fname)

        pvalue_fname = output_prefix + '_pvalue.exr'
        xyz_to_rgb_bmp(p_value).write(pvalue_fname)
        print('Saved error image to: ' + pvalue_fname)

        pytest.fail("Radiance values exceeded scene's tolerances!")


def render_ref_images(scenes, spp, overwrite, scene=None, variant=None):
    if scene is not None:
        if not scene.endswith('.xml'):
            scene = scene + '.xml'
        for s in scenes:
            if os.path.basename(s) == scene:
                scenes = [s]
                break
        else:
            raise ValueError('Scene "{}" not found in available scenes: {}'.format(
                scene, list(map(os.path.basename, scenes))))

    for scene_fname in scenes:
        scene_dir = dirname(scene_fname)

        if os.path.split(scene_dir)[1] in EXCLUDE_FOLDERS:
            continue

        for variant_ in mi.variants():
            if variant is not None:
                if not variant.split('_')[0] == 'scalar' or variant_.endswith('double'):
                    continue

                if variant != variant_:
                    continue

            if 'polarized' in variant_ and os.path.split(scene_dir)[1] in POLARIZED_EXCLUDE_FOLDERS:
                continue

            mi.set_variant(variant_)

            ref_fname, var_fname = get_ref_fname(scene_fname)
            if exists(ref_fname) and exists(var_fname) and not overwrite:
                continue

            print(f'Rendering: {scene_fname} - {variant_}')
            scene = mi.load_file(scene_fname, spp=spp)
            scene.integrator().render(scene, seed=0, develop=False)

            bmp = scene.sensors()[0].film().bitmap(raw=False)
            img, var_img = bitmap_extract(bmp)

            # Write rendered image to a file
            os.makedirs(dirname(ref_fname), exist_ok=True)
            xyz_to_rgb_bmp(img).write(ref_fname)
            print(f'Saved rendered image to: {ref_fname}')

            # Write variance image to a file
            xyz_to_rgb_bmp(var_img).write(var_fname)
            print(f'Saved variance image to: {var_fname}')


if __name__ == '__main__':
    """
    Generate reference images for all the scenes contained within the TEST_SCENE_DIR directory,
    and for all the color mode having their `scalar_*` mode enabled.
    """
    parser = argparse.ArgumentParser(prog='RenderReferenceImages')
    parser.add_argument('--overwrite', action='store_true',
                        help='Force rerendering of all reference images. Otherwise, only missing references will be rendered.')
    parser.add_argument('--spp', default=32000, type=int,
                        help='Samples per pixel. Default value: 32000')
    parser.add_argument('--scene', default=None, type=str,
                        help='Name of a specific scene to render. Otherwise, try to render reference images for all scenes.')
    parser.add_argument('--variant', default=None, type=str,
                        help='Name of a specific variant to render. Otherwise, try to render reference images for all variants.')
    args = parser.parse_args()
    render_ref_images(SCENES, **vars(args))
