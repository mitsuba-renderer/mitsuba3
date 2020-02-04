import os
from os.path import join, realpath
import argparse
import mitsuba
import pytest
import enoki as ek
import numpy as np
from enoki.dynamic import Float32 as Float

from mitsuba.python.test import variants_all

TEST_SCENE_DIR = realpath(join(os.path.dirname(__file__), '../../../resources/data/tests/scenes'))
scenes = os.listdir(TEST_SCENE_DIR)

def get_ref_name():
    return 'ref_' + '_'.join(mitsuba.variant().split('_')[1:]) + '.exr'

@pytest.mark.parametrize(*['scene_name', scenes ])
def test_render(variants_all, scene_name):
    from mitsuba.core import Bitmap, Struct, Thread

    scene_dir = join(TEST_SCENE_DIR, scene_name)
    Thread.thread().file_resolver().append(scene_dir)

    ref_name = get_ref_name()
    ref_fname = join(scene_dir, ref_name)
    assert os.path.exists(ref_fname)

    scene_fname = join(scene_dir, 'scene.xml')

    scene = mitsuba.core.xml.load_file(scene_fname, parameters=[('spp', str(32))])
    scene.integrator().render(scene, scene.sensors()[0])

    film = scene.sensors()[0].film()

    cur_bitmap = film.bitmap().convert(Bitmap.PixelFormat.RGB, Struct.Type.Float32, False)
    cur_image = np.array(cur_bitmap, copy=False)

    if False:
        # Write rendered image to a file
        cur_fname = join(scene_dir, '_render_' + mitsuba.variant() + '.exr')
        cur_bitmap.write(cur_fname)
        print('Saved rendered image to: ' + cur_fname)

    ref_bitmap = Bitmap(ref_fname).convert(Bitmap.PixelFormat.RGB, Struct.Type.Float32, False)
    ref_image = np.array(ref_bitmap, copy=False)

    error = np.mean(np.mean(np.abs(ref_image - cur_image)))
    success = error < 0.001 * np.max(np.max(ref_image))

    if not success:
        # Write diff image to a file
        diff_fname = join(scene_dir, '_diff_' + mitsuba.variant() + '.exr')
        diff_image = np.abs(ref_image - cur_image)
        Bitmap(diff_image).convert(Bitmap.PixelFormat.Y, Struct.Type.Float32, False).write(diff_fname)
        print('Saved diff image to: ' + diff_fname)

        assert False



if __name__ == '__main__':
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

    for scene_name in scenes:
        scene_dir = join(TEST_SCENE_DIR, scene_name)

        for variant in mitsuba.variants():
            if not variant.split('_')[0] == 'scalar':
                continue

            mitsuba.set_variant(variant)
            from mitsuba.core import Bitmap, Struct, Thread

            ref_name = get_ref_name()
            ref_fname = join(scene_dir, ref_name)

            if os.path.exists(ref_fname) and not overwrite:
                continue

            Thread.thread().file_resolver().append(scene_dir)

            scene_fname = join(scene_dir, 'scene.xml')

            scene = mitsuba.core.xml.load_file(scene_fname, parameters=[('spp', str(ref_spp))])
            scene.integrator().render(scene, scene.sensors()[0])

            film = scene.sensors()[0].film()
            cur_bitmap = film.bitmap().convert(Bitmap.PixelFormat.RGB, Struct.Type.Float32, False)

            # Write rendered image to a file
            cur_bitmap.write(ref_fname)
            print('Saved rendered image to: ' + ref_fname)
