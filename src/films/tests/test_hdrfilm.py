import pytest
import numpy as np
import drjit as dr
import mitsuba as mi


def test01_construct(variant_scalar_rgb):
    # With default reconstruction filter
    film = mi.load_dict({'type': 'hdrfilm'})
    assert film is not None
    assert film.rfilter() is not None

    # With a provided reconstruction filter
    film = mi.load_dict({
        'type': 'hdrfilm',
        'filter': {
            'type': 'gaussian',
            'stddev': 18.5
        }
    })
    assert film is not None
    assert film.rfilter().radius() == (4 * 18.5)

    # Certain parameter values are not allowed
    with pytest.raises(RuntimeError):
        mi.load_dict({
            'type': 'hdrfilm',
            'component_format': "uint8",
        })
    with pytest.raises(RuntimeError):
        mi.load_dict({
            'type': 'hdrfilm',
            'pixel_format': "brga",
        })


def test02_crops(variant_scalar_rgb):
    film = mi.load_dict({
        'type': 'hdrfilm',
        'width': 32,
        'height': 21,
        'crop_width': 11,
        'crop_height': 5,
        'crop_offset_x': 2,
        'crop_offset_y': 3,
        'sample_border': True,
        'pixel_format': "rgba",
    })
    assert film is not None
    assert dr.all(film.size() == [32, 21])
    assert dr.all(film.crop_size() == [11, 5])
    assert dr.all(film.crop_offset() == [2, 3])
    assert film.sample_border()

    # Crop size doesn't adjust its size, so an error should be raised if the
    # resulting crop window goes out of bounds.
    incomplete = """<film version="3.0.0" type="hdrfilm">
            <integer name="width" value="32"/>
            <integer name="height" value="21"/>
            <integer name="crop_offset_x" value="30"/>
            <integer name="crop_offset_y" value="20"/>"""
    with pytest.raises(RuntimeError):
        film = mi.load_string(incomplete + "</film>")
    film = mi.load_string(incomplete + """
            <integer name="crop_width" value="2"/>
            <integer name="crop_height" value="1"/>
        </film>""")
    assert film is not None
    assert dr.all(film.size() == [32, 21])
    assert dr.all(film.crop_size() == [2, 1])
    assert dr.all(film.crop_offset() == [30, 20])


@pytest.mark.parametrize('file_format', ['exr', 'rgbe', 'pfm'])
def test03_bitmap(variant_scalar_rgb, file_format, tmpdir):
    """
    Create a test image. Develop it to a few file format, each time reading
    it back and checking that contents are unchanged.
    """
    import numpy as np

    rng = np.random.default_rng(seed=12345 + ord(file_format[0]))
    # Note: depending on the file format, the alpha channel may be automatically removed.
    film = mi.load_dict({
        'type': 'hdrfilm',
        'width': 41,
        'height': 37,
        'file_format': file_format,
        'pixel_format': "rgba",
        'component_format': "float32",
        'filter': {'type': 'box'}
    })
    # Regardless of the output file format, values are stored as RGBAW (5 channels).
    contents = rng.uniform(size=(film.size()[1], film.size()[0], 5))
    # RGBE and will only reconstruct well images that have similar scales on
    # all channel (because exponent is shared between channels).
    if file_format == "rgbe":
        contents = 1 + 0.1 * contents
    # Use unit weights.
    contents[:, :, 4] = 1.0

    block = mi.ImageBlock(film.size(), [0, 0], 5, film.rfilter())
    for x in range(film.size()[1]):
        for y in range(film.size()[0]):
            block.put([y+0.5, x+0.5], contents[x, y, :])

    film.prepare([])
    film.put_block(block)

    filename = str(tmpdir.join('test_image.' + file_format))
    film.write(filename)

    # Read back and check contents
    other = mi.Bitmap(filename).convert(mi.Bitmap.PixelFormat.RGBAW, mi.Struct.Type.Float32, srgb_gamma=False)
    img   = mi.TensorXf(other)

    if False:
        import matplotlib.pyplot as plt
        plt.figure()
        plt.subplot(1, 3, 1)
        plt.imshow(contents[:, :, :3])
        plt.subplot(1, 3, 2)
        plt.imshow(img[:, :, :3])
        plt.subplot(1, 3, 3)
        plt.imshow(dr.sum(dr.abs(img[:, :, :3] - contents[:, :, :3]), axis=2), cmap='coolwarm')
        plt.colorbar()
        plt.show()

    if file_format == "exr":
        assert dr.allclose(img, contents, atol=1e-5)
    else:
        if file_format == "rgbe":
            assert dr.allclose(img[:, :, :3], contents[:, :, :3], atol=1e-2), \
                   '\n{}\nvs\n{}\n'.format(img[:4, :4, :3], contents[:4, :4, :3])
        else:
            assert dr.allclose(img[:, :, :3], contents[:, :, :3], atol=1e-5)
        # Alpha channel was ignored, alpha and weights should default to 1.0.
        assert dr.allclose(img[:, :, 3:5], 1.0, atol=1e-6)


