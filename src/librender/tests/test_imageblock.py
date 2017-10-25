import numpy as np
import os
import pytest

from mitsuba.core import Bitmap, Struct, ReconstructionFilter, float_dtype
from mitsuba.core import MTS_WAVELENGTH_SAMPLES as N_SAMPLES
from mitsuba.core.xml import load_string
from mitsuba.render import ImageBlock


def check_value(im, arr, atol=1e-9):
    vals = np.array(im.bitmap(), copy=False)
    ref = np.empty(shape=vals.shape)
    ref[:] = arr
    # Easier to read in case of assert failure
    for k in range(vals.shape[2]):
        assert np.allclose(vals[:, :, k], ref[:, :, k], atol=atol), \
               'Channel %d:\n' % (k) + str(vals[:, :, k]) \
                              + '\n' + str(ref[:, :, k])

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

def test03_put_values():
    # Recall that we must pass a reconstruction filter to use those `put` methods.
    rfilter = load_string("""<rfilter version="2.0.0" type="box">
            <float name="radius" value="0.4"/>
        </rfilter>""")
    im = ImageBlock(Bitmap.ERGB, [10, 8], filter=rfilter)

    # From a spectrum & alpha value
    border = im.border_size()
    ref = np.zeros(shape=(im.height() + 2 * border, im.width() + 2 * border,
                          N_SAMPLES))
    for i in range(border, im.height() + border):
        for j in range(border, im.width() + border):
            # To avoid the effects of the reconstruction filter, we'll just
            # add one sample right in the center of each pixel.
            ref[i, j, :] = np.random.uniform(size=(N_SAMPLES,))
            im.put([j + 0.5, i + 0.5], ref[i, j, :], alpha=1.0)
    check_value(im, ref[:, :, :im.channel_count()], atol=1e-6)

def test04_put_packets():
    # Recall that we must pass a reconstruction filter to use those `put` methods.
    rfilter = load_string("""<rfilter version="2.0.0" type="box">
            <float name="radius" value="0.4"/>
        </rfilter>""")
    im = ImageBlock(Bitmap.ERGB, [10, 8], filter=rfilter)

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
                          N_SAMPLES))
    for i in range(n):
        (x, y) = positions[i, :] + border
        ref[int(y), int(x), :] += spectra[i, :]
    # Vectorized `put`
    im.put(positions + 0.5, spectra, alphas)

    # check_value(im, ref[:, :, 0], atol=1e-6)
    check_value(im, ref[:, :, :im.channel_count()], atol=1e-6)







