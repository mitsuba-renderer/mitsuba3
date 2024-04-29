import numpy as np
import os

import pytest
import drjit as dr
import mitsuba as mi

from mitsuba.test.util import find_resource


def test_read_convert_yc(variant_scalar_rgb, tmpdir):
    # Tests reading & upsampling a luminance/chroma image
    b = mi.Bitmap(find_resource('resources/data/tests/bitmap/XYZ_YC.exr'))
    # Tests float16 XYZ -> float32 RGBA conversion
    b = b.convert(mi.Bitmap.PixelFormat.RGBA, mi.Struct.Type.Float32, False)
    ref = [ 0.36595437, 0.27774358, 0.11499051, 1.]
    # Tests automatic mi.Bitmap->NumPy conversion
    assert np.allclose(np.mean(b, axis=(0, 1)), ref, atol=1e-3)
    tmp_file = os.path.join(str(tmpdir), "out.exr")
    # Tests bitmap resampling filters
    rfilter = mi.load_dict({'type': 'box'})
    b = b.resample([1, 1], rfilter, (mi.FilterBoundaryCondition.Zero, mi.FilterBoundaryCondition.Zero))
    # Tests OpenEXR bitmap writing
    b.write(tmp_file)
    b = mi.Bitmap(tmp_file)
    os.remove(tmp_file)
    assert np.allclose(np.mean(b, axis=(0, 1)), ref, atol=1e-3)


def test_read_write_complex_exr(variant_scalar_rgb, tmpdir):
    # Tests reading and writing of complex multi-channel images with custom properties
    b1 = mi.Bitmap(mi.Bitmap.PixelFormat.MultiChannel, mi.Struct.Type.Float32, [4, 5], 6)
    a = b1.struct_()
    for i in range(6):
        a[i].name = "my_ch_%i" % i
    b2 = np.array(b1, copy=False)
    meta = b1.metadata()
    meta["str_prop"] = "value"
    meta["int_prop"] = 15
    meta["dbl_prop"] = 30.0
    meta["vec3_prop"] = [1.0, 2.0, 3.0]

    # TODO py::implicitly_convertible<py::array, Transform4f>() doesn't seem to work in transform_v.cpp
    # meta["mat_prop"] = np.arange(16, dtype=mi.float_dtype).reshape((4, 4)) + np.eye(4, dtype=mi.float_dtype)

    assert b2.shape == (5, 4, 6)
    assert b2.dtype == np.float32
    b2[:] = np.arange(4*5*6).reshape((5, 4, 6))
    tmp_file = os.path.join(str(tmpdir), "out.exr")
    b1.write(tmp_file)

    b3 = mi.Bitmap(tmp_file)
    os.remove(tmp_file)
    meta = b3.metadata()
    meta.remove_property("generatedBy")
    meta.remove_property("pixelAspectRatio")
    meta.remove_property("screenWindowWidth")
    assert b3 == b1
    b2[0, 0, 0] = 3
    assert b3 != b1
    b2[0, 0, 0] = 0
    assert b3 == b1
    assert str(b3) == str(b1)
    meta["str_prop"] = "value2"
    assert b3 != b1
    assert str(b3) != str(b1)


def test_convert_rgb_y(variant_scalar_rgb, tmpdir):
    # Tests RGBA(float64) -> Y (float32) conversion
    b1 = mi.Bitmap(mi.Bitmap.PixelFormat.RGBA, mi.Struct.Type.Float64, [3, 1])
    b2 = np.array(b1, copy=False)
    b2[:] = [[[1, 0, 0, 1], [0, 1, 0, 0.5], [0, 0, 1, 0]]]
    b3 = np.array(b1.convert(mi.Bitmap.PixelFormat.Y, mi.Struct.Type.Float32, False)).ravel()
    assert np.allclose(b3, [0.212671, 0.715160, 0.072169])


