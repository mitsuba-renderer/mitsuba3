import numpy as np

def polvis(fname, args):
    import mitsuba.scalar_rgb as mi
    mi.set_log_level(mi.LogLevel.Info)

    te = mi.ThreadEnvironment()
    with mi.ScopedSetThreadEnvironment(te):
        try:
            assert fname[-4:] == '.exr', "Needs to be a EXR image."
            name = fname[:-4]

            # Load image and convert to NumPy array
            img_in = mi.Bitmap(fname)
            img_py = np.array(img_in, copy=False)
            assert img_py.shape[2] == 16, "Needs to be a 16-channel image output by the `stokes` integrator."

            if not args.scale is None:
                # Apply a linear scale factor to the image before generating outputs.
                img_py *= args.scale

            if args.intensity:
                # Write out just the S0 (intensity) channels
                s0 = img_py[:, :, 4:7]
                mi.Bitmap(s0).convert(mi.Bitmap.PixelFormat.RGB, mi.Struct.Type.UInt8, True).write("%s_intensity.png" % name)

            if not args.polarizer is None:
                # Write out intensity after applying a linear polarizer at a given angle to the image

                # Create linear polarizer at right rotation
                LP = mi.mueller.linear_polarizer(1.0)
                angle = args.polarizer
                LP = mi.mueller.rotated_element(np.radians(angle), LP)
                LP = np.array(LP)

                # Extract Stokes vector for all channels
                stokes_rgb = [img_py[:, :, 4::3], img_py[:, :, 5::3], img_py[:, :, 6::3]]
                # Apply Mueller matrix to all pixels and extract intensity (S0) component
                out = np.dstack([(img @ LP.T)[:, :, 0] for img in stokes_rgb])

                mi.Bitmap(out).convert(mi.Bitmap.PixelFormat.RGB, mi.Struct.Type.UInt8, True).write("%s_polarizer_%.02f.png" % (name, angle))

            # Extract Stokes vectors. Either one channel only (if `--channel` is specified), or else average all.
            stokes = None
            if not args.channel is None:
                stokes = img_py[:, :, 4+args.channel::3]
            else:
                stokes_rgb = [img_py[:, :, 4::3], img_py[:, :, 5::3], img_py[:, :, 6::3]]
                stokes = np.mean(stokes_rgb, axis=0)

            # Create the various quantities that are used by false-color visualizations
            s0   = stokes[:, :, 0]
            s3   = stokes[:, :, 3]
            s12  = np.sqrt(np.maximum(0.0, stokes[:, :, 1]**2 + stokes[:, :, 2]**2))
            s123 = np.sqrt(np.maximum(0.0, stokes[:, :, 1]**2 + stokes[:, :, 2]**2 + stokes[:, :, 3]**2))

            dop    = np.divide(s123, s0, out=np.zeros_like(s0), where=s0!=0)
            rdop_l = np.divide(s12, s123, out=np.zeros_like(s0), where=s123!=0)
            rdop_c = np.divide(np.abs(s3), s123, out=np.zeros_like(s0), where=s123!=0) # The original paper does not have the abs(...) here, but that leads to negative values in the final false-color images.

            black_white = np.dstack([s0, s0, s0])

            if args.stokes or args.stokes_nrm:
                # Write out false-color images of the raw Stokes vectors.
                out_list = [s0]
                for i in range(3):
                    tmp = stokes[:, :, 1+i]
                    if args.stokes_nrm:
                        tmp = np.divide(tmp, s0, out=np.zeros_like(s0), where=s0!=0)

                    # Red channel: negative, green channel: positive, blue channel: unused
                    out = np.dstack([np.maximum(0, -tmp),
                                     np.maximum(0,  tmp),
                                     np.zeros(s0.shape)])

                    if args.direct_overlay or args.luminance_overlay:
                        # Optionally overlay the false-color information on top of the original black and white image.
                        # In this case, also normalize the Stokes components based on `S0`.
                        alpha = dop[:, :, np.newaxis]
                        if args.luminance_overlay:
                            out *= s0[:, :, np.newaxis]
                        out = out*alpha + black_white*(1 - alpha)

                    out_list.append(out)

                for idx, img in enumerate(out_list):
                    mi.Bitmap(img).convert(mi.Bitmap.PixelFormat.RGB, mi.Struct.Type.UInt8, True).write("%s_s%d.png" % (name, idx))

            if args.dop:
                # Write out false-color image of degree of polarization.
                # Uses the red channel only
                z = np.zeros(s0.shape)
                out = np.dstack([dop, z, z])

                if args.direct_overlay or args.luminance_overlay:
                    # Optionally overlay the false-color information on top of the original black and white image.
                    alpha = dop[:, :, np.newaxis]
                    if args.luminance_overlay:
                        out *= s0[:, :, np.newaxis]
                    out = out*alpha + black_white*(1 - alpha)

                mi.Bitmap(out).convert(mi.Bitmap.PixelFormat.RGB, mi.Struct.Type.UInt8, True).write("%s_dop.png" % (name))

            if args.top:
                # Write out false-color image of the type of polarization (linear vs. circular)

                # Cyan for linear polarization, yellow for circular polarization.
                c_top = np.dstack([rdop_c, rdop_l + rdop_c, rdop_l])
                out = c_top * dop[:, :, np.newaxis]

                if args.direct_overlay or args.luminance_overlay:
                    # Optionally overlay the false-color information on top of the original black and white image.
                    alpha = dop[:, :, np.newaxis]
                    if args.luminance_overlay:
                        out *= s0[:, :, np.newaxis]
                    out = out*alpha + black_white*(1 - alpha)

                mi.Bitmap(out).convert(mi.Bitmap.PixelFormat.RGB, mi.Struct.Type.UInt8, True).write("%s_top.png" % (name))

            if args.lin:
                # Write out false-color image for the oscillation plane of linear polarization.

                s1_nrm = np.divide(stokes[:, :, 1], s0, out=np.zeros_like(s0), where=s0!=0)
                s2_nrm = np.divide(stokes[:, :, 2], s0, out=np.zeros_like(s0), where=s0!=0)

                # S1. Red: negative, green: positive
                out_a = np.dstack([np.maximum(0, -s1_nrm),
                                   np.maximum(0,  s1_nrm),
                                   np.zeros(s0.shape)])
                # S2: Blue: negative, yellow: positive
                out_b = np.dstack([np.maximum(0,  s2_nrm),
                                   np.maximum(0,  s2_nrm),
                                   np.maximum(0, -s2_nrm)])

                out = (out_a + out_b) * rdop_l[:, :, np.newaxis]

                if args.direct_overlay or args.luminance_overlay:
                    # Optionally overlay the false-color information on top of the original black and white image.
                    alpha = rdop_l[:, :, np.newaxis]
                    if args.luminance_overlay:
                        out *= s0[:, :, np.newaxis]
                    out = out*alpha + black_white*(1 - alpha)

                mi.Bitmap(out).convert(mi.Bitmap.PixelFormat.RGB, mi.Struct.Type.UInt8, True).write("%s_lin.png" % (name))

            if args.cir:
                # Write out false-color image for the chirality of circular polarization.

                s3_nrm = np.divide(stokes[:, :, 3], s0, out=np.zeros_like(s0), where=s0!=0)

                # Blue: right circular polarization, yellow: left circular polarization
                cir = np.dstack([np.maximum(0, -s3_nrm),
                                 np.maximum(0, -s3_nrm),
                                 np.maximum(0,  s3_nrm)])
                out = cir * rdop_c[:, :, np.newaxis]

                if args.direct_overlay or args.luminance_overlay:
                    # Optionally overlay the false-color information on top of the original black and white image.
                    alpha = rdop_c[:, :, np.newaxis]
                    if args.luminance_overlay:
                        out *= s0[:, :, np.newaxis]
                    out = out*alpha + black_white*(1 - alpha)

                mi.Bitmap(out).convert(mi.Bitmap.PixelFormat.RGB, mi.Struct.Type.UInt8, True).write("%s_cir.png" % (name))

        except Exception as e:
            sys.stderr.write('Could not apply polarization visualization to image "%s": %s!\n' %
                (fname, str(e)))

