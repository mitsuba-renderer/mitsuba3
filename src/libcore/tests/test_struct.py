from mitsuba.core import Struct, StructConverter
import struct
import numpy as np
import pytest
import itertools
import sys


# List of supported conversions
supported_types = [
    ('b', Struct.EInt8),
    ('B', Struct.EUInt8),
    ('h', Struct.EInt16),
    ('H', Struct.EUInt16),
    ('i', Struct.EInt32),
    ('I', Struct.EUInt32),
    ('q', Struct.EInt64),
    ('Q', Struct.EUInt64),
    ('e', Struct.EFloat16),
    ('f', Struct.EFloat32),
    ('d', Struct.EFloat64)
]


def from_srgb(x):
    if x < 0.04045:
        return x / 12.92
    else:
        return ((x + 0.055) / 1.055) ** 2.4


def to_srgb(x):
    if x < 0.0031308:
        return x * 12.92
    else:
        return 1.055 * (x ** (1.0 / 2.4)) - 0.055


def check_conversion(conv, src_fmt, dst_fmt, data_in,
                     data_out=None, err_thresh=1e-6):
    if sys.version_info < (3, 6) and ('e' in src_fmt or 'e' in dst_fmt):
        pytest.skip('half precision floats unsupported in Python < 3.6')
    src_data = struct.pack(src_fmt, *data_in)
    import binascii
    print(binascii.hexlify(src_data).decode('utf8'))
    converted = conv.convert(src_data)
    print(binascii.hexlify(converted).decode('utf8'))
    dst_data = struct.unpack(dst_fmt, converted)
    ref = data_out if data_out is not None else data_in
    print(ref)
    print(dst_data)
    for i in range(len(dst_data)):
        abs_err = float(dst_data[i]) - float(ref[i])
        print(abs_err)
        assert np.abs(abs_err / (ref[i] + 1e-6)) < err_thresh


def test01_basics():
    s = Struct()
    assert s.field_count() == 0
    assert s.alignment() == 1
    assert s.size() == 0

    s.append('float_val', Struct.EFloat32)
    assert s.field_count() == 1
    assert s.alignment() == 4
    assert s.size() == 4

    s.append('byte_val', Struct.EUInt8)
    assert s.field_count() == 2
    assert s.alignment() == 4
    assert s.size() == 8

    s.append('half_val', Struct.EFloat16)
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


@pytest.mark.parametrize('param', supported_types)
def test02_passthrough(param):
    s = Struct().append('val', param[1])
    ss = StructConverter(s, s)
    values = list(range(10))
    if Struct.is_signed(param[1]):
        values += list(range(-10, 0))
    check_conversion(ss, '@' + param[0] * len(values),
                         '@' + param[0] * len(values),
                         values)


@pytest.mark.parametrize('param', itertools.product(supported_types, repeat=2))
def test03_convert(param):
    p1, p2 = param
    s1 = Struct().append('val', p1[1])
    s2 = Struct().append('val', p2[1])
    s = StructConverter(s1, s2)
    values = list(range(10))
    if Struct.is_signed(p1[1]) and Struct.is_signed(p2[1]):
        values += list(range(-10, 0))
    max_range = int(min(Struct.range(p1[1])[1], Struct.range(p2[1])[1]))
    if max_range > 1024:
        values += list(range(1000, 1024))

    # Native -> Native
    check_conversion(s, '@' + p1[0] * len(values),
                        '@' + p2[0] * len(values), values)

    # LE -> BE
    s1 = Struct(byte_order=Struct.EBigEndian).append('val', p1[1])
    s2 = Struct(byte_order=Struct.ELittleEndian).append('val', p2[1])
    s = StructConverter(s1, s2)
    check_conversion(s, '>' + p1[0] * len(values),
                        '<' + p2[0] * len(values), values)

    # BE -> LE
    s1 = Struct(byte_order=Struct.ELittleEndian).append('val', p1[1])
    s2 = Struct(byte_order=Struct.EBigEndian).append('val', p2[1])
    s = StructConverter(s1, s2)
    check_conversion(s, '<' + p1[0] * len(values),
                        '>' + p2[0] * len(values), values)


