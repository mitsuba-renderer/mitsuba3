import numpy as np
import os

import mitsuba
import pytest
import enoki as ek

mitsuba.set_variant('scalar_rgb')

from mitsuba.core import Bitmap, Struct, ReconstructionFilter, float_dtype, FilterBoundaryCondition

def find_resource(fname):
    path = os.path.dirname(os.path.realpath(__file__))
    while True:
        full = os.path.join(path, fname)
        if os.path.exists(full):
            return full
        if path == '' or path == '/':
            raise Exception("find_resource(): could not find \"%s\"" % fname)
        path = os.path.dirname(path)


def test_read_convert_yc(tmpdir):
    # Tests reading & upsampling a luminance/chroma image
    b = Bitmap(find_resource('resources/data/tests/bitmap/XYZ_YC.exr'))
    # Tests float16 XYZ -> float32 RGBA conversion
    b = b.convert(Bitmap.PixelFormat.RGBA, Struct.Type.Float32, False)
    ref = [ 0.36595437, 0.27774358, 0.11499051, 1.]
    # Tests automatic Bitmap->NumPy conversion
    assert np.allclose(np.mean(b, axis=(0, 1)), ref, atol=1e-3)
    tmp_file = os.path.join(str(tmpdir), "out.exr")
    # Tests bitmap resampling filters
    rfilter = mitsuba.core.xml.load_string("<rfilter version='2.0.0' type='box'/>")
    b = b.resample([1, 1], rfilter, (FilterBoundaryCondition.Zero, FilterBoundaryCondition.Zero))
    # Tests OpenEXR bitmap writing
    b.write(tmp_file)
    b = Bitmap(tmp_file)
    os.remove(tmp_file)
    assert np.allclose(np.mean(b, axis=(0, 1)), ref, atol=1e-3)


def test_read_write_complex_exr(tmpdir):
    # Tests reading and writing of complex multi-channel images with custom properties
    b1 = Bitmap(Bitmap.PixelFormat.MultiChannel, Struct.Type.Float32, [4, 5], 6)
    a = b1.struct_()
    for i in range(6):
        a[i].name = "my_ch_%i" % i
    b2 = np.array(b1, copy=False)
    meta = b1.metadata()
    meta["str_prop"] = "value"
    meta["int_prop"] = 15
    meta["dbl_prop"] = 30.0
    meta["vec3_prop"] = [1.0, 2.0, 3.0]
    # meta["mat_prop"] = np.arange(16, dtype=float_dtype).reshape((4, 4)) + np.eye(4, dtype=float_dtype) # TODO py::implicitly_convertible<py::array, Transform4f>() doesn't seem to work in transform_v.cpp

    assert b2.shape == (5, 4, 6)
    assert b2.dtype == np.float32
    b2[:] = np.arange(4*5*6).reshape((5, 4, 6))
    tmp_file = os.path.join(str(tmpdir), "out.exr")
    b1.write(tmp_file)

    b3 = Bitmap(tmp_file)
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


def test_convert_rgb_y(tmpdir):
    # Tests RGBA(float64) -> Y (float32) conversion
    b1 = Bitmap(Bitmap.PixelFormat.RGBA, Struct.Type.Float64, [3, 1])
    b2 = np.array(b1, copy=False)
    b2[:] = [[[1, 0, 0, 1], [0, 1, 0, 0.5], [0, 0, 1, 0]]]
    b3 = np.array(b1.convert(Bitmap.PixelFormat.Y, Struct.Type.Float32, False)).ravel()
    assert np.allclose(b3, [0.212671, 0.715160, 0.072169])


def test_convert_rgb_y_gamma(tmpdir):
    def to_srgb(value):
        if value <= 0.0031308:
            return 12.92 * value
        return 1.055 * (value ** (1.0/2.4)) - 0.055

    # Tests RGBA(float64) -> Y (uint8_t, linear) conversion
    b1 = Bitmap(Bitmap.PixelFormat.RGBA, Struct.Type.Float64, [3, 1])
    b2 = np.array(b1, copy=False)
    b2[:] = [[[1, 0, 0, 1], [0, 1, 0, 0.5], [0, 0, 1, 0]]]
    b3 = np.array(b1.convert(Bitmap.PixelFormat.Y, Struct.Type.UInt8, False)).ravel()
    assert np.allclose(b3, [0.212671*255, 0.715160*255, 0.072169*255], atol=1)

    # Tests RGBA(float64) -> Y (uint8_t, gamma) conversion
    b1 = Bitmap(Bitmap.PixelFormat.RGBA, Struct.Type.Float64, [3, 1])
    b2 = np.array(b1, copy=False)
    b2[:] = [[[1, 0, 0, 1], [0, 1, 0, 0.5], [0, 0, 1, 0]]]
    b3 = np.array(b1.convert(Bitmap.PixelFormat.Y, Struct.Type.UInt8, True)).ravel()
    assert np.allclose(b3, [to_srgb(0.212671)*255, to_srgb(0.715160)*255, to_srgb(0.072169)*255], atol=1)