def test_convert_rgb_y_gamma(variant_scalar_rgb, tmpdir):
    def to_srgb(value):
        if value <= 0.0031308:
            return 12.92 * value
        return 1.055 * (value ** (1.0/2.4)) - 0.055

    # Tests RGBA(float64) -> Y (uint8_t, linear) conversion
    b1 = mi.Bitmap(mi.Bitmap.PixelFormat.RGBA, mi.Struct.Type.Float64, [3, 1])
    b2 = np.array(b1, copy=False)
    b2[:] = [[[1, 0, 0, 1], [0, 1, 0, 0.5], [0, 0, 1, 0]]]
    b3 = np.array(b1.convert(mi.Bitmap.PixelFormat.Y, mi.Struct.Type.UInt8, False)).ravel()
    assert np.allclose(b3, [0.212671*255, 0.715160*255, 0.072169*255], atol=1)

    # Tests RGBA(float64) -> Y (uint8_t, gamma) conversion
    b1 = mi.Bitmap(mi.Bitmap.PixelFormat.RGBA, mi.Struct.Type.Float64, [3, 1])
    b2 = np.array(b1, copy=False)
    b2[:] = [[[1, 0, 0, 1], [0, 1, 0, 0.5], [0, 0, 1, 0]]]
    b3 = np.array(b1.convert(mi.Bitmap.PixelFormat.Y, mi.Struct.Type.UInt8, True)).ravel()
    assert np.allclose(b3, [to_srgb(0.212671)*255, to_srgb(0.715160)*255, to_srgb(0.072169)*255], atol=1)


def test_convert_optional_args(variant_scalar_rgb):
    b = mi.Bitmap(mi.Bitmap.PixelFormat.RGBA, mi.Struct.Type.Float64, [3, 1])
    assert not b.srgb_gamma()

    b1 = b.convert(srgb_gamma=True)
    assert b1.srgb_gamma()
    assert b1.pixel_format() == b.pixel_format()
    assert b1.component_format() == b.component_format()

    b2 = b.convert(pixel_format=mi.Bitmap.PixelFormat.RGB)
    assert b2.srgb_gamma() == b.srgb_gamma()
    assert b2.pixel_format() == mi.Bitmap.PixelFormat.RGB
    assert b2.component_format() == b.component_format()

    b3 = b.convert(component_format=mi.Struct.Type.UInt8)
    assert b2.srgb_gamma() == b.srgb_gamma()
    assert b1.pixel_format() == b.pixel_format()
    assert b3.component_format() == mi.Struct.Type.UInt8


def test_premultiply_alpha(variant_scalar_rgb, tmpdir):
    # Tests RGBA(float64) -> Y (float32) conversion
    b1 = mi.Bitmap(mi.Bitmap.PixelFormat.RGBA, mi.Struct.Type.Float64, [3, 1])
    assert b1.premultiplied_alpha()
    b1.set_premultiplied_alpha(False)
    assert not b1.premultiplied_alpha()

    b2 = np.array(b1, copy=False)
    b2[:] = [[[1, 0, 0, 1], [0, 1, 0, 0.5], [0, 0, 1, 0]]]

    # Premultiply
    b3 = np.array(b1.convert(mi.Bitmap.PixelFormat.RGBA, mi.Struct.Type.Float32, False, mi.Bitmap.AlphaTransform.Premultiply)).ravel()
    assert np.allclose(b3, [1.0, 0.0, 0.0, 1.0, 0.0, 0.5, 0.0, 0.5, 0.0, 0.0, 0.0, 0.0])

    # Unpremultiply
    b1.set_premultiplied_alpha(True)
    b3 = np.array(b1.convert(mi.Bitmap.PixelFormat.RGBA, mi.Struct.Type.Float32, False, mi.Bitmap.AlphaTransform.Unpremultiply)).ravel()
    assert np.allclose(b3, [1.0, 0.0, 0.0, 1.0, 0.0, 2, 0.0, 0.5, 0.0, 0.0, 0.0, 0.0])


