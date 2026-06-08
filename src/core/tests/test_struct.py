import struct

from mitsuba import Struct, StructConverter

# The structured-data conversion engine lives in the standalone 'struct-jit'
# library, which has its own extensive test suite covering the numeric
# conversion behavior (type/normalization/gamma/weight/alpha/blend/dither/
# endianness/defaults/checks). These tests therefore only exercise the thin
# Mitsuba Python binding surface (mitsuba.Struct / mitsuba.StructConverter).


def test01_basics():
    s = Struct()
    assert len(s) == 0
    assert s.align() == 1
    assert s.nbytes() == 0

    s.append('float_val', Struct.Type.Float32)
    assert len(s) == 1
    assert s.align() == 4
    assert s.nbytes() == 4

    s.append('byte_val', Struct.Type.UInt8)
    assert len(s) == 2
    assert s.nbytes() == 8

    s.append('half_val', Struct.Type.Float16)
    assert len(s) == 3
    assert s.nbytes() == 8

    assert 'float_val' in s and 'missing' not in s

    assert s[0].name == 'float_val' and s[0].offset == 0 \
        and s[0].type == Struct.Type.Float32
    assert s[1].name == 'byte_val' and s[1].offset == 4 \
        and s[1].type == Struct.Type.UInt8
    assert s[2].name == 'half_val' and s[2].offset == 6 \
        and s[2].type == Struct.Type.Float16


def test02_convert_binding():
    # End-to-end smoke test of the StructConverter(...).convert(bytes) binding.
    # The conversion logic itself is covered by struct-jit's own tests.
    s1 = Struct().append('v', Struct.Type.UInt8, Struct.Flags.Normalized)
    s2 = Struct().append('v', Struct.Type.Float32)
    conv = StructConverter(s1, s2)
    assert conv.source() == s1 and conv.target() == s2
    out = conv.convert(struct.pack('@B', 255))
    assert struct.unpack('@f', out)[0] == 1.0

    # Reordering plus a default-filled output field.
    src = Struct().append('a', Struct.Type.Int32).append('b', Struct.Type.Int32)
    dst = (Struct().append('b', Struct.Type.Int32)
                   .append('c', Struct.Type.Int32, Struct.Flags.Default, 7))
    out = StructConverter(src, dst).convert(struct.pack('@ii', 1, 2))
    assert struct.unpack('@ii', out) == (2, 7)
