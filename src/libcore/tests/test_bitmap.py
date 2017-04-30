from mitsuba.core import (Bitmap, Struct, ReconstructionFilter, float_dtype, PCG32)
from mitsuba.core.xml import load_string
import numpy as np
import os

def find_resource(fname):
    path = os.path.dirname(os.path.realpath(__file__))
    while True:
        full = os.path.join(path, fname)
        if os.path.exists(full):
            return full
        path = os.path.dirname(path)
        if path == '':
            raise Exception("find_resource(): could not find \"%s\"" % fname);


def test_read_convert_yc(tmpdir):
    # Tests reading & upsampling a luminance/chroma image
    b = Bitmap(find_resource('resources/data/tests/bitmap/XYZ_YC.exr'))
    # Tests float16 XYZ -> float32 RGBA conversion
    b = b.convert(Bitmap.ERGBA, Struct.EFloat32, False)
    ref = [ 0.36595437, 0.27774358, 0.11499051, 1.]
    # Tests automatic Bitmap->NumPy conversion
    assert np.allclose(np.mean(b, axis=(0, 1)), ref)
    tmp_file = os.path.join(str(tmpdir), "out.exr")
    # Tests bitmap resampling filters
    rfilter = load_string("<rfilter version='2.0.0' type='box'/>")
    b = b.resample([1, 1], rfilter, (ReconstructionFilter.EZero, ReconstructionFilter.EZero))
    # Tests OpenEXR bitmap writing
    b.write(tmp_file)
    b = Bitmap(tmp_file)
    os.remove(tmp_file)
    assert np.allclose(np.mean(b, axis=(0, 1)), ref, atol=1e-5)

def test_read_write_complex_exr(tmpdir):
    # Tests reading and writing of complex multi-channel images with custom properties
    b1 = Bitmap(Bitmap.EMultiChannel, Struct.EFloat32, [4, 5], 6)
    a = b1.struct_()
    for i in range(6):
        a[i].name = "my_ch_%i" % i
    b2 = np.array(b1, copy=False)
    meta = b1.metadata()
    meta["str_prop"] = "value"
    meta["int_prop"] = 15
    meta["dbl_prop"] = 30.0
    meta["vec3_prop"] = np.array([1.0, 2.0, 3.0])
    meta["mat_prop"] = np.arange(16, dtype=float_dtype).reshape((4, 4)) + np.eye(4, dtype=float_dtype)

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
    b1 = Bitmap(Bitmap.ERGBA, Struct.EFloat64, [3, 1])
    b2 = np.array(b1, copy=False)
    b2[:] = [[[1, 0, 0, 1], [0, 1, 0, 0.5], [0, 0, 1, 0]]]
    b3 = np.array(b1.convert(Bitmap.EY, Struct.EFloat32, False)).ravel()
    assert np.allclose(b3, [0.212671, 0.715160, 0.072169])


def test_convert_rgb_y_gamma(tmpdir):
    def to_srgb(value):
        if value <= 0.0031308:
            return 12.92 * value
        return 1.055 * (value ** (1.0/2.4)) - 0.055

    # Tests RGBA(float64) -> Y (uint8_t, linear) conversion
    b1 = Bitmap(Bitmap.ERGBA, Struct.EFloat64, [3, 1])
    b2 = np.array(b1, copy=False)
    b2[:] = [[[1, 0, 0, 1], [0, 1, 0, 0.5], [0, 0, 1, 0]]]
    b3 = np.array(b1.convert(Bitmap.EY, Struct.EUInt8, False)).ravel()
    assert np.allclose(b3, [0.212671*255, 0.715160*255, 0.072169*255], atol=1)

    # Tests RGBA(float64) -> Y (uint8_t, gamma) conversion
    b1 = Bitmap(Bitmap.ERGBA, Struct.EFloat64, [3, 1])
    b2 = np.array(b1, copy=False)
    b2[:] = [[[1, 0, 0, 1], [0, 1, 0, 0.5], [0, 0, 1, 0]]]
    b3 = np.array(b1.convert(Bitmap.EY, Struct.EUInt8, True)).ravel()
    assert np.allclose(b3, [to_srgb(0.212671)*255, to_srgb(0.715160)*255, to_srgb(0.072169)*255], atol=1)

def test_read_write_jpeg(tmpdir):
    b = Bitmap(Bitmap.EY, Struct.EUInt8, [10, 10])
    tmp_file = os.path.join(str(tmpdir), "out.jpg")
    ref = np.uint8(PCG32().next_float(10, 10)*255)
    np.array(b, copy=False)[:] = ref[..., np.newaxis]
    b.write(tmp_file, quality=50)
    b2 = Bitmap(tmp_file)
    assert np.sum(np.abs(np.float32(np.array(b2)[:, :, 0])-ref)) / (10*10*255) < 0.07
    os.remove(tmp_file)
