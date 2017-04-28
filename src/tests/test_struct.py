from mitsuba.core import Struct, StructConverter
import struct
import numpy as np
import pytest
import itertools


def check_conversion(conv, srcFmt, dstFmt, dataIn, dataOut=None):
    srcData = struct.pack(srcFmt, *dataIn)
    # import binascii
    # print(binascii.hexlify(srcData).decode('utf8'))
    converted = conv.convert(srcData)
    # print(binascii.hexlify(converted).decode('utf8'))
    dstData = struct.unpack(dstFmt, converted)
    ref = dataOut if dataOut is not None else dataIn
    for i in range(len(dstData)):
        assert np.abs(dstData[i] - ref[i]) / (ref[i] + 1e-6) < 1e-6


def test01_basics():
    s = Struct()
    assert s.field_count() == 0
    assert s.alignment() == 1
    assert s.size() == 0

    s.append("float_val", Struct.EFloat32)
    assert s.field_count() == 1
    assert s.alignment() == 4
    assert s.size() == 4

    s.append("byte_val", Struct.EUInt8)
    assert s.field_count() == 2
    assert s.alignment() == 4
    assert s.size() == 8

    s.append("half_val", Struct.EFloat16)
    assert s.field_count() == 3
    assert s.alignment() == 4
    assert s.size() == 8

    assert len(s) == 3
    assert s[0].name == 'float_val'
    assert s[0].offset == 0
    assert s[0].size == 4
    assert s[0].type == Struct.EFloat32

    assert s[1].name == 'byte_val'
    assert s[1].offset == 4
    assert s[1].size == 1
    assert s[1].type == Struct.EUInt8

    assert s[2].name == 'half_val'
    assert s[2].offset == 6
    assert s[2].size == 2
    assert s[2].type == Struct.EFloat16


@pytest.mark.parametrize('param', [
    ("h", Struct.EInt16),
    ("i", Struct.EInt32),
    ("q", Struct.EInt64),
    ("f", Struct.EFloat32),
    ("d", Struct.EFloat64)]
)
def test02_passthrough(param):
    s = Struct().append("val", param[1])
    ss = StructConverter(s, s)
    check_conversion(ss, "<" + param[0] + param[0],
                         "<" + param[0] + param[0],
                         (100, -101))


@pytest.mark.parametrize('param', itertools.product([
    ("b", Struct.EInt8),
    ("h", Struct.EInt16),
    ("i", Struct.EInt32),
    ("q", Struct.EInt64),
    ("f", Struct.EFloat32),
    ("d", Struct.EFloat64)], repeat=2))
def test03_convert1(param):
    p1, p2 = param
    s1 = Struct().append("val", p1[1])
    s2 = Struct().append("val", p2[1])
    s = StructConverter(s1, s2)
    check_conversion(s, "<" + p1[0], "<" + p2[0], (1,))


@pytest.mark.parametrize('p', [
    ("b", Struct.EInt8),
    ("h", Struct.EInt16),
    ("i", Struct.EInt32),
    ("q", Struct.EInt64),
    ("f", Struct.EFloat32),
    ("d", Struct.EFloat64)])
def test04_convert_half(p):
        s1 = Struct().append("val", Struct.EFloat16)
        s2 = Struct().append("val", p[1])
        s = StructConverter(s1, s2)
        check_conversion(s, "<H", "<" + p[0], (15360, ), (1, ))

        s1 = Struct().append("val", p[1])
        s2 = Struct().append("val", Struct.EFloat16)
        s = StructConverter(s1, s2)
        check_conversion(s, "<" + p[0], "<H", (1, ), (15360, ))


@pytest.mark.parametrize('param', [
    ("H", Struct.EUInt16),
    ("I", Struct.EUInt32),
    ("Q", Struct.EUInt64)])
def test04_bswap(param):
        le = Struct().append("val", param[1])
        be = Struct(byte_order=Struct.EBigEndian).append(
                "val", param[1])
        s1 = StructConverter(le, be)
        s2 = StructConverter(be, le)
        check_conversion(s1, "<" + param[0] + param[0],
                         ">" + param[0] + param[0],
                         (100, 101))
        check_conversion(s2, ">" + param[0] + param[0],
                         "<" + param[0] + param[0],
                         (100, 101))