def test_read_write_jpeg(variant_scalar_rgb, tmpdir, np_rng):
    tmp_file = os.path.join(str(tmpdir), "out.jpg")

    b = mi.Bitmap(mi.Bitmap.PixelFormat.Y, mi.Struct.Type.UInt8, [10, 10])
    ref = np.uint8(np_rng.random((10, 10))*255)
    np.array(b, copy=False)[:] = ref[...]
    b.write(tmp_file, quality=50)
    b2 = mi.Bitmap(tmp_file)
    assert np.sum(np.abs(np.float32(np.array(b2)[:, :])-ref)) / (10*10*255) < 0.07

    b = mi.Bitmap(mi.Bitmap.PixelFormat.RGB, mi.Struct.Type.UInt8, [10, 10])
    ref = np.uint8(np_rng.random((10, 10, 3))*255)
    np.array(b, copy=False)[:] = ref
    b.write(tmp_file, quality=100)
    b2 = mi.Bitmap(tmp_file)
    assert np.sum(np.abs(np.float32(np.array(b2))-ref)) / (3*10*10*255) < 0.2

    os.remove(tmp_file)


def test_read_write_png(variant_scalar_rgb, tmpdir, np_rng):
    tmp_file = os.path.join(str(tmpdir), "out.png")

    b = mi.Bitmap(mi.Bitmap.PixelFormat.Y, mi.Struct.Type.UInt8, [10, 10])
    ref = np.uint8(np_rng.random((10, 10))*255)
    np.array(b, copy=False)[:] = ref[...]
    b.write(tmp_file)
    b2 = mi.Bitmap(tmp_file)
    assert np.sum(np.abs(np.float32(np.array(b2)[:, :])-ref)) == 0

    b = mi.Bitmap(mi.Bitmap.PixelFormat.RGBA, mi.Struct.Type.UInt8, [10, 10])
    ref = np.uint8(np_rng.random((10, 10, 4))*255)
    np.array(b, copy=False)[:] = ref
    b.write(tmp_file)
    b2 = mi.Bitmap(tmp_file)
    assert np.sum(np.abs(np.float32(np.array(b2))-ref)) == 0

    os.remove(tmp_file)


def test_read_write_hdr(variant_scalar_rgb, tmpdir, np_rng):
    b = mi.Bitmap(mi.Bitmap.PixelFormat.RGB, mi.Struct.Type.Float32, [10, 20])
    ref = np.float32(np_rng.random((20, 10, 3)))
    np.array(b, copy=False)[:] = ref[...]
    tmp_file = os.path.join(str(tmpdir), "out.hdr")
    b.write(tmp_file)
    b2 = mi.Bitmap(tmp_file)
    assert np.abs(np.mean(np.array(b2)-ref)) < 1e-2
    os.remove(tmp_file)


def test_read_write_pfm(variant_scalar_rgb, tmpdir, np_rng):
    b = mi.Bitmap(mi.Bitmap.PixelFormat.RGB, mi.Struct.Type.Float32, [10, 20])
    ref = np.float32(np_rng.random((20, 10, 3)))
    np.array(b, copy=False)[:] = ref[...]
    tmp_file = os.path.join(str(tmpdir), "out.pfm")
    b.write(tmp_file)
    b2 = mi.Bitmap(tmp_file)
    assert np.abs(np.mean(np.array(b2)-ref)) == 0
    os.remove(tmp_file)


def test_read_write_ppm(variant_scalar_rgb, tmpdir, np_rng):
    b = mi.Bitmap(mi.Bitmap.PixelFormat.RGB, mi.Struct.Type.UInt8, [10, 20])
    ref = np.uint8(np_rng.random((20, 10, 3))*255)
    np.array(b, copy=False)[:] = ref[...]
    tmp_file = os.path.join(str(tmpdir), "out.ppm")
    b.write(tmp_file)
    b2 = mi.Bitmap(tmp_file)
    assert np.abs(np.mean(np.array(b2)-ref)) == 0
    os.remove(tmp_file)


def test_read_bmp(variant_scalar_rgb):
    b = mi.Bitmap(find_resource('resources/data/common/textures/flower.bmp'))
    ref = [ 136.50910448, 134.07641791,  85.67253731 ]
    assert np.allclose(np.mean(b, axis=(0, 1)), ref)


def test_read_tga(variant_scalar_rgb):
    b1 = mi.Bitmap(find_resource('resources/data/tests/bitmap/tga_uncompressed.tga'))
    b2 = mi.Bitmap(find_resource('resources/data/tests/bitmap/tga_compressed.tga'))
    assert b1 == b2


@pytest.mark.parametrize('file_format',
                         [mi.Bitmap.FileFormat.JPEG,
                          mi.Bitmap.FileFormat.PPM,
                          mi.Bitmap.FileFormat.PNG])