def test_premultiply_alpha(tmpdir):
    # Tests RGBA(float64) -> Y (float32) conversion
    b1 = Bitmap(Bitmap.PixelFormat.RGBA, Struct.Type.Float64, [3, 1])
    assert b1.premultiplied_alpha()
    b1.set_premultiplied_alpha(False)
    assert not b1.premultiplied_alpha()

    b2 = np.array(b1, copy=False)
    b2[:] = [[[1, 0, 0, 1], [0, 1, 0, 0.5], [0, 0, 1, 0]]]

    # Premultiply
    b3 = np.array(b1.convert(Bitmap.PixelFormat.RGBA, Struct.Type.Float32, False, Bitmap.AlphaTransform.Premultiply)).ravel()
    assert np.allclose(b3, [1.0, 0.0, 0.0, 1.0, 0.0, 0.5, 0.0, 0.5, 0.0, 0.0, 0.0, 0.0])

    # Unpremultiply
    b1.set_premultiplied_alpha(True)
    b3 = np.array(b1.convert(Bitmap.PixelFormat.RGBA, Struct.Type.Float32, False, Bitmap.AlphaTransform.Unpremultiply)).ravel()
    assert np.allclose(b3, [1.0, 0.0, 0.0, 1.0, 0.0, 2, 0.0, 0.5, 0.0, 0.0, 0.0, 0.0])



def test_read_write_jpeg(tmpdir):
    np.random.seed(12345)
    tmp_file = os.path.join(str(tmpdir), "out.jpg")

    b = Bitmap(Bitmap.PixelFormat.Y, Struct.Type.UInt8, [10, 10])
    ref = np.uint8(np.random.random((10, 10))*255)
    np.array(b, copy=False)[:] = ref[..., np.newaxis]
    b.write(tmp_file, quality=50)
    b2 = Bitmap(tmp_file)
    assert np.sum(np.abs(np.float32(np.array(b2)[:, :, 0])-ref)) / (10*10*255) < 0.07

    b = Bitmap(Bitmap.PixelFormat.RGB, Struct.Type.UInt8, [10, 10])
    ref = np.uint8(np.random.random((10, 10, 3))*255)
    np.array(b, copy=False)[:] = ref
    b.write(tmp_file, quality=100)
    b2 = Bitmap(tmp_file)
    assert np.sum(np.abs(np.float32(np.array(b2))-ref)) / (3*10*10*255) < 0.2

    os.remove(tmp_file)


def test_read_write_png(tmpdir):
    np.random.seed(12345)
    tmp_file = os.path.join(str(tmpdir), "out.png")

    b = Bitmap(Bitmap.PixelFormat.Y, Struct.Type.UInt8, [10, 10])
    ref = np.uint8(np.random.random((10, 10))*255)
    np.array(b, copy=False)[:] = ref[..., np.newaxis]
    b.write(tmp_file)
    b2 = Bitmap(tmp_file)
    assert np.sum(np.abs(np.float32(np.array(b2)[:, :, 0])-ref)) == 0

    b = Bitmap(Bitmap.PixelFormat.RGBA, Struct.Type.UInt8, [10, 10])
    ref = np.uint8(np.random.random((10, 10, 4))*255)
    np.array(b, copy=False)[:] = ref
    b.write(tmp_file)
    b2 = Bitmap(tmp_file)
    assert np.sum(np.abs(np.float32(np.array(b2))-ref)) == 0

    os.remove(tmp_file)


def test_read_write_hdr(tmpdir):
    np.random.seed(12345)

    b = Bitmap(Bitmap.PixelFormat.RGB, Struct.Type.Float32, [10, 20])
    ref = np.float32(np.random.random((20, 10, 3)))
    np.array(b, copy=False)[:] = ref[...]
    tmp_file = os.path.join(str(tmpdir), "out.hdr")
    b.write(tmp_file)
    b2 = Bitmap(tmp_file)
    assert np.abs(np.mean(np.array(b2)-ref)) < 1e-2
    os.remove(tmp_file)


def test_read_write_pfm(tmpdir):
    np.random.seed(12345)

    b = Bitmap(Bitmap.PixelFormat.RGB, Struct.Type.Float32, [10, 20])
    ref = np.float32(np.random.random((20, 10, 3)))
    np.array(b, copy=False)[:] = ref[...]
    tmp_file = os.path.join(str(tmpdir), "out.pfm")
    b.write(tmp_file)
    b2 = Bitmap(tmp_file)
    assert np.abs(np.mean(np.array(b2)-ref)) == 0
    os.remove(tmp_file)


def test_read_write_ppm(tmpdir):
    np.random.seed(12345)

    b = Bitmap(Bitmap.PixelFormat.RGB, Struct.Type.UInt8, [10, 20])
    ref = np.uint8(np.random.random((20, 10, 3))*255)
    np.array(b, copy=False)[:] = ref[...]
    tmp_file = os.path.join(str(tmpdir), "out.ppm")
    b.write(tmp_file)
    b2 = Bitmap(tmp_file)
    assert np.abs(np.mean(np.array(b2)-ref)) == 0
    os.remove(tmp_file)


def test_read_bmp():
    b = Bitmap(find_resource('resources/data/common/textures/flower.bmp'))
    ref = [ 136.50910448, 134.07641791,  85.67253731 ]
    assert np.allclose(np.mean(b, axis=(0, 1)), ref)


def test_read_tga():
    b1 = Bitmap(find_resource('resources/data/tests/bitmap/tga_uncompressed.tga'))
    b2 = Bitmap(find_resource('resources/data/tests/bitmap/tga_compressed.tga'))
    assert b1 == b2


def test_accumulate():
    # ----- Accumulate the whole bitmap
    b1 = Bitmap(Bitmap.PixelFormat.RGB, Struct.Type.UInt8, [10, 10])
    n = b1.height() * b1.width()
    # 0, 1, 2, ..., 99
    np.array(b1, copy=False)[:] = np.arange(n).reshape(b1.height(), b1.width(), 1)

    b2 = Bitmap(Bitmap.PixelFormat.RGB, Struct.Type.UInt8, [10, 10])
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