@pytest.mark.parametrize('param', [
    ("h", Struct.EInt16),
    ("i", Struct.EInt32),
    ("q", Struct.EInt64),
    ("f", Struct.EFloat32),
    ("d", Struct.EFloat64)])
def test06_bswap_signed(param):
        # LE -> BE
        le = Struct().append("val", param[1])
        be = Struct(byte_order=Struct.EBigEndian).append(
                "val", param[1])
        s1 = StructConverter(le, be)
        check_conversion(s1, "<" + param[0] + param[0],
                        ">" + param[0] + param[0],
                        (100, -101))
        # BE -> LE
        le = Struct().append("val", param[1])
        be = Struct(byte_order=Struct.EBigEndian).append(
                "val", param[1])
        s = StructConverter(be, le)
        check_conversion(s, ">" + param[0] + param[0],
                        "<" + param[0] + param[0],
                        (100, -101))


def test07_missing():
    s1 = Struct().append("val1", Struct.EInt32) \
                 .append("val3", Struct.EInt32)
    s2 = Struct().append("val1", Struct.EInt32) \
                 .append("val2", Struct.EInt32, int(Struct.EDefault), 123) \
                 .append("val3", Struct.EInt32)
    s = StructConverter(s1, s2)
    check_conversion(s, "<ii", "<iii", (1, 3), (1, 123, 3))


def test08_remapRange_1():
    s = StructConverter(
        Struct().append("v", Struct.EUInt8, int(Struct.ENormalized)),
        Struct().append("v", Struct.EFloat32)
    )
    check_conversion(s, "<BBB", "<fff", (0, 1, 255),
                     (0, np.float32(1.0 / 255.0), 1))

    s = StructConverter(
        Struct().append("v", Struct.EInt8, int(Struct.ENormalized)),
        Struct().append("v", Struct.EFloat32)
    )
    check_conversion(s, "<bbb", "<fff", (-128, 0, 127),
                     (0, np.float32(128 / 255.0), 1))


def test08_remapRange_2():
    s = StructConverter(
        Struct().append("v", Struct.EFloat32),
        Struct().append("v", Struct.EUInt8, int(Struct.ENormalized))
    )
    check_conversion(s, "<fff", "<BBB",
                     (0, np.float32(1.0 / 255.0), 1),
                     (0, 1, 255))

    s = StructConverter(
        Struct().append("v", Struct.EFloat32),
        Struct().append("v", Struct.EInt8, int(Struct.ENormalized))
    )
    check_conversion(s, "<fff", "<bbb",
                     (0, np.float32(128 / 255.0), 1),
                     (-128, 0, 127))


def test08_remapRange_3():
    s = StructConverter(
        Struct().append("v", Struct.EInt16, int(Struct.ENormalized)),
        Struct().append("v", Struct.EUInt8, int(Struct.ENormalized))
    )
    check_conversion(s, "<hhh", "<BBB",
                     (-32768, 0, 32767),
                     (0, 128, 255))

    s = StructConverter(
        Struct().append("v", Struct.EInt8, int(Struct.ENormalized)),
        Struct().append("v", Struct.EUInt16, int(Struct.ENormalized))
    )
    check_conversion(s, "<bbb", "<HHH",
                     (-128, 0, 127),
                     (0, 32896, 65535))


def test09_gamma_0():
    s = StructConverter(
        Struct().append("v", Struct.EUInt8,
                        int(Struct.ENormalized) | int(Struct.EGamma)),
        Struct().append("v", Struct.EFloat32)
    )
    check_conversion(s, "<BBB", "<fff",
                     (0, 127, 255),
                     (0, np.float32(0.212230786), 1))

    s = StructConverter(
        Struct().append("v", Struct.EUInt8,
                        int(Struct.ENormalized) | int(Struct.EGamma)),
        Struct().append("v", Struct.EFloat64)
    )
    check_conversion(s, "<BBB", "<ddd",
                     (0, 127, 255),
                     (0, np.float64(0.212230786), 1))


def test09_gamma_1():
    s = StructConverter(
        Struct().append("v", Struct.EFloat32),
        Struct().append("v", Struct.EUInt8,
                        int(Struct.ENormalized) | int(Struct.EGamma)),
    )
    check_conversion(s, "<fff", "<BBB",
                     (0, np.float32(0.212230786), 1),
                     (0, 127, 255))

    s = StructConverter(
        Struct().append("v", Struct.EFloat64),
        Struct().append("v", Struct.EUInt8,
                        int(Struct.ENormalized) | int(Struct.EGamma)),
    )
    check_conversion(s, "<ddd", "<BBB",
                     (0, np.float32(0.212230786), 1),
                     (0, 127, 255))


