import math
import numpy as np
import os
import pytest

from mitsuba.core import Bitmap, Struct, ReconstructionFilter, float_dtype
from mitsuba.core import cie1931_xyz, \
                         MTS_WAVELENGTH_SAMPLES as N_SAMPLES, \
                         MTS_WAVELENGTH_MIN as W_MIN, \
                         MTS_WAVELENGTH_MAX as W_MAX
from mitsuba.core.xml import load_string
from mitsuba.render import ImageBlock


def convert_to_xyz(spectrum):
    # TODO: use sampled wavelengths
    wavelengths = np.array([W_MIN, 517, 673, W_MAX])
    responses = cie1931_xyz(wavelengths)

    xyz = np.zeros(shape=(3,))
    for li in range(N_SAMPLES):
        xyz[0] += spectrum[li] * responses[0][li]
        xyz[1] += spectrum[li] * responses[1][li]
        xyz[2] += spectrum[li] * responses[2][li]

    return xyz

def check_value(im, arr, atol=1e-9):
    vals = np.array(im.bitmap(), copy=False)
    ref = np.empty(shape=vals.shape)
    ref[:] = arr  # Allows to benefit from broadcasting when passing `arr`

    # Easier to read in case of assert failure
    for k in range(vals.shape[2]):
        assert np.allclose(vals[:, :, k], ref[:, :, k], atol=atol), \
               'Channel %d:\n' % (k) + str(vals[:, :, k]) \
                              + '\n\n' + str(ref[:, :, k])

def test01_construct():
    im = ImageBlock(Bitmap.ERGBA, [33, 12])
    assert im is not None
    assert np.all(im.offset() == 0)
    im.set_offset([10, 20])
    assert np.all(im.offset() == [10, 20])

    assert np.all(im.size() == [33, 12])
    assert im.warns()
    assert im.border_size() == 0  # Since there's no reconstruction filter
    assert im.channel_count() == 4
    assert im.pixel_format() == Bitmap.ERGBA
    assert im.bitmap() is not None

    rfilter = load_string("""<rfilter version="2.0.0" type="gaussian">
            <float name="stddev" value="15"/>
        </rfilter>""")
    im = ImageBlock(Bitmap.EYA, [10, 11], filter=rfilter, channels=2, warn=False)
    assert im.border_size() == rfilter.border_size()
    assert im.channel_count() == 2
    assert not im.warns()

    # Can't specify a number of channels, unless we're using the multichannel
    # pixel format.
    with pytest.raises(RuntimeError):
        ImageBlock(Bitmap.EYA, [10, 11], channels=5)
    im = ImageBlock(Bitmap.EMultiChannel, [10, 11], channels=6)
    assert im is not None
    im.channel_count() == 6

def test02_put_image_block():
    # TODO: test with varying `offset` values
    im = ImageBlock(Bitmap.ERGBA, [10, 5])
    # Should be cleared right away.
    check_value(im, 0)

    im2 = ImageBlock(Bitmap.ERGBA, im.size())
    ref = 3.14 * np.arange(
            im.height() * im.width()).reshape(im.height(), im.width(), 1)
    np.array(im2.bitmap(), copy=False)[:] = ref
    check_value(im2, ref)

    for i in range(5):
        im.put(im2)
        check_value(im, (i+1) * ref)

def test03_put_values_basic():
    # Recall that we must pass a reconstruction filter to use the `put` methods.
    rfilter = load_string("""<rfilter version="2.0.0" type="box">
            <float name="radius" value="0.4"/>
        </rfilter>""")
    im = ImageBlock(Bitmap.EXYZAW, [10, 8], filter=rfilter)

    # From a spectrum & alpha value
    border = im.border_size()
    ref = np.zeros(shape=(im.height() + 2 * border, im.width() + 2 * border,
                          3 + 1 + 1))
    for i in range(border, im.height() + border):
        for j in range(border, im.width() + border):
            spectrum = np.random.uniform(size=(N_SAMPLES,))
            ref[i, j, :3] = convert_to_xyz(spectrum)
            ref[i, j,  3] = 1  # Alpha
            ref[i, j,  4] = 1  # Weight
            # To avoid the effects of the reconstruction filter (simpler test),
            # we'll just add one sample right in the center of each pixel.
            im.put([j + 0.5, i + 0.5], spectrum, alpha=1.0)
    check_value(im, ref, atol=1e-6)

