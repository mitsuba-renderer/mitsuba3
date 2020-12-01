import mitsuba
import numpy
mitsuba.set_variant('scalar_rgb')

from mitsuba.core import Bitmap, ThreadEnvironment, \
    ScopedSetThreadEnvironment, Thread, LogLevel, Struct, Log

from concurrent.futures import ThreadPoolExecutor

import sys
import os

te = ThreadEnvironment()
Thread.thread().logger().set_log_level(LogLevel.Info)

def tonemap(fname):
    with ScopedSetThreadEnvironment(te):
        try:
            img_in = Bitmap(fname)
            img_out = img_in.convert(Bitmap.PixelFormat.RGB, Struct.Type.UInt8, True)
            fname_out = fname.replace('.exr', '.png')
            img_out.write(fname_out)
            Log(LogLevel.Info, 'Wrote "%s".' % fname_out)
        except Exception as e:
            sys.stderr.write('Could not tonemap image "%s": %s!\n' %
                (fname, str(e)))


if __name__ == '__main__':
    if len(sys.argv) == 1:
        print('Syntax: python %s <EXR file(s)>' % sys.argv[0])
        exit()

    with ThreadPoolExecutor() as exc:
        for fname in sys.argv[1:]:
            if os.path.exists(fname):
                exc.submit(tonemap, fname)
            else:
                sys.stderr.write('File "%s" not found!\n' % fname)