@pytest.mark.parametrize('pixel_format', ['RGB', 'RGBA', 'XYZ', 'XYZA', 'luminance', 'luminance_alpha'])
@pytest.mark.parametrize('has_aovs', [False, True])
def test04_develop_and_bitmap(variants_all_rgb, pixel_format, has_aovs):
    aovs_channels = ['aov.r', 'aov.g', 'aov.b'] if has_aovs else []
    if pixel_format == 'luminance':
        output_channels = ['Y'] + aovs_channels
    elif pixel_format == 'luminance_alpha':
        output_channels = ['Y', 'A'] + aovs_channels
    else:
        output_channels = list(pixel_format) + aovs_channels

    film = mi.load_dict({
        'type': 'hdrfilm',
        'pixel_format': pixel_format,
        'component_format': 'float32',
        'width': 3,
        'height': 5,
        # 'rfilter': { 'type': 'box' }
    })

    has_alpha = pixel_format.endswith('A') or pixel_format.endswith('alpha')

    res = film.size()
    block = mi.ImageBlock(res, [0, 0], (5 if has_alpha else 4) + len(aovs_channels), film.rfilter())

    if dr.is_jit_v(mi.Float):
        pixel_idx = dr.arange(mi.UInt32, dr.prod(res))
        x = pixel_idx % res[0]
        y = pixel_idx // res[0]

        pos = mi.Point2f(x, y) + 0.5
        v = [x, 2 * y, 0.1, 0.5]
        if has_alpha:
            v += [1.0]
        if has_aovs:
            v += [10 + x, 20 + y, 10.1]
        block.put(pos, v)
    else:
        for x in range(res[1]):
            for y in range(res[0]):
                v = [x, 2 * y, 0.1, 0.5]
                if has_alpha:
                    v += [1.0]
                if has_aovs:
                    v += [10 + x, 20 + y, 10.1]
                block.put([y + 0.5, x + 0.5], v)

    film.prepare(aovs_channels)
    film.put_block(block)

    image = film.develop()

    assert dr.prod(image.shape) == dr.prod(res) * (len(output_channels))

    data_bitmap = mi.Bitmap(image, mi.Bitmap.PixelFormat.MultiChannel, output_channels)
    bitmap = film.bitmap()
    assert dr.allclose(mi.TensorXf(bitmap), mi.TensorXf(data_bitmap))


def test05_without_prepare(variant_scalar_rgb):
    film = mi.load_dict({
        'type': 'hdrfilm',
        'width': 3,
        'height': 2,
    })

    with pytest.raises(RuntimeError, match=r'prepare\(\)'):
        _ = film.develop()


@pytest.mark.parametrize('develop', [False, True])
def test06_empty_film(variants_all_rgb, develop):
    film = mi.load_dict({
        'type': 'hdrfilm',
        'width': 3,
        'height': 2,
    })
    film.prepare([])

    if develop:
        image = dr.ravel(film.develop())
        assert dr.all((image == 0) | dr.isnan(image))
    else:
        image = mi.TensorXf(film.bitmap())
        assert dr.all((image == 0) | dr.isnan(image), axis=None)


def test07_luminance_alpha_mono(variants_all):
    film = mi.load_dict({
        'type': 'hdrfilm',
        'width': 3,
        'height': 2,
        'pixel_format': 'luminance_alpha'
    })
    film.prepare([])

    image = mi.TensorXf(film.bitmap())

    assert image.shape[2] == 2


def register_affine():
    """Register a stage that scales and offsets every channel of the
    developed image, and return the list that records the channel names
    it was called with"""
    channels = []

    class Affine(mi.PostProcess):
        def __init__(self, props):
            super().__init__(props)
            self.scale = props.get('scale', 1.0)
            self.offset = props.get('offset', 0.0)

        def eval(self, image, image_channels):
            channels.append(image_channels)
            return mi.TensorXf(image.array * self.scale + self.offset,
                               image.shape)

    mi.register_postprocess('affine', Affine)
    return channels