def test04_put_packets_basic():
    # Recall that we must pass a reconstruction filter to use the `put` methods.
    rfilter = load_string("""<rfilter version="2.0.0" type="box">
            <float name="radius" value="0.4"/>
        </rfilter>""")
    im = ImageBlock(Bitmap.EXYZAW, [10, 8], filter=rfilter)

    n = 29
    positions = np.random.uniform(size=(n, 2))
    positions[:, 0] = np.floor(positions[:, 0] * (im.width()-1)).astype(np.int)
    positions[:, 1] = np.floor(positions[:, 1] * (im.height()-1)).astype(np.int)
    # Repeat some of the coordinates to make sure that we test the case where
    # the same pixel receives several values
    positions[-3:, :] = positions[:3, :]

    spectra = np.arange(n * N_SAMPLES).reshape((n, N_SAMPLES))
    alphas = np.ones(shape=(n,))

    border = im.border_size()
    ref = np.zeros(shape=(im.height() + 2 * border, im.width() + 2 * border,
                          3 + 1 + 1))
    for i in range(n):
        (x, y) = positions[i, :] + border
        ref[int(y), int(x), :3] += convert_to_xyz(spectra[i, :])
        ref[int(y), int(x),  3] += 1  # Alpha
        ref[int(y), int(x),  4] += 1  # Weight
    # Vectorized `put`
    im.put(positions + 0.5, spectra, alphas)

    check_value(im, ref, atol=1e-6)

def test05_put_with_filter():
    """The previous tests used a very simple box filter, parametrized so that
    it essentially had no effect. In this test, we use a more realistic
    Gaussian reconstruction filter, with non-zero radius."""
    rfilter = load_string("""<rfilter version="2.0.0" type="gaussian">
            <float name="stddev" value="0.5"/>
        </rfilter>""")
    size = [12, 12]
    im = ImageBlock(Bitmap.EXYZAW, size, filter=rfilter)
    im2 = ImageBlock(Bitmap.EXYZAW, size, filter=rfilter)

    positions = np.array([
        [5, 6], [0, 1], [5, 6], [1, 11], [11, 11],
        [0, 1], [2, 5], [4, 1], [0, 11], [5, 4]
    ], dtype=np.float)
    n = positions.shape[0]
    positions += np.random.uniform(size=positions.shape, low=0, high=0.95)

    spectra = np.arange(n * N_SAMPLES).reshape((n, N_SAMPLES))
    alphas = np.ones(shape=(n,))

    radius = int(math.ceil(rfilter.radius()))
    border = im.border_size()
    ref = np.zeros(shape=(im.height() + 2 * border, im.width() + 2 * border,
                          3 + 1 + 1))
    for i in range(n):
        # -- Scalar `put`
        im2.put(positions[i, :], spectra[i, :], alpha=1.0)

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

                weight = rfilter.eval_discretized(w_pos[0]) \
                         * rfilter.eval_discretized(w_pos[1])

                xyz = convert_to_xyz(spectra[i, :])
                ref[r_pos[1], r_pos[0], :3] += weight * xyz
                ref[r_pos[1], r_pos[0],  3] += weight * 1  # Alpha
                ref[r_pos[1], r_pos[0],  4] += weight * 1  # Weight

    # -- Vectorized `put`
    im.put(positions, spectra, alphas)

    check_value(im, ref, atol=1e-6)
    check_value(im2, ref, atol=1e-6)