def test_read_write_memorystream_uint8(variant_scalar_rgb, np_rng, file_format):
    ref = np.uint8(np_rng.random((10, 10, 3)) * 255)
    b = mi.Bitmap(ref)
    stream = mi.MemoryStream()
    b.write(stream, file_format)
    stream.seek(0)
    b2 = mi.Bitmap(stream)
    assert np.sum(np.abs(np.float32(np.array(b2))-ref)) / (3*10*10*255) < 0.2


@pytest.mark.parametrize('file_format',
                         [mi.Bitmap.FileFormat.OpenEXR,
                          mi.Bitmap.FileFormat.RGBE,
                          mi.Bitmap.FileFormat.PFM])
def test_read_write_memorystream_float32(variant_scalar_rgb, np_rng, file_format):
    ref = np.float32(np_rng.random((10, 10, 3)))
    b = mi.Bitmap(ref)
    stream = mi.MemoryStream()
    b.write(stream, file_format)
    stream.seek(0)
    b2 = mi.Bitmap(stream)
    assert np.abs(np.mean(np.array(b2)-ref)) < 1e-2


def test_accumulate(variant_scalar_rgb):
    # ----- Accumulate the whole bitmap
    b1 = mi.Bitmap(mi.Bitmap.PixelFormat.RGB, mi.Struct.Type.UInt8, [10, 10])
    n = b1.height() * b1.width()
    # 0, 1, 2, ..., 99
    np.array(b1, copy=False)[:] = np.arange(n).reshape(b1.height(), b1.width(), 1)

    b2 = mi.Bitmap(mi.Bitmap.PixelFormat.RGB, mi.Struct.Type.UInt8, [10, 10])
    # 100, 99, ..., 1
    np.array(b2, copy=False)[:] = np.arange(n, 0, -1).reshape(
        b2.height(), b2.width(), 1)

    b1.accumulate(b2)
    assert np.all(np.array(b1, copy=False) == n)

    # ----- Accumulate only a sub-frame
    np.array(b1, copy=False)[:] = np.arange(n).reshape(b1.height(), b1.width(), 1)
    ref = np.array(b1)
    ref[1:6, 3:4, :] += np.array(b2, copy=False)[3:8, 5:6, :]

    # Recall that positions are specified as (x, y) in Mitsuba convention,
    # but (row, column) in arrays.
    b1.accumulate(b2, [5, 3], [3, 1], [1, 5])
    assert np.all(np.array(b1, copy=False) == ref)


def test_split(variant_scalar_rgb):
    channels = ["R", "G", "B", "A", "W", "im.X", "im.Y", "im.Z", "im.A", "depth.T", "val.U", "val.V", "test.Y", "test.R", "test.M", "multi.X", "multi.B", "multi.A", "lum.Y", "lum.A"]
    bmp = mi.Bitmap(
        mi.Bitmap.PixelFormat.MultiChannel,
        mi.Struct.Type.Float32,
        [10, 10],
        len(channels),
        channels
    )
    splits = bmp.split()

    fields = {
        "<root>": set(["R", "G", "B", "A", "W"]),
        "im": set(["X", "Y", "Z", "A"]),
        "depth": set("T"),
        "val": set(["U", "V"]),
        "test": set(["M", "R", "Y"]),
        "multi": set(["X", "B", "A"]),
        "lum": set(["Y", "A"])
    }
    #print(splits)

    assert len(splits) == len(fields.keys())
    assert set([split[0] for split in splits]) == set(fields.keys())

    for split in splits:
        assert set([f.name for f in split[1].struct_()]) == fields[split[0]]

        if split[0] == "<root>":
            assert split[1].pixel_format() == mi.Bitmap.PixelFormat.RGBAW
        elif split[0] == "im":
            assert split[1].pixel_format() == mi.Bitmap.PixelFormat.XYZA
        elif split[0] == "lum":
            assert split[1].pixel_format() == mi.Bitmap.PixelFormat.YA
        else:
            assert split[1].pixel_format() == mi.Bitmap.PixelFormat.MultiChannel

        for f in split[1].struct_():
            if f.name == "A" and split[1].pixel_format() != mi.Bitmap.PixelFormat.MultiChannel:
                assert f.flags == 64
            elif f.name == "W":
                assert f.flags == 16
            else:
                assert f.flags == 32


