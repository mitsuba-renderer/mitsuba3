if __name__ == '__main__':
    import sys, os
    import argparse
    from concurrent.futures import ThreadPoolExecutor
    import mitsuba.scalar_rgb as mi

    mi.set_log_level(mi.LogLevel.Info)
    te = mi.ThreadEnvironment()

    def tonemap(fname, scale):
        with mi.ScopedSetThreadEnvironment(te):
            try:
                img_in = mi.Bitmap(fname)
                if scale != 1:
                    img_in = mi.Bitmap(mi.TensorXf(img_in) * scale)
                img_out = img_in.convert(mi.Bitmap.PixelFormat.RGB, mi.Struct.Type.UInt8, True)
                fname_out = fname.replace('.exr', '.png')
                img_out.write(fname_out)
                mi.Log(mi.LogLevel.Info, 'Wrote "%s".' % fname_out)
            except Exception as e:
                sys.stderr.write('Could not tonemap image "%s": %s!\n' %
                    (fname, str(e)))

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