def test08_postprocess(variants_all_rgb):
    channels = register_affine()

    film = mi.load_dict({
        'type': 'hdrfilm', 'width': 2, 'height': 2, 'pixel_format': 'rgba',
        'rfilter': {'type': 'box'},
        'first': {'type': 'affine', 'offset': 0.1},
        'second': {'type': 'affine', 'scale': 0.5}
    })
    film.prepare(['depth'])
    assert len(film.postprocess()) == 2
    assert film.channels() == ['R', 'G', 'B', 'A', 'depth']

    # The film is empty, hence the stages in declaration order yield (0+0.1)*0.5
    assert dr.allclose(film.develop(), 0.05)
    assert dr.allclose(mi.TensorXf(film.bitmap()), 0.05)

    # Both paths name the channels of the developed image, not the film storage
    assert channels == [['R', 'G', 'B', 'A', 'depth']] * 4

    # postprocess=False yields the linear image
    linear = film.develop(postprocess=False)
    assert linear.shape == (2, 2, 5) and dr.allclose(linear, 0)
    assert dr.allclose(mi.TensorXf(film.bitmap(postprocess=False)), 0)
    assert len(channels) == 4

    # raw=True returns the unprocessed storage, which still has a weight channel
    raw = film.develop(raw=True)
    assert raw.shape == (2, 2, 6) and dr.allclose(raw, 0)

    # External callers can run the stages on an image of their own
    image = dr.ones(mi.TensorXf, (2, 2, 5))
    assert dr.allclose(
        film.apply_postprocess(image, ['R', 'G', 'B', 'A', 'depth']), 0.55)


def test09_postprocess_output(variants_all_rgb, tmp_path):
    """Rendered images and files carry the stages, 8-bit files are sRGB-encoded"""
    register_affine()

    scene = mi.load_dict({
        'type': 'scene',
        'integrator': {'type': 'path', 'max_depth': 2},
        'emitter': {'type': 'constant', 'radiance': 0.5},
        'sensor': {
            'type': 'perspective',
            'film': {
                'type': 'hdrfilm', 'width': 3, 'height': 2,
                'pixel_format': 'rgba', 'rfilter': {'type': 'box'},
                'stage': {'type': 'affine', 'scale': 0.5}
            },
            'sampler': {'type': 'independent', 'sample_count': 4}
        }
    })
    film = scene.sensors()[0].film()

    image = mi.render(scene, spp=4)
    assert image.shape == (2, 3, 4)
    assert dr.allclose(image[..., :3], 0.25)
    assert dr.allclose(film.develop(postprocess=False)[..., :3], 0.5)

    # Storage formats record the developed values as they are
    exr = str(tmp_path / 'out.exr')
    film.write(exr)
    assert np.allclose(np.array(mi.Bitmap(exr))[..., :3], 0.25, atol=1e-3)
    film.write(exr, postprocess=False)
    assert np.allclose(np.array(mi.Bitmap(exr))[..., :3], 0.5, atol=1e-3)

    # 8-bit formats receive the sRGB transfer function, JPEG files lose alpha
    png = str(tmp_path / 'out.png')
    film.write(png)
    bitmap = mi.Bitmap(png)
    assert bitmap.pixel_format() == mi.Bitmap.PixelFormat.RGBA
    assert bitmap.component_format() == mi.Struct.Type.UInt8
    decoded = bitmap.convert(mi.Bitmap.PixelFormat.RGB, mi.Struct.Type.Float32, False)
    assert np.allclose(np.array(decoded), 0.25, atol=1e-2)

    jpg = str(tmp_path / 'out.jpg')
    film.write(jpg)
    assert mi.Bitmap(jpg).pixel_format() == mi.Bitmap.PixelFormat.RGB


def test10_postprocess_ad(variants_all_ad_rgb):
    """Derivatives propagate through the stages, and a stage that detaches the image is rejected"""
    class Square(mi.PostProcess):
        def __init__(self, props):
            super().__init__(props)

        def eval(self, image, channels):
            return mi.TensorXf(dr.square(image.array), image.shape)

    class Detach(mi.PostProcess):
        def __init__(self, props):
            super().__init__(props)

        def eval(self, image, channels):
            return mi.TensorXf(dr.detach(image.array), image.shape)

    mi.register_postprocess('square', Square)
    mi.register_postprocess('detach', Detach)

    def make_film(stage):
        film = mi.load_dict({
            'type': 'hdrfilm', 'width': 2, 'height': 2,
            'rfilter': {'type': 'box'}, 'stage': {'type': stage}
        })
        film.prepare([])
        return film

    image = dr.full(mi.TensorXf, 3.0, (2, 2, 3))
    dr.enable_grad(image)
    result = make_film('square').apply_postprocess(image, ['R', 'G', 'B'])
    dr.backward(dr.sum(result.array))
    assert dr.allclose(dr.grad(image), 6.0)

    with pytest.raises(RuntimeError, match='severed'):
        make_film('detach').apply_postprocess(image, ['R', 'G', 'B'])
