import os
from os.path import join, realpath, dirname, basename, splitext, exists
import argparse
import glob
import mitsuba
import pytest
import enoki as ek
import numpy as np
from enoki.scalar import ArrayXf as Float


color_modes = ['mono', 'rgb', 'spectral_polarized', 'spectral']

# List all the XML scenes in the tests/scenes folder
TEST_SCENE_DIR = realpath(join(os.path.dirname(__file__), '../../../resources/data/tests/scenes'))
SCENES = glob.glob(join(TEST_SCENE_DIR, '*', '*.xml'))

# List of test scene folders to exclude
EXCLUDE_FOLDERS = [
    'orthographic_sensor', #TODO remove this after rebase onto master
]

# List of test scene folders to exclude for JIT modes
JIT_EXCLUDE_FOLDERS = [
    'participating_media',
]

# List of test scene folders to exclude for symbolic modes
SYMBOLIC_EXCLUDE_FOLDERS = [
    # all working now, yay
]

POLARIZED_EXCLUDE_FOLDERS = [
    'participating_media',
]

if hasattr(ek, 'JitFlag'):
    JIT_FLAG_OPTIONS = {
        'scalar': {},
        'jit_flag_option_0' : {ek.JitFlag.VCallRecord : 0, ek.JitFlag.VCallOptimize : 0, ek.JitFlag.LoopRecord : 0},
        'jit_flag_option_1' : {ek.JitFlag.VCallRecord : 1, ek.JitFlag.VCallOptimize : 0, ek.JitFlag.LoopRecord : 0},
        'jit_flag_option_2' : {ek.JitFlag.VCallRecord : 1, ek.JitFlag.VCallOptimize : 1, ek.JitFlag.LoopRecord : 0},
        'jit_flag_option_3' : {ek.JitFlag.VCallRecord : 1, ek.JitFlag.VCallOptimize : 1, ek.JitFlag.LoopRecord : 1},
    }

def list_all_render_test_configs():
    """
    Return a list of (variant, scene_fname, jit_flags) for all the valid render
    tests given the list of exclusion above.
    """
    configs = []
    for variant in mitsuba.variants():
        is_jit = "cuda" in variant or "llvm" in variant
        is_polarized = "polarized" in variant
        for scene in SCENES:
            if any(ex in scene for ex in EXCLUDE_FOLDERS):
                continue
            if is_jit and any(ex in scene for ex in JIT_EXCLUDE_FOLDERS):
                continue
            if is_polarized and any(ex in scene for ex in POLARIZED_EXCLUDE_FOLDERS):
                continue
            if not is_jit:
                configs.append((variant, scene, 'scalar'))
            else:
                for k, v in JIT_FLAG_OPTIONS.items():
                    if k == 'scalar':
                        continue
                    is_symbolic = v[ek.JitFlag.VCallRecord] == 1
                    if is_symbolic and any(ex in scene for ex in SYMBOLIC_EXCLUDE_FOLDERS):
                        continue
                    configs.append((variant, scene, k))
    return configs


def get_ref_fname(scene_fname):
    """Give a scene filename, return the path to the reference and variance images"""
    for color_mode in color_modes:
        if color_mode in mitsuba.variant():
            ref_fname = join(dirname(scene_fname), 'refs', splitext(
                basename(scene_fname))[0] + '_ref_' + color_mode + '.exr')
            var_fname = ref_fname.replace('.exr', '_var.exr')
            return ref_fname, var_fname
    assert False


def xyz_to_rgb_bmp(arr):
    """Convert an XYZ image to RGB"""
    from mitsuba.core import Bitmap, Struct
    xyz_bmp = Bitmap(arr, Bitmap.PixelFormat.XYZ)
    return xyz_bmp.convert(Bitmap.PixelFormat.RGB, Struct.Type.Float32, False)


def read_rgb_bmp_to_xyz(fname):
    """Load and convert RGB image to XYZ bitmap"""
    from mitsuba.core import Bitmap, Struct
    return Bitmap(fname).convert(Bitmap.PixelFormat.XYZ, Struct.Type.Float32, False)


