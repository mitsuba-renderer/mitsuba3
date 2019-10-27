from mitsuba.core import util, Bitmap, ThreadEnvironment, \
    ScopedSetThreadEnvironment, Thread, Info
from threading import Thread as PyThread, Semaphore
import sys
import os

sema = Semaphore(util.core_count())
te = ThreadEnvironment()
Thread.thread().logger().set_log_level(Info)


def tonemap(fname):
    with ScopedSetThreadEnvironment(te):
        try:
            img_in = Bitmap(fname)
            img_out = img_in.convert(Bitmap.ERGB, Bitmap.EUInt8, True)
            fname_out = fname.replace('exr', 'png')
            img_out.write(fname_out)
        except Exception as e:
            sys.stderr.write('Could not tonemap image "%s": %s!\n' %
                (fname, str(e)))
        sema.release()


if __name__ == '__main__':
    if len(sys.argv) == 1:
        print('Syntax: python %s <EXR file(s)>' % sys.argv[0])
        exit()

    for fname in sys.argv[1:]:
        if os.path.exists(fname):
            print(fname)
            sema.acquire()
            thread = PyThread(target=tonemap, args=(fname,))
            thread.start()
        else:
            sys.stderr.write('File "%s" not found!\n' % fname)

    for _ in range(8):
        sema.acquire()