def test_split_data(variant_scalar_rgb):
    b_multi = mi.Bitmap(find_resource('resources/data/tests/bitmap/spot_multi.exr'))
    b_0 = mi.Bitmap(find_resource('resources/data/tests/bitmap/spot_0.exr'))
    b_1 = mi.Bitmap(find_resource('resources/data/tests/bitmap/spot_1.exr'))
    b_2 = mi.Bitmap(find_resource('resources/data/tests/bitmap/spot_2.exr'))

    split = b_multi.split()

    assert np.allclose(np.array(split[0][1]), np.array(b_0))
    assert np.allclose(np.array(split[1][1]), np.array(b_1))
    assert np.allclose(np.array(split[2][1]), np.array(b_2))


def test_construct_from_array(variants_all_rgb):
    b = mi.Bitmap(find_resource('resources/data/tests/bitmap/spot_0.exr'))

    b_np = np.array(b)
    b_t = mi.TensorXf(b_np)

    assert dr.allclose(np.ravel(b_np), b_t.array.numpy())

    b1 = mi.Bitmap(b_np)
    b2 = mi.Bitmap(b_t)

    assert dr.allclose(b_np, np.array(b1))
    assert dr.allclose(b_np, np.array(b2))


def test_construct_from_non_contiguous_array(variants_all_rgb):
    flat_arr = np.array([
        [[0, 1], [3, 4], [6,  7]],
        [[1, 2], [4, 5], [7,  8]],
        [[2, 3], [5, 6], [8,  9]],
        [[3, 4], [6, 7], [9,  10]],
    ], dtype=np.float32)

    for i in range(2):
        strided = np.flip(flat_arr, axis=i)

        # Test assumes that `np.flip` will create a strided array
        assert strided.strides[i] < 0

        b = mi.Bitmap(strided)
        assert dr.allclose(strided, np.array(b))


def test_construct_from_int8_array(variants_all_rgb):
    # test uint8
    b_np = np.reshape(np.arange(16), (4, 4, 1)).astype(np.uint8)
    b1 = mi.Bitmap(b_np)
    assert dr.allclose(b_np, np.array(b1))

    # test int8
    b_np = b_np.astype(np.int8) - 5
    b2 = mi.Bitmap(b_np)
    assert dr.allclose(b_np, np.array(b2))


def test_resample(variants_all):
    """
    Resampling a mi.Bitmap should work even in non-scalar and non-RGB variants.
    """
    b = mi.Bitmap(find_resource('resources/data/tests/bitmap/XYZ_YC.exr'))

    b1 = b.resample([4, 6])
    assert b1.width() == 4 and b1.height() == 6

    b2 = b.resample([6, 7], None, (mi.FilterBoundaryCondition.Zero, mi.FilterBoundaryCondition.Zero), (-1, 1))
    assert b2.width() == 6 and b2.height() == 7

    b.set_srgb_gamma(True)
    b.set_premultiplied_alpha(True)

    b3 = b.resample([4, 6])
    assert b3.srgb_gamma()
    assert b3.premultiplied_alpha()


def test_check_weight_division(variant_scalar_rgb):
    b = mi.Bitmap(
        pixel_format=mi.Bitmap.PixelFormat.RGBAW,
        component_format=mi.Struct.Type.Float32,
        size=(1, 3)
    )
    b.clear()

    x = np.array(
        b, copy=False
    )

    x[0, 0, :] = (2, 0, 0, 0, 1)
    x[1, 0, :] = (2, 0, 0, 0, 2)
    x[2, 0, :] = (2, 0, 0, 0, 0)

    b = b.convert(
        pixel_format=mi.Bitmap.PixelFormat.RGBA,
        component_format=mi.Struct.Type.Float32,
        srgb_gamma=False
    )

    x = np.array(
        b, copy=False
    )

    assert np.all(x[0, 0, :] == (2, 0, 0, 0))
    assert np.all(x[1, 0, :] == (1, 0, 0, 0))
    assert np.all(x[2, 0, :] == (2, 0, 0, 0))