def bitmap_extract(bmp, require_variance=True):
    """Extract different channels from moment integrator AOVs"""
    # AVOs from the moment integrator are in XYZ (float32)
    from mitsuba.core import Bitmap, Struct

    split = bmp.split()
    if len(split) == 1:
        if require_variance:
            raise RuntimeError(
                'Could not extract variance image from bitmap. '
                'Did you wrap the integrator into a `moment` integrator?\n{}'.format(bmp))
        b_root = split[0][1]
        if b_root.channel_count() >= 3 and b_root.pixel_format() != Bitmap.PixelFormat.XYZ:
            b_root = b_root.convert(Bitmap.PixelFormat.XYZ, Struct.Type.Float32, False)
        return np.array(b_root, copy=True), None
    else:
        img = np.array(split[1][1], copy=False)
        img_m2 = np.array(split[2][1], copy=False)
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
        cdf = (1.0 - ek.erf(-Float(x.flatten()) / ek.sqrt(2.0))) * 0.5
        return np.array(cdf).reshape(shape)

    # Compute p-value
    p_value = 2.0 * (1.0 - stdnormal_cdf(z_stat))

    return p_value


@pytest.mark.slow
@pytest.mark.parametrize("variant, scene_fname, jit_flags_key", list_all_render_test_configs())
def test_render(variant, scene_fname, jit_flags_key):
    mitsuba.set_variant(variant)
    from mitsuba.core import load_file, Bitmap

    if 'cuda' in variant or 'llvm' in variant:
        ek.flush_malloc_cache()
        for k, v in JIT_FLAG_OPTIONS[jit_flags_key].items():
            ek.set_flag(k, v)

    ref_fname, ref_var_fname = get_ref_fname(scene_fname)
    if not (exists(ref_fname) and exists(ref_var_fname)):
        pytest.skip("Missing reference data:\n- Reference image: {}\n- Variance image: {}".format(
            ref_fname, ref_var_fname))


    ref_bmp = read_rgb_bmp_to_xyz(ref_fname)
    ref_img = np.array(ref_bmp, copy=False)

    ref_var_bmp = read_rgb_bmp_to_xyz(ref_var_fname)
    ref_var_img = np.array(ref_var_bmp, copy=False)

    significance_level = 0.01

    # Compute spp budget
    sample_budget = int(2e6)
    pixel_count = ek.hprod(ref_bmp.size())
    spp = sample_budget // pixel_count

    # Load and render
    scene = load_file(scene_fname, spp=spp)
    scene.integrator().render(scene, seed=0, develop_film=False)

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
            basename(scene_fname))[0] + '_' + mitsuba.variant())

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
        Bitmap(err_img).write(err_fname)
        print('Saved error image to: ' + err_fname)

        pvalue_fname = output_prefix + '_pvalue.exr'
        xyz_to_rgb_bmp(p_value).write(pvalue_fname)
        print('Saved error image to: ' + pvalue_fname)

        assert False


def render_ref_images(scenes, spp, overwrite, scene=None):
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

    for scene_fname in SCENES:
        scene_dir = dirname(scene_fname)

        if os.path.split(scene_dir)[1] in EXCLUDE_FOLDERS:
            continue

        for variant in mitsuba.variants():
            if not variant.split('_')[0] == 'scalar' or variant.endswith('double'):
                continue

            if 'polarized' in variant and os.path.split(scene_dir)[1] in POLARIZED_EXCLUDE_FOLDERS:
                continue

            mitsuba.set_variant(variant)

            ref_fname, var_fname = get_ref_fname(scene_fname)
            if exists(ref_fname) and exists(var_fname) and not overwrite:
                continue

            scene = load_file(scene_fname, spp=spp)
            scene.integrator().render(scene, seed=0, develop_film=False)

            bmp = scene.sensors()[0].film().bitmap(raw=False)
            img, var_img = bitmap_extract(bmp)

            # Write rendered image to a file
            os.makedirs(dirname(ref_fname), exist_ok=True)
            xyz_to_rgb_bmp(img).write(ref_fname)
            print('Saved rendered image to: ' + ref_fname)

            # Write variance image to a file
            xyz_to_rgb_bmp(var_img).write(var_fname)
            print('Saved variance image to: ' + var_fname)


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
    args = parser.parse_args()
    render_ref_images(SCENES, **vars(args))
