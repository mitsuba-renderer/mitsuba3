"""Command line renderer: ``python -m mitsuba [options] <scene.xml> ...``"""

import argparse
import sys

import drjit as dr
import mitsuba as mi
from mitsuba import detail


def main(argv=None):
    thread_count = dr.thread_count() + 1
    banner = '\n'.join([detail.info_build(thread_count),
                        detail.info_copyright(),
                        detail.info_features()])

    p = argparse.ArgumentParser(
        prog='python -m mitsuba', description=banner,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('scenes', nargs='*', metavar='<scene.xml>',
                   help='One or more scene XML files')
    p.add_argument('-m', '--mode', metavar='<variant>',
                   help='Request a specific variant of the renderer. Default: '
                        'the most capable variant whose backend is available '
                        'at runtime (preferring an RGB color representation). '
                        'Available: ' + ', '.join(mi.variants()))
    p.add_argument('-v', '--verbose', action='count', default=0,
                   help='Be more verbose (can be specified multiple times)')
    p.add_argument('-t', '--threads', type=int, metavar='<count>',
                   help='Render with the specified number of threads')
    p.add_argument('-D', '--define', action='append', default=[],
                   metavar='<key>=<value>',
                   help='Define a constant that can be referenced as "$key" '
                        'within the scene description')
    p.add_argument('-s', '--sensor', type=int, default=0, metavar='<index>',
                   help='Index of the sensor to render with (following the '
                        'declaration order in the scene file). Default: 0')
    p.add_argument('-a', '--append', action='append', default=[],
                   metavar='<path1>;<path2>;..',
                   help='Add one or more entries to the resource search path')
    p.add_argument('-o', '--output', metavar='<filename>',
                   help='Write the output image to the file "filename"')
    p.add_argument('-S', dest='print_ir', action='store_true',
                   help='Dump the PTX or LLVM intermediate representation to '
                        'the console (JIT variants only)')
    args = p.parse_args(argv)

    if not args.scenes:
        p.print_help()
        return 0

    try:
        log_level = args.verbose + (1 if mi.DEBUG else 0)
        mi.set_log_level([mi.LogLevel.Info, mi.LogLevel.Debug,
                          mi.LogLevel.Trace][min(log_level, 2)])
        dr.set_log_level([dr.LogLevel.Error, dr.LogLevel.Warn,
                          dr.LogLevel.Info, dr.LogLevel.InfoSym,
                          dr.LogLevel.Debug, dr.LogLevel.Trace][min(log_level, 5)])

        if args.threads is not None:
            thread_count = args.threads
            if thread_count < 1:
                mi.Log(mi.LogLevel.Warn, 'Thread count should be greater '
                       'than 0. It will be set to 1 instead.')
                thread_count = 1
            dr.set_thread_count(thread_count - 1)

        params = {}
        for define in args.define:
            key, sep, value = define.partition('=')
            if not sep:
                raise RuntimeError('-D/--define: expect key=value pair!')
            params[key] = value

        for path in args.append:
            for entry in path.split(';'):
                mi.file_resolver().append(entry)

        # mi.variants() is ordered by descending preference
        mi.set_variant(*([args.mode] if args.mode else mi.variants()))

        if args.print_ir:
            if not mi.variant().startswith(('cuda_', 'llvm_', 'metal_')):
                raise RuntimeError('Specified an argument that only makes '
                                   'sense in a JIT (LLVM/CUDA/Metal) mode!')
            dr.set_flag(dr.JitFlag.PrintIR, True)

        mi.Log(mi.LogLevel.Info, detail.info_build(thread_count))
        mi.Log(mi.LogLevel.Info, detail.info_copyright())
        mi.Log(mi.LogLevel.Info, detail.info_features())
        mi.Log(mi.LogLevel.Info, 'Rendering using the "%s" variant.' % mi.variant())
        if mi.DEBUG:
            mi.Log(mi.LogLevel.Warn, 'Renderer is compiled in debug mode, '
                   'performance will be considerably reduced.')

        for filename in args.scenes:
            scene = mi.load_file(filename, **params)
            if not isinstance(scene, mi.Scene):
                raise RuntimeError('Root element of the input file must be '
                                   'a <scene> tag!')
            integrator = scene.integrator()
            if integrator is None:
                raise RuntimeError('No integrator specified for scene: %s' % scene)
            integrator.render(scene, args.sensor, develop=False, profile=True)
            scene.sensors()[args.sensor].film().write(args.output or filename)
    except Exception as e:
        print('\n\x1b[31mCaught a critical exception: %s\x1b[0m' % e,
              file=sys.stderr)
        return -1

    return 0


if __name__ == '__main__':
    sys.exit(main())
