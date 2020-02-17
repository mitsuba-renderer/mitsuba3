import math
import numpy as np
import os

import pytest
import enoki as ek
import mitsuba


def check_value(im, arr, atol=1e-9):
    vals = np.array(im.data(), copy=False).reshape([im.height() + 2 * im.border_size(),
                                                    im.width() + 2 * im.border_size(),
                                                    im.channel_count()])
    ref = np.empty(shape=vals.shape)
    ref[:] = arr  # Allows to benefit from broadcasting when passing `arr`

    # Easier to read in case of assert failure
    for k in range(vals.shape[2]):
        assert ek.allclose(vals[:, :, k], ref[:, :, k], atol=atol), \
               'Channel %d:\n' % (k) + str(vals[:, :, k]) \
                              + '\n\n' + str(ref[:, :, k])

def test01_construct(variant_scalar_rgb):
    from mitsuba.core.xml import load_string
    from mitsuba.render import ImageBlock

    im = ImageBlock([33, 12], 4)
    assert im is not None
    assert ek.all(im.offset() == 0)
    im.set_offset([10, 20])
    assert ek.all(im.offset() == [10, 20])

    assert ek.all(im.size() == [33, 12])
    assert im.warn_invalid()
    assert im.border_size() == 0  # Since there's no reconstruction filter
    assert im.channel_count() == 4
    assert im.data() is not None

    rfilter = load_string("""<rfilter version="2.0.0" type="gaussian">
            <float name="stddev" value="15"/>
        </rfilter>""")
    im = ImageBlock([10, 11], 2, filter=rfilter, warn_invalid=False)
    assert im.border_size() == rfilter.border_size()
    assert im.channel_count() == 2
    assert not im.warn_invalid()

    im = ImageBlock([10, 11], 6)
    assert im is not None
    im.channel_count() == 6

def test02_put_image_block(variant_scalar_rgb):
    from mitsuba.core.xml import load_string
    from mitsuba.render import ImageBlock

    rfilter = load_string("""<rfilter version="2.0.0" type="box"/>""")
    # TODO: test with varying `offset` values
    im = ImageBlock([10, 5], 4, filter=rfilter)
    im.clear()
    check_value(im, 0)

    im2 = ImageBlock(im.size(), 4, filter=rfilter)
    im2.clear()
    ref = 3.14 * np.arange(im.height() * im.width() * im.channel_count()) \
                    .reshape(im.height(), im.width(), im.channel_count())

    for x in range(im.height()):
        for y in range(im.width()):
            im2.put([y+0.5, x+0.5], ref[x, y, :])

    check_value(im2, ref)

    for i in range(5):
        im.put(im2)
        check_value(im, (i+1) * ref)

def test03_put_values_basic(variant_scalar_rgb):
    from mitsuba.core import srgb_to_xyz
    from mitsuba.core.xml import load_string
    from mitsuba.render import ImageBlock

    # Recall that we must pass a reconstruction filter to use the `put` methods.
    rfilter = load_string("""<rfilter version="2.0.0" type="box">
            <float name="radius" value="0.4"/>
        </rfilter>""")
    im = ImageBlock([10, 8], 5, filter=rfilter)
    im.clear()

    # From a spectrum & alpha value
    border = im.border_size()
    ref = np.zeros(shape=(im.height() + 2 * border, im.width() + 2 * border, 3 + 1 + 1))
    for i in range(border, im.height() + border):
        for j in range(border, im.width() + border):
            spectrum = np.random.uniform(size=(3,))
            ref[i, j, :3] = srgb_to_xyz(spectrum)
            ref[i, j,  3] = 1  # Alpha
            ref[i, j,  4] = 1  # Weight
            # To avoid the effects of the reconstruction filter (simpler test),
            # we'll just add one sample right in the center of each pixel.
            im.put([j + 0.5, i + 0.5], [], spectrum, alpha=1.0)

    check_value(im, ref, atol=1e-6)

def test04_put_packets_basic(variant_scalar_rgb):
    from mitsuba.core import srgb_to_xyz

    try:
        mitsuba.set_variant("packet_rgb")
        from mitsuba.core.xml import load_string
        from mitsuba.render import ImageBlock
    except ImportError:
        pytest.skip("packet_rgb mode not enabled")


    # Recall that we must pass a reconstruction filter to use the `put` methods.
    rfilter = load_string("""<rfilter version="2.0.0" type="box">
            <float name="radius" value="0.4"/>
        </rfilter>""")
    im = ImageBlock([10, 8], 5, filter=rfilter)
    im.clear()

    n = 29
    positions = np.random.uniform(size=(n, 2))

    positions[:, 0] = np.floor(positions[:, 0] * (im.width()-1)).astype(np.int)
    positions[:, 1] = np.floor(positions[:, 1] * (im.height()-1)).astype(np.int)
    # Repeat some of the coordinates to make sure that we test the case where
    # the same pixel receives several values
    positions[-3:, :] = positions[:3, :]

    spectra = np.arange(n * 3).reshape((n, 3))
    alphas = np.ones(shape=(n,))

    border = im.border_size()
    ref = np.zeros(shape=(im.height() + 2 * border, im.width() + 2 * border, 3 + 1 + 1))
    for i in range(n):
        (x, y) = positions[i, :] + border
        ref[int(y), int(x), :3] +=  srgb_to_xyz(spectra[i, :])
        ref[int(y), int(x),  3] += 1  # Alpha
        ref[int(y), int(x),  4] += 1  # Weight

    # Vectorized `put`
    im.put(positions + 0.5, [], spectra, alphas)

    check_value(im, ref, atol=1e-6)

