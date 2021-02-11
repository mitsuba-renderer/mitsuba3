import mitsuba
import numpy as np
import argparse
mitsuba.set_variant('scalar_rgb')

from mitsuba.core import Bitmap, ThreadEnvironment, \
    ScopedSetThreadEnvironment, Thread, LogLevel, Struct, Log

from concurrent.futures import ThreadPoolExecutor

import sys
import os

te = ThreadEnvironment()
Thread.thread().logger().set_log_level(LogLevel.Info)

def tonemap(fname, scale):
    with ScopedSetThreadEnvironment(te):
        try:
            img_in = Bitmap(fname)
            img_py = np.array(img_in, copy=False)
            img_py *= scale
            img_out = img_in.convert(Bitmap.PixelFormat.RGB, Struct.Type.UInt8, True)
            fname_out = fname.replace('.exr', '.png')
            img_out.write(fname_out)
            Log(LogLevel.Info, 'Wrote "%s".' % fname_out)
        except Exception as e:
            sys.stderr.write('Could not tonemap image "%s": %s!\n' %
                (fname, str(e)))


if __name__ == '__main__':
    class MyParser(argparse.ArgumentParser):
        def error(self, message):
            sys.stderr.write('error: %s\n' % message)
            self.print_help()
            sys.exit(2)

    parser = MyParser(description=
        'This program loads a sequence of EXR files and writes tonemapped PNGs using '
        'a sRGB response curve. Blue-noise dithering is applied to avoid banding '
        'artifacts in this process.')
    parser.add_argument('--scale', type=float, default='1', help='Optional scale factor that should be applied before tonemapping')
    parser.add_argument('file', type=str, nargs='+', help='One more more EXR files to be processed')
    args = parser.parse_args()

    with ThreadPoolExecutor() as exc:
        for f in args.file:
            if os.path.exists(f):
                exc.submit(tonemap, f, args.scale)
            else:
                sys.stderr.write('File "%s" not found!\n' % f)
