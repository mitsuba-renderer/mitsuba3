import enoki as ek
import mitsuba
import pytest


def test01_construct(variant_scalar_rgb):
    from mitsuba.core import load_string

    # With default reconstruction filter
    film = load_string("""<film version="2.0.0" type="hdrfilm"></film>""")
    assert film is not None
    assert film.rfilter() is not None

    # With a provided reconstruction filter
    film = load_string("""<film version="2.0.0" type="hdrfilm">
            <rfilter type="gaussian">
                <float name="stddev" value="18.5"/>
            </rfilter>
        </film>""")
    assert film is not None
    assert film.rfilter().radius() == (4 * 18.5)

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
    from mitsuba.core import load_string

    film = load_string("""<film version="2.0.0" type="hdrfilm">
            <integer name="width" value="32"/>
            <integer name="height" value="21"/>
            <integer name="crop_width" value="11"/>
            <integer name="crop_height" value="5"/>
            <integer name="crop_offset_x" value="2"/>
            <integer name="crop_offset_y" value="3"/>
            <boolean name="sample_border" value="true"/>
            <string name="pixel_format" value="rgba"/>
        </film>""")
    assert film is not None
    assert ek.all(film.size() == [32, 21])
    assert ek.all(film.crop_size() == [11, 5])
    assert ek.all(film.crop_offset() == [2, 3])
    assert film.sample_border()

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
def test03_bitmap(variant_scalar_rgb, file_format, tmpdir):
    """Create a test image. Develop it to a few file format, each time reading
    it back and checking that contents are unchanged."""
    from mitsuba.core import load_string, Bitmap, Struct
    from mitsuba.render import ImageBlock
    import numpy as np

    rng = np.random.default_rng(seed=12345 + ord(file_format[0]))
    # Note: depending on the file format, the alpha channel may be automatically removed.
    film = load_string("""<film version="2.0.0" type="hdrfilm">
            <integer name="width" value="41"/>
            <integer name="height" value="37"/>
            <string name="file_format" value="{}"/>
            <string name="pixel_format" value="rgba"/>
            <string name="component_format" value="float32"/>
            <rfilter type="box"/>
        </film>""".format(file_format))
    # Regardless of the output file format, values are stored as RGBAW (5 channels).
    contents = rng.uniform(size=(film.size()[1], film.size()[0], 5))
    # RGBE and will only reconstruct well images that have similar scales on
    # all channel (because exponent is shared between channels).
    if file_format == "rgbe":
        contents = 1 + 0.1 * contents
    # Use unit weights.
    contents[:, :, 4] = 1.0

    block = ImageBlock(0, film.size(), 5, film.rfilter())
    for x in range(film.size()[1]):
        for y in range(film.size()[0]):
            block.put([y+0.5, x+0.5], contents[x, y, :])

    film.prepare([])
    film.put_block(block)

    filename = str(tmpdir.join('test_image.' + file_format))
    film.write(filename)

    # Read back and check contents
    other = Bitmap(filename).convert(Bitmap.PixelFormat.RGBAW, Struct.Type.Float32, srgb_gamma=False)
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


@pytest.mark.parametrize('pixel_format', ['RGB', 'RGBA', 'XYZ', 'XYZA', 'luminance', 'luminance_alpha'])
@pytest.mark.parametrize('has_aovs', [False, True])
def test04_develop_and_bitmap(variants_all_rgb, pixel_format, has_aovs):
    from mitsuba.core import load_dict, Bitmap, Float, UInt32, Point2f
    from mitsuba.render import ImageBlock
    import numpy as np

    aovs_channels = ['aov.r', 'aov.g', 'aov.b'] if has_aovs else []
    if pixel_format == 'luminance':
        output_channels = ['Y'] + aovs_channels
    elif pixel_format == 'luminance_alpha':
        output_channels = ['Y', 'A'] + aovs_channels
    else:
        output_channels = list(pixel_format) + aovs_channels

    film = load_dict({
        'type': 'hdrfilm',
        'pixel_format': pixel_format,
        'component_format': 'float32',
        'width': 3,
        'height': 5,
        # 'rfilter': { 'type': 'box' }
    })

    res = film.size()
    block = ImageBlock(0, res, 5 + len(aovs_channels), film.rfilter())

    if ek.is_jit_array_v(Float):
        pixel_idx = ek.arange(UInt32, ek.hprod(res))
        x = pixel_idx % res[0]
        y = pixel_idx // res[0]

        pos = Point2f(x, y) + 0.5

        aovs = [10 + x, 20 + y, 10.1] if has_aovs else []
        block.put(pos, [x, 2*y, 0.1, 0.5, 1.0] + aovs)
    else:
        for x in range(res[1]):
            for y in range(res[0]):
                aovs = [10 + x, 20 + y, 10.1] if has_aovs else []
                block.put([y + 0.5, x + 0.5], [x, 2*y, 0.1, 0.5, 1.0] + aovs)

    film.prepare(aovs_channels)
    film.put_block(block)

    image = film.develop()

    assert ek.hprod(image.shape) == ek.hprod(res) * (len(output_channels))

    data_bitmap = Bitmap(image, Bitmap.PixelFormat.MultiChannel, output_channels)
    bitmap = film.bitmap()
    assert np.allclose(np.array(bitmap), np.array(data_bitmap))


def test05_without_prepare(variant_scalar_rgb):
    from mitsuba.core import load_dict

    film = load_dict({
        'type': 'hdrfilm',
        'width': 3,
        'height': 2,
    })

    with pytest.raises(RuntimeError, match=r'prepare\(\)'):
        _ = film.develop()


@pytest.mark.parametrize('develop', [False, True])
def test06_empty_film(variants_all_rgb, develop):
    from mitsuba.core import load_dict

    film = load_dict({
        'type': 'hdrfilm',
        'width': 3,
        'height': 2,
    })
    film.prepare([])

    # TODO: do not allow NaNs from film
    if develop:
        image = ek.ravel(film.develop())
        assert ek.all((image == 0) | ek.isnan(image))
    else:
        import numpy as np
        image = np.array(film.bitmap())
        assert np.all((image == 0) | np.isnan(image))