def test05_put_with_filter(variant_scalar_rgb):
    from mitsuba.core import srgb_to_xyz
    from mitsuba.core.xml import load_string
    from mitsuba.render import ImageBlock

    """The previous tests used a very simple box filter, parametrized so that
    it essentially had no effect. In this test, we use a more realistic
    Gaussian reconstruction filter, with non-zero radius."""

    try:
        mitsuba.set_variant("packet_rgb")
        from mitsuba.core.xml import load_string as load_string_packet
        from mitsuba.render import ImageBlock as ImageBlockP
    except ImportError:
        pytest.skip("packet_rgb mode not enabled")

    rfilter = load_string("""<rfilter version="2.0.0" type="gaussian">
            <float name="stddev" value="0.5"/>
        </rfilter>""")
    rfilter_p = load_string_packet("""<rfilter version="2.0.0" type="gaussian">
            <float name="stddev" value="0.5"/>
        </rfilter>""")
    size = [12, 12]
    im  = ImageBlockP(size, 5, filter=rfilter_p)
    im.clear()
    im2 = ImageBlock(size, 5, filter=rfilter)
    im2.clear()

    positions = np.array([
        [5, 6], [0, 1], [5, 6], [1, 11], [11, 11],
        [0, 1], [2, 5], [4, 1], [0, 11], [5, 4]
    ], dtype=np.float)
    n = positions.shape[0]
    positions += np.random.uniform(size=positions.shape, low=0, high=0.95)

    spectra = np.arange(n * 3).reshape((n, 3))
    alphas = np.ones(shape=(n,))

    radius = int(math.ceil(rfilter.radius()))
    border = im.border_size()
    ref = np.zeros(shape=(im.height() + 2 * border, im.width() + 2 * border, 3 + 1 + 1))

    for i in range(n):
        # -- Scalar `put`
        im2.put(positions[i, :], [], spectra[i, :], alpha=1.0)

        # Fractional part of the position
        offset = positions[i, :] - positions[i, :].astype(np.int)

        # -- Reference
        # Reconstruction window around the pixel position
        pos = positions[i, :] - 0.5 + border
        lo  = np.ceil(pos - radius).astype(np.int)
        hi  = np.floor(pos + radius).astype(np.int)

        for dy in range(lo[1], hi[1] + 1):
            for dx in range(lo[0], hi[0] + 1):
                r_pos = np.array([dx, dy])
                w_pos = r_pos - pos

                if (np.any(r_pos < 0) or np.any(r_pos >= ref.shape[:2])):
                    continue

                weight = rfilter.eval_discretized(w_pos[0]) * rfilter.eval_discretized(w_pos[1])

                xyz = srgb_to_xyz(spectra[i, :])
                ref[r_pos[1], r_pos[0], :3] += weight * xyz
                ref[r_pos[1], r_pos[0],  3] += weight * 1  # Alpha
                ref[r_pos[1], r_pos[0],  4] += weight * 1  # Weight

    # -- Vectorized `put`
    im.put(positions, [], spectra, alphas)

    check_value(im, ref, atol=1e-6)
    check_value(im2, ref, atol=1e-6)


def test06_put_values_basic(variant_scalar_spectral):
    from mitsuba.core import spectrum_to_xyz, MTS_WAVELENGTH_SAMPLES
    from mitsuba.core.xml import load_string
    from mitsuba.render import ImageBlock

    # Recall that we must pass a reconstruction filter to use the `put` methods.
    rfilter = load_string("""<rfilter version="2.0.0" type="box">
            <float name="radius" value="0.4"/>
        </rfilter>""")
    im = ImageBlock([10, 8], 5, filter=rfilter)
    im.clear()

    # From a spectrum & alpha value
    border = im.border_size()
    ref = np.zeros(shape=(im.height() + 2 * border, im.width() + 2 * border,
                          3 + 1 + 1))
    for i in range(border, im.height() + border):
        for j in range(border, im.width() + border):
            wavelengths = np.random.uniform(size=(MTS_WAVELENGTH_SAMPLES,), low=350, high=750)
            spectrum = np.random.uniform(size=(MTS_WAVELENGTH_SAMPLES,))
            ref[i, j, :3] = spectrum_to_xyz(spectrum, wavelengths)
            ref[i, j,  3] = 1  # Alpha
            ref[i, j,  4] = 1  # Weight
            # To avoid the effects of the reconstruction filter (simpler test),
            # we'll just add one sample right in the center of each pixel.
            im.put([j + 0.5, i + 0.5], wavelengths, spectrum, alpha=1.0)

    check_value(im, ref, atol=1e-6)