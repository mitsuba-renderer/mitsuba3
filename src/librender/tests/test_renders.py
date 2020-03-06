import os
from os.path import join, realpath, dirname, basename
import argparse
import glob
import mitsuba
import pytest
import enoki as ek
import numpy as np
from enoki.dynamic import Float32 as Float


@pytest.fixture(params=mitsuba.variants())
def variants_all(request):
    try:
        mitsuba.set_variant(request.param)
    except:
        pytest.skip("%s mode not enabled" % request.param)
    return request.param


color_modes = ['mono', 'rgb', 'spectral_polarized', 'spectral']

TEST_SCENE_DIR = realpath(join(os.path.dirname(__file__), '../../../resources/data/tests/scenes'))
scenes = glob.glob(join(TEST_SCENE_DIR, '*', '*.xml'))

# Exclude certain tests for now
EXCLUDE_FOLDERS = ['participating_media']


def get_ref_fname(scene_path):
    for color_mode in color_modes:
        if color_mode in mitsuba.variant():
            return join(dirname(scene_path), 'refs', os.path.splitext(basename(scene_path))[0] + '_ref_' + color_mode + '.exr')
    assert False


@pytest.mark.parametrize(*['scene_fname', scenes])
def test_render(variants_all, scene_fname):
    from mitsuba.core import Bitmap, Struct, Thread

    scene_dir = dirname(scene_fname)

    if os.path.split(scene_dir)[1] in EXCLUDE_FOLDERS:
        pytest.skip(f"Skip rendering scene {scene_fname}")

    Thread.thread().file_resolver().append(scene_dir)

    ref_fname = get_ref_fname(scene_fname)
    assert os.path.exists(ref_fname)

    scene = mitsuba.core.xml.load_file(scene_fname, spp=32)
    scene.integrator().render(scene, scene.sensors()[0])

    film = scene.sensors()[0].film()

    cur_bitmap = film.bitmap(raw=True).convert(Bitmap.PixelFormat.RGB, Struct.Type.Float32, False)
    cur_image = np.array(cur_bitmap, copy=False)

    ref_bitmap = Bitmap(ref_fname).convert(Bitmap.PixelFormat.RGB, Struct.Type.Float32, False)
    ref_image = np.array(ref_bitmap, copy=False)

    error = np.mean(np.mean(np.abs(ref_image - cur_image)))
    threshold = 0.5 * np.mean(np.mean(ref_image))
    success = error < threshold

    if not success:
        print("Failed. error: {} // threshold: {}".format(error, threshold))

        # Write rendered image to a file
        cur_fname = os.path.splitext(scene_fname)[0] + '_render_' + mitsuba.variant() + '.exr'
        cur_bitmap.write(cur_fname)
        print('Saved rendered image to: ' + cur_fname)

        assert False


def main():
    """
    Generate reference images for all the scenes contained within the TEST_SCENE_DIR directory,
    and for all the color mode having their `scalar_*` mode enabled.
    """

    parser = argparse.ArgumentParser(prog='RenderReferenceImages')
    parser.add_argument('--overwrite', action='store_true',
                        help='Force rerendering of all reference images. Otherwise, only missing references will be rendered.')
    parser.add_argument('--spp', default=256, type=int,
                        help='Samples per pixel')
    args = parser.parse_args()

    ref_spp = args.spp
    overwrite = args.overwrite

    for scene_fname in scenes:
        scene_dir = dirname(scene_fname)

        for variant in mitsuba.variants():
            if not variant.split('_')[0] == 'scalar' or variant.endswith('double'):
                continue

            mitsuba.set_variant(variant)
            from mitsuba.core import Bitmap, Struct, Thread

            ref_fname = get_ref_fname(scene_fname)
            if os.path.exists(ref_fname) and not overwrite:
                continue

            Thread.thread().file_resolver().append(scene_dir)
            scene = mitsuba.core.xml.load_file(scene_fname, spp=ref_spp)
            scene.integrator().render(scene, scene.sensors()[0])

            film = scene.sensors()[0].film()
            cur_bitmap = film.bitmap(raw=True).convert(Bitmap.PixelFormat.RGB, Struct.Type.Float32, False)

            # Write rendered image to a file
            cur_bitmap.write(ref_fname)
            print('Saved rendered image to: ' + ref_fname)


if __name__ == '__main__':
    main()
