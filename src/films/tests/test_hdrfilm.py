import mitsuba
import pytest
import os
import enoki as ek


def test01_construct(variant_scalar_rgb):
    from mitsuba.core.xml import load_string

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


def test02_crops(variant_scalar_rgb):
    from mitsuba.core.xml import load_string

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
    assert ek.all(film.size() == [32, 21])
    assert ek.all(film.crop_size() == [11, 5])
    assert ek.all(film.crop_offset() == [2, 3])
    assert film.has_high_quality_edges()

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
    assert ek.all(film.size() == [32, 21])
    assert ek.all(film.crop_size() == [2, 1])
    assert ek.all(film.crop_offset() == [30, 20])


@pytest.mark.parametrize('file_format', ['exr', 'rgbe', 'pfm'])
def test03_develop(variant_scalar_rgb, file_format, tmpdir):
    from mitsuba.core.xml import load_string
    from mitsuba.core import Bitmap, Struct, ReconstructionFilter, float_dtype
    from mitsuba.render import ImageBlock
    import numpy as np

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
            <rfilter type="box"/>
        </film>""".format(file_format))
    # Regardless of the output file format, values are stored as XYZAW (5 channels).
    contents = np.random.uniform(size=(film.size()[1], film.size()[0], 5))
    # RGBE and will only reconstruct well images that have similar scales on
    # all channel (because exponent is shared between channels).
    if file_format is "rgbe":
        contents = 1 + 0.1 * contents
    # Use unit weights.
    contents[:, :, 4] = 1.0

    block = ImageBlock(film.size(), 5, film.reconstruction_filter())

    block.clear()
    for x in range(film.size()[1]):
        for y in range(film.size()[0]):
            block.put([y+0.5, x+0.5], contents[x, y, :])

    film.prepare(['X', 'Y', 'Z', 'A', 'W'])
    film.put(block)

    with pytest.raises(RuntimeError):
        # Should raise when the destination file hasn't been specified.
        film.develop()

    filename = str(tmpdir.join('test_image.' + file_format))
    film.set_destination_file(filename)
    film.develop()

    # Read back and check contents
    other = Bitmap(filename).convert(Bitmap.PixelFormat.XYZAW, Struct.Type.Float32, srgb_gamma=False)
    img   = np.array(other, copy=False)

    if False:
        import matplotlib.pyplot as plt
        plt.figure()
        plt.subplot(1, 3, 1)
        plt.imshow(contents[:, :, :3])
        plt.subplot(1, 3, 2)
        plt.imshow(img[:, :, :3])
        plt.subplot(1, 3, 3)
        plt.imshow(ek.sum(ek.abs(img[:, :, :3] - contents[:, :, :3]), axis=2), cmap='coolwarm')
        plt.colorbar()
        plt.show()

    if file_format == "exr":
        assert ek.allclose(img, contents, atol=1e-5)
    else:
        if file_format == "rgbe":
            assert ek.allclose(img[:, :, :3], contents[:, :, :3], atol=1e-2), \
                   '\n{}\nvs\n{}\n'.format(img[:4, :4, :3], contents[:4, :4, :3])
        else:
            assert ek.allclose(img[:, :, :3], contents[:, :, :3], atol=1e-5)
        # Alpha channel was ignored, alpha and weights should default to 1.0.
        assert ek.allclose(img[:, :, 3:5], 1.0, atol=1e-6)
