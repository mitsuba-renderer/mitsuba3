import numpy as np
import os
import pytest

from mitsuba.scalar_rgb.core import Bitmap, Struct, ReconstructionFilter, float_dtype
from mitsuba.scalar_rgb.core.xml import load_string


def test01_construct():
    # With default reconstruction filter
    film = load_string("""<film version="2.0.0" type="hdrfilm"></film>""")
    assert film is not None
    assert film.reconstruction_filter() is not None

    # With a provided reconstruction filter
    film = load_string("""<film version="2.0.0" type="hdrfilm">
            <rfilter type="gaussian">
                <float name="stddev" value="18.5"/>
            </rfilter>
        </film>""")
    assert film is not None
    assert film.reconstruction_filter().radius() == (4 * 18.5)

    # Certain parameter values are not allowed
    with pytest.raises(RuntimeError):
        load_string("""<film version="2.0.0" type="hdrfilm">
            <string name="component_format" value="uint8"/>
        </film>""")
    with pytest.raises(RuntimeError):
        load_string("""<film version="2.0.0" type="hdrfilm">
            <string name="pixel_format" value="brga"/>
        </film>""")

def test02_crops():
    film = load_string("""<film version="2.0.0" type="hdrfilm">
            <integer name="width" value="32"/>
            <integer name="height" value="21"/>
            <integer name="crop_width" value="11"/>
            <integer name="crop_height" value="5"/>
            <integer name="crop_offset_x" value="2"/>
            <integer name="crop_offset_y" value="3"/>
            <boolean name="high_quality_edges" value="true"/>
            <string name="pixel_format" value="rgba"/>
        </film>""")
    assert film is not None
    assert np.all(film.size() == [32, 21])
    assert np.all(film.crop_size() == [11, 5])
    assert np.all(film.crop_offset() == [2, 3])
    assert film.has_high_quality_edges()
    assert film.bitmap().has_alpha()

    # Crop size doesn't adjust its size, so an error should be raised if the
    # resulting crop window goes out of bounds.
    incomplete = """<film version="2.0.0" type="hdrfilm">
            <integer name="width" value="32"/>
            <integer name="height" value="21"/>
            <integer name="crop_offset_x" value="30"/>
            <integer name="crop_offset_y" value="20"/>"""
    with pytest.raises(RuntimeError):
        film = load_string(incomplete + "</film>")
    film = load_string(incomplete + """
            <integer name="crop_width" value="2"/>
            <integer name="crop_height" value="1"/>
        </film>""")
    assert film is not None
    assert np.all(film.size() == [32, 21])
    assert np.all(film.crop_size() == [2, 1])
    assert np.all(film.crop_offset() == [30, 20])

def test03_clear_set_add():
    film = load_string("""<film version="2.0.0" type="hdrfilm">
            <integer name="width" value="32"/>
            <integer name="height" value="21"/>
            <string name="pixel_format" value="luminance"/>
            <string name="component_format" value="uint32"/>
        </film>""")
    # Note that the internal storage format is independent from the format
    # which will be developped.
    assert film.bitmap().pixel_format() == Bitmap.PixelFormat.XYZAW
    assert film.bitmap().component_format() == (
        Struct.Type.Float32 if float_dtype == np.float32
                          else Struct.Type.Float64)

    b = Bitmap(Bitmap.PixelFormat.XYZAW, Struct.Type.Float, film.bitmap().size())
    n = b.width() * b.height()
    # 0, 1, 2, ...
    ref = np.arange(n).reshape(b.height(), b.width(), 1).astype(
        float_dtype) / float(4 * (n + 1))
    assert np.all(ref >= 0) and np.all(ref <= 1)
    np.array(b, copy=False)[:] = ref

    film.set_bitmap(b)
    assert np.allclose(np.array(film.bitmap(), copy=False), ref)
    film.add_bitmap(b, multiplier=1.5)
    assert np.allclose(np.array(film.bitmap(), copy=False), (1 + 1.5) * ref,
                       atol=1e-6)
    film.clear()
    assert np.all(np.array(film.bitmap(), copy=False) == 0)
    # This shouldn't affect the original bitmap that was passed.
    assert np.all(np.array(b) == ref)

@pytest.mark.parametrize('file_format', ['exr', 'rgbe', 'pfm'])
def test04_develop(file_format, tmpdir):
    """Create a test image. Develop it to a few file format, each time reading
    it back and checking that contents are unchanged."""
    np.random.seed(12345 + ord(file_format[0]))
    # Note: depending on the file format, the alpha channel may be automatically removed.
    film = load_string("""<film version="2.0.0" type="hdrfilm">
            <integer name="width" value="41"/>
            <integer name="height" value="37"/>
            <string name="file_format" value="{}"/>
            <string name="pixel_format" value="rgba"/>
            <string name="component_format" value="float32"/>
        </film>""".format(file_format))
    # Regardless of the output file format, values are stored as XYZAW (5 channels).
    contents = np.random.uniform(size=(film.size()[1], film.size()[0], 5))
    # RGBE and will only reconstruct well images that have similar scales on
    # all channel (because exponent is shared between channels).
    if file_format is "rgbe":
        contents = 1 + 0.1 * contents
    # Use unit weights.
    contents[:, :, 4] = 1.0

    np.array(film.bitmap(), copy=False)[:] = contents

    with pytest.raises(RuntimeError):
        # Should raise when the destination file hasn't been specified.
        film.develop()

    filename = str(tmpdir.join('test_image.' + file_format))
    film.set_destination_file(filename)
    film.develop()

    # Read back and check contents
    other = Bitmap(filename).convert(Bitmap.PixelFormat.XYZAW, Struct.Type.Float, srgb_gamma=False)
    img   = np.array(other, copy=False)

    if file_format == "exr":
        assert np.allclose(img, contents, atol=1e-5)
    else:
        if file_format == "rgbe":
            # RGBE is really lossy
            if False:
                import matplotlib.pyplot as plt
                plt.figure()
                plt.subplot(1, 3, 1)
                plt.imshow(contents[:, :, :3])
                plt.subplot(1, 3, 2)
                plt.imshow(img[:, :, :3])
                plt.subplot(1, 3, 3)
                plt.imshow(np.sum(np.abs(img[:, :, :3] - contents[:, :, :3]), axis=2), cmap='coolwarm')
                plt.colorbar()
                plt.show()

            assert np.allclose(img[:, :, :3], contents[:, :, :3], atol=1e-2), \
                   '\n{}\nvs\n{}\n'.format(img[:4, :4, :3], contents[:4, :4, :3])
        else:
            assert np.allclose(img[:, :, :3], contents[:, :, :3], atol=1e-5)
        # Alpha channel was ignored, alpha and weights should default to 1.0.
        assert np.allclose(img[:, :, 3:5], 1.0, atol=1e-6)