@pytest.mark.parametrize('param', supported_types)
def test03_missing_field(param):
    s1 = Struct().append('val1', param[1]) \
                 .append('val3', param[1])
    s2 = Struct().append('val1', param[1]) \
                 .append('val2', param[1], Struct.EDefault, 123) \
                 .append('val3', param[1])
    s = StructConverter(s1, s2)

    values = list(range(10))
    if Struct.is_signed(param[1]):
        values += list(range(-10, 0))
    output = []
    for k in range(len(values) // 2):
        output += [values[k * 2], 123, values[k * 2 + 1]]

    check_conversion(s, '@' + param[0] * len(values),
                        '@' + param[0] * (len(values) + len(values) // 2),
                     values, output)


def test04_missing_field_error():
    s1 = Struct().append('val1', Struct.EUInt32)
    s2 = Struct().append('val2', Struct.EUInt32)
    with pytest.raises(RuntimeError, match='Unable to find field "val2"'):
        s = StructConverter(s1, s2)
        check_conversion(s, '@I', '@I', [1], [1])


def test05_round_and_saturation():
    s1 = Struct().append('val', Struct.EFloat32)
    s2 = Struct().append('val', Struct.EInt8)
    s = StructConverter(s1, s2)
    values = [-0.55, -0.45, 0, 0.45, 0.55, 127, 128, -127, -200]
    check_conversion(s, '@fffffffff', '@bbbbbbbbb', values,
                     [-1, 0, 0, 0, 1, 127, 127, -127, -128])


def test06_round_and_saturation_normalized():
    s1 = Struct().append('val', Struct.EFloat32)
    s2 = Struct().append('val', Struct.EInt8, Struct.ENormalized)
    s = StructConverter(s1, s2)
    f = 1.0 / 127.0
    values = [-0.55 * f, -0.45 * f, 0, 0.45 * f, 0.55 * f, 1, 2, -1, -2]
    check_conversion(s, '@fffffffff', '@bbbbbbbbb', values,
                     [-1, 0, 0, 0, 1, 127, 127, -127, -128])


@pytest.mark.parametrize('param', supported_types)
def test07_roundtrip_normalization(param):
    s1 = Struct().append('val', param[1], Struct.ENormalized)
    s2 = Struct().append('val', Struct.EFloat32)
    s = StructConverter(s1, s2)
    max_range = 1.0
    if Struct.is_integer(param[1]):
        max_range = float(Struct.range(param[1])[1])
    values_in = list(range(10))
    values_out = [i / max_range for i in range(10)]
    check_conversion(s, '@' + (param[0] * 10), '@' + ('f' * 10),
                     values_in, values_out)

    s = StructConverter(s2, s1)
    check_conversion(s, '@' + ('f' * 10), '@' + (param[0] * 10),
                     values_out, values_in)


@pytest.mark.parametrize('param', supported_types)
def test08_roundtrip_normalization_int2int(param):
    if Struct.is_float(param[1]):
        return
    s1_type = Struct.EInt8 if Struct.is_signed(param[1]) else Struct.EUInt8
    s1_dtype = 'b' if Struct.is_signed(param[1]) else 'B'
    s1_range = Struct.range(s1_type)
    s2_range = Struct.range(param[1])
    s1 = Struct().append('val', s1_type, Struct.ENormalized)
    s2 = Struct().append('val', param[1], Struct.ENormalized)
    s = StructConverter(s1, s2)
    values_in = list(range(int(s1_range[0]), int(s1_range[1] + 1)))
    values_out = np.array(values_in, dtype=np.float64)
    values_out *= s2_range[1] / s1_range[1]
    values_out = np.rint(values_out)
    values_out = np.maximum(values_out, s2_range[0])
    values_out = np.minimum(values_out, s2_range[1])
    values_out = np.array(values_out, s2.dtype()['val'])
    check_conversion(s, '@' + (s1_dtype * len(values_in)),
                        '@' + (param[0] * len(values_in)),
                     values_in, values_out)


def test09_gamma_1():
    s = StructConverter(
        Struct().append('v', Struct.EUInt8,
                        Struct.ENormalized | Struct.EGamma),
        Struct().append('v', Struct.EFloat32)
    )

    src_data = list(range(256))
    dest_data = [from_srgb(x / 255.0) for x in src_data]

    check_conversion(s, '@' + ('B' * 256), '@' + ('f' * 256),
                     src_data, dest_data, err_thresh=1e-5)


def test10_gamma_2():
    s = StructConverter(
        Struct().append('v', Struct.EFloat32),
        Struct().append('v', Struct.EUInt8,
                        Struct.ENormalized | Struct.EGamma)
    )

    src_data = list(np.linspace(0, 1, 256))
    dest_data = [np.uint8(np.round(to_srgb(x) * 255)) for x in src_data]

    check_conversion(s, '@' + ('f' * 256), '@' + ('B' * 256),
                     src_data, dest_data)


@pytest.mark.parametrize('param', supported_types)
def test11_assert_value(param):
    s = StructConverter(
        Struct().append('v', param[1], default=10,
                        flags=int(Struct.EAssert)),
        Struct().append('v', param[1])
    )
    check_conversion(s, '@' + param[0], '@' + param[0], (10,), (10,))
    with pytest.raises(RuntimeError):
        check_conversion(s, '@' + param[0], '@' + param[0], (11,), (11,))

    s = StructConverter(
        Struct().append('v1', param[1], default=10,
                        flags=int(Struct.EAssert))
                .append('v2', param[1]),
        Struct().append('v2', param[1])
    )
    check_conversion(s, '@' + param[0] * 2,
                        '@' + param[0], (10, 10), (10,))
    with pytest.raises(RuntimeError):
        check_conversion(s, '@' + param[0] * 2,
                            '@' + param[0], (11, 11), (11,))


def test12_blend():
    src = Struct()
    src.append('a', Struct.EFloat32)
    src.append('b', Struct.EFloat32)

    target = Struct()
    target.append('v', Struct.EFloat32)
    target.field('v').blend = [(3.0, 'a'), (4.0, 'b')]

    s = StructConverter(
        src,
        target
    )
    check_conversion(s, '@ff', '@f',
                     (1.0, 2.0),
                     (3.0 + 8.0,))

    src = Struct()
    src.append('a', Struct.EUInt8, Struct.ENormalized)
    src.append('b', Struct.EUInt8, Struct.ENormalized)

    target = Struct()
    target.append('v', Struct.EFloat32)
    target.field('v').blend = [(3.0, 'a'), (4.0, 'b')]

    s = StructConverter(
        src,
        target
    )

    check_conversion(s, '@BB', '@f',
                     (255, 127),
                     (3.0 + 4.0 * (127.0 / 255.0),))


def test13_blend_gamma():
    def to_srgb(value):
        if value <= 0.0031308:
            return 12.92 * value
        return 1.055 * (value ** (1.0 / 2.4)) - 0.055

    def from_srgb(value):
        if value <= 0.04045:
            return value * 1.0 / 12.92
        return ((value + 0.055) * (1.0 / 1.055)) ** 2.4

    src = Struct()
    src.append('a', Struct.EUInt8, Struct.ENormalized | Struct.EGamma)
    src.append('b', Struct.EUInt8, Struct.ENormalized | Struct.EGamma)

    target = Struct()
    target.append('v', Struct.EUInt8, Struct.ENormalized | Struct.EGamma)
    target.field('v').blend = [(1, 'a'), (1, 'b')]

    s = StructConverter(src, target)
    ref = int(np.round(to_srgb(from_srgb(100 / 255.0) +
                               from_srgb(200 / 255.0)) * 255))

    check_conversion(s, '@BB', '@B', (100, 200), (ref,))


@pytest.mark.parametrize('param', supported_types)
def test14_weight(param):
    src = Struct() \
        .append('value1', param[1], Struct.ENormalized) \
        .append('value2', param[1], Struct.ENormalized) \
        .append('weight', param[1], Struct.ENormalized | Struct.EWeight)

    target = Struct().append('value1', Struct.EFloat32) \
                     .append('value2', Struct.EFloat32)

    s = StructConverter(src, src)
    check_conversion(s, '@' + param[0] * 3,
                        '@' + param[0] * 3,
                     (10, 20, 20), (10, 20, 20))

    s = StructConverter(src, target)
    check_conversion(s, '@' + param[0] * 3, '@ff',
                     (10, 20, 20),
                     (0.5, 1.0))


def test15_test_dither():
    from mitsuba.core import Bitmap
    import numpy.linalg as la

    b = Bitmap(Bitmap.EY, Struct.EFloat32, [10, 256])
    value = np.linspace(0, 1 / 255.0, 10)
    np.array(b, copy=False)[:, :, 0] = np.tile(value, (256, 1))
    b = b.convert(Bitmap.EY, Struct.EUInt8, False)
    b = b.convert(Bitmap.EY, Struct.EFloat32, False)
    err = la.norm(np.mean(np.array(b, copy=False), axis=(0, 2)) - value)
    assert(err < 5e-4)