if __name__ == '__main__':
    import sys, os, argparse
    from concurrent.futures import ThreadPoolExecutor

    class MyParser(argparse.ArgumentParser):
        def error(self, message):
            sys.stderr.write('error: %s\n' % message)
            self.print_help()
            sys.exit(2)

    parser = MyParser(description=
        'This program creates visualizations for the polarization specific information from '
        'EXR images rendered with the `stokes` integrator in Mitsuba 3. '
        'The various false-color images are described in the paper "A Standardised '
        'Polarisation Visualisation for Images" by Wilkie and Weidlich (SCCG 2010).')

    parser.add_argument('file', type=str, nargs='+',
                        help='One more more EXR files to be processed')
    parser.add_argument('-s', '--scale', type=float, default=None,
                        help='Scale the input EXR image by a linear factor before computing the visualizations.')
    parser.add_argument('-i', '--intensity', action='store_true',
                        help='Output an RGB image for the intensity channel. (I.e. extract `S0`.)')
    parser.add_argument('--polarizer', type=float, default=None, metavar='ANGLE',
                        help='Output an RGB image for the intensity channel, but after first applying a linear polarizer rotated at a specified angle (specified in degrees and measured counter-clockwise from the horizontal axis).')

    parser.add_argument('--stokes', action='store_true',
                        help='Write out false-color visualizations for the raw Stokes components.')
    parser.add_argument('--stokes_nrm', action='store_true',
                        help='Same as `--stokes`, but first normalize the `S1, S2, S3` components based on `S0`.')
    parser.add_argument('--dop', action='store_true',
                        help='Write out false-color visualization for the degree of polarization.')
    parser.add_argument('--top', action='store_true',
                        help='Write out false-color visualization for the type of polarization (linear vs. circular).')
    parser.add_argument('--lin', action='store_true',
                        help='Write out false-color visualization for the oscillation plane of linear polarization.')
    parser.add_argument('--cir', action='store_true',
                        help='Write out false-color visualization for the chirality of circular polarization (right vs. left circular polarization).')

    parser.add_argument('--channel', type=int, default=None,
                        help='Which channel (R=0, G=1, B=2) should be used for the false-color visualizations? If not specified, the RGB channels will be averaged instead.')

    parser.add_argument('--direct_overlay', action='store_true',
                        help='False-color visualizations are composited on top of the black and white image as a "direct overlay".')
    parser.add_argument('--luminance_overlay', action='store_true',
                        help='False-color visualizations are composited on top of the black and white image as a "luminance scaled overlay".')

    args = parser.parse_args()

    with ThreadPoolExecutor() as exc:
        for f in args.file:
            if os.path.exists(f):
                exc.submit(polvis, f, args)
            else:
                sys.stderr.write('File "%s" not found!\n' % f)