def test09_fail():
    s = StructConverter(
        Struct().append("v", Struct.EInt32, default=10,
                        flags=int(Struct.EAssert)),
        Struct().append("v", Struct.EInt32)
    )
    check_conversion(s, "<i", "<i", (10,), (10,))
    with pytest.raises(RuntimeError):
        check_conversion(s, "<i", "<i", (11,), (11,))


def test10_fail():
    s = StructConverter(
        Struct().append("v", Struct.EFloat32, default=10,
                        flags=int(Struct.EAssert)),
        Struct().append("v", Struct.EFloat32)
    )
    check_conversion(s, "<f", "<f", (10,), (10,))
    with pytest.raises(RuntimeError):
        check_conversion(s, "<f", "<f", (11,), (11,))


def test11_fail():
    s = StructConverter(
        Struct().append("v1", Struct.EFloat32, default=10,
                        flags=int(Struct.EAssert))
                .append("v2", Struct.EFloat32),
        Struct().append("v2", Struct.EFloat32)
    )
    check_conversion(s, "<ff", "<f", (10, 10,), (10,))
    with pytest.raises(RuntimeError):
        check_conversion(s, "<ff", "<f", (11, 11), (11,))

def test12_blend():
    src = Struct()
    src.append("a", Struct.EFloat32)
    src.append("b", Struct.EFloat32)

    target = Struct()
    target.append("v", Struct.EFloat32)
    target.field("v").blend = [(3.0, "a"), (4.0, "b")]

    s = StructConverter(
        src,
        target
    )
    check_conversion(s, "<ff", "<f",
                     (1.0, 2.0),
                     (3.0 + 8.0,))

    src = Struct()
    src.append("a", Struct.EUInt8, Struct.ENormalized)
    src.append("b", Struct.EUInt8, Struct.ENormalized)

    target = Struct()
    target.append("v", Struct.EFloat32)
    target.field("v").blend = [(3.0, "a"), (4.0, "b")]

    s = StructConverter(
        src,
        target
    )

    check_conversion(s, "<BB", "<f",
                     (255, 127),
                     (3.0 + 4.0 * (127.0/255.0),))

def test13_blend_gamma():
    def to_srgb(value):
        if value <= 0.0031308:
            return 12.92 * value
        return 1.055 * (value ** (1.0/2.4)) - 0.055

    def from_srgb(value):
        if value <= 0.04045:
            return value * 1.0 / 12.92
        return ((value + 0.055) * (1.0 / 1.055)) ** 2.4

    src = Struct()
    src.append("a", Struct.EUInt8, Struct.ENormalized | Struct.EGamma)
    src.append("b", Struct.EUInt8, Struct.ENormalized | Struct.EGamma)

    target = Struct()
    target.append("v", Struct.EUInt8, Struct.ENormalized | Struct.EGamma)
    target.field("v").blend = [(1, "a"), (1, "b")]

    s = StructConverter(src, target)
    ref = int(np.round(to_srgb(from_srgb(100/255.0) + from_srgb(200/255.0)) * 255))

    check_conversion(s, "<BB", "<B", (100, 200), (ref,))

def test14_blend_fail():
    src = Struct()
    src.append("a", Struct.EUInt8)
    src.append("b", Struct.EUInt8)

    target = Struct()
    target.append("v", Struct.EFloat32)
    target.field("v").blend = [(3.0, "a"), (4.0, "b")]

    with pytest.raises(RuntimeError):
        s = StructConverter(src, target)
        check_conversion(s, "<BB", "<f",
                         (255, 127),
                         (3.0 + 4.0 * (127.0/255.0),))

def test15_blend_fail_2():
    src = Struct(pack=True)
    src.append("a", Struct.EFloat32)
    src.append("b", Struct.EFloat64)

    target = Struct()
    target.append("v", Struct.EFloat32)
    target.field("v").blend = [(3.0, "a"), (4.0, "b")]

    with pytest.raises(RuntimeError):
        s = StructConverter(src, target)
        check_conversion(s, "<fd", "<f",
                         (1.0, 2.0),
                         (3.0 + 8.0,))
