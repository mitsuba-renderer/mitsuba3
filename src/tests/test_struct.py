try:
    import unittest2 as unittest
except:
    import unittest

from mitsuba.core import Struct, StructConverter
import struct
import numpy as np


class StructConvTest(unittest.TestCase):
    def assertConversion(self, conv, srcFmt, dstFmt, dataIn, dataOut=None):
        srcData = struct.pack(srcFmt, *dataIn)
        # import binascii
        # print(binascii.hexlify(srcData).decode('utf8'))
        converted = conv.convert(srcData)
        # print(binascii.hexlify(converted).decode('utf8'))
        dstData = struct.unpack(dstFmt, converted)
        ref = dataOut if dataOut is not None else dataIn
        for i in range(len(dstData)):
            self.assertAlmostEqual(dstData[i], ref[i], places=6)

    def test01_basics(self):
        s = Struct()
        self.assertEqual(s.fieldCount(), 0)
        self.assertEqual(s.alignment(), 1)
        self.assertEqual(s.size(), 0)
        s.append("floatVal", Struct.EFloat32)
        self.assertEqual(s.fieldCount(), 1)
        self.assertEqual(s.alignment(), 4)
        self.assertEqual(s.size(), 4)
        s.append("byteVal", Struct.EUInt8)
        self.assertEqual(s.fieldCount(), 2)
        self.assertEqual(s.alignment(), 4)
        self.assertEqual(s.size(), 8)
        s.append("halfVal", Struct.EFloat16)
        self.assertEqual(s.fieldCount(), 3)
        self.assertEqual(s.alignment(), 4)
        self.assertEqual(s.size(), 8)
        self.assertEqual(len(s), 3)
        self.assertEqual(s[0].name, 'floatVal')
        self.assertEqual(s[0].offset, 0)
        self.assertEqual(s[0].size, 4)
        self.assertEqual(s[0].type, Struct.EFloat32)
        self.assertEqual(s[1].name, 'byteVal')
        self.assertEqual(s[1].offset, 4)
        self.assertEqual(s[1].size, 1)
        self.assertEqual(s[1].type, Struct.EUInt8)
        self.assertEqual(s[2].name, 'halfVal')
        self.assertEqual(s[2].offset, 6)
        self.assertEqual(s[2].size, 2)
        self.assertEqual(s[2].type, Struct.EFloat16)

    def test02_passthrough(self):
        for param in [
                      ("h", Struct.EInt16),
                      ("i", Struct.EInt32),
                      ("q", Struct.EInt64),
                      ("f", Struct.EFloat32),
                      ("d", Struct.EFloat64)
                    ]:

            with self.subTest(param=param):
                s = Struct().append("val", param[1])
                ss = StructConverter(s, s)
                self.assertConversion(ss, "<" + param[0] + param[0],
                                          "<" + param[0] + param[0],
                                      (100, -101))

    def test03_convert1(self):
        plist = [
            ("b", Struct.EInt8),
            ("h", Struct.EInt16),
            ("i", Struct.EInt32),
            ("q", Struct.EInt64),
            ("f", Struct.EFloat32),
            ("d", Struct.EFloat64)
        ]
        for p1 in plist:
            for p2 in plist:
                with self.subTest(p1=p1, p2=p2):
                    s1 = Struct().append("val", p1[1])
                    s2 = Struct().append("val", p2[1])
                    s = StructConverter(s1, s2)
                    self.assertConversion(s, "<" + p1[0], "<" + p2[0], (1,))

    def test04_convert1_half(self):
        plist = [
            ("b", Struct.EInt8),
            ("h", Struct.EInt16),
            ("i", Struct.EInt32),
            ("q", Struct.EInt64),
            ("f", Struct.EFloat32),
            ("d", Struct.EFloat64)
        ]
        for p in plist:
            with self.subTest(p=p):
                s1 = Struct().append("val", Struct.EFloat16)
                s2 = Struct().append("val", p[1])
                s = StructConverter(s1, s2)
                self.assertConversion(s, "<H", "<" + p[0], (15360, ), (1, ))

            with self.subTest(p=p):
                s1 = Struct().append("val", p[1])
                s2 = Struct().append("val", Struct.EFloat16)
                s = StructConverter(s1, s2)
                self.assertConversion(s, "<" + p[0], "<H", (1, ), (15360, ))

    def test05_bswap(self):
        for param in [
                      ("H", Struct.EUInt16),
                      ("I", Struct.EUInt32),
                      ("Q", Struct.EUInt64)
                    ]:
            with self.subTest(param=param):
                le = Struct().append("val", param[1])
                be = Struct(byteOrder=Struct.EBigEndian).append(
                        "val", param[1])
                s1 = StructConverter(le, be)
                s2 = StructConverter(be, le)
                self.assertConversion(s1, "<" + param[0] + param[0],
                                          ">" + param[0] + param[0],
                                      (100, 101))
                self.assertConversion(s2, ">" + param[0] + param[0],
                                          "<" + param[0] + param[0],
                                      (100, 101))

    def test06_bswap_signed(self):
        for param in [
                      ("h", Struct.EInt16),
                      ("i", Struct.EInt32),
                      ("q", Struct.EInt64),
                      ("f", Struct.EFloat32),
                      ("d", Struct.EFloat64)
                    ]:

            with self.subTest(param=param, mode="le->be"):
                le = Struct().append("val", param[1])
                be = Struct(byteOrder=Struct.EBigEndian).append(
                        "val", param[1])
                s1 = StructConverter(le, be)
                self.assertConversion(s1, "<" + param[0] + param[0],
                                          ">" + param[0] + param[0],
                                      (100, -101))
            with self.subTest(param=param, mode="be->le"):
                le = Struct().append("val", param[1])
                be = Struct(byteOrder=Struct.EBigEndian).append(
                        "val", param[1])
                s = StructConverter(be, le)
                self.assertConversion(s, ">" + param[0] + param[0],
                                         "<" + param[0] + param[0],
                                      (100, -101))

    def test07_missing(self):
        s1 = Struct().append("val1", Struct.EInt32) \
                     .append("val3", Struct.EInt32)
        s2 = Struct().append("val1", Struct.EInt32) \
                     .append("val2", Struct.EInt32, int(Struct.EDefault), 123) \
                     .append("val3", Struct.EInt32)
        s = StructConverter(s1, s2)
        self.assertConversion(s, "<ii", "<iii", (1, 3), (1, 123, 3))

    def test08_remapRange_1(self):
        s = StructConverter(
            Struct().append("v", Struct.EUInt8, int(Struct.ENormalized)),
            Struct().append("v", Struct.EFloat32)
        )
        self.assertConversion(s, "<BBB", "<fff", (0, 1, 255),
                              (0, np.float32(1.0 / 255.0), 1))

        s = StructConverter(
            Struct().append("v", Struct.EInt8, int(Struct.ENormalized)),
            Struct().append("v", Struct.EFloat32)
        )
        self.assertConversion(s, "<bbb", "<fff", (-128, 0, 127),
                              (0, np.float32(128 / 255.0), 1))

    def test08_remapRange_2(self):
        s = StructConverter(
            Struct().append("v", Struct.EFloat32),
            Struct().append("v", Struct.EUInt8, int(Struct.ENormalized))
        )
        self.assertConversion(s, "<fff", "<BBB",
                              (0, np.float32(1.0 / 255.0), 1),
                              (0, 1, 255))

        s = StructConverter(
            Struct().append("v", Struct.EFloat32),
            Struct().append("v", Struct.EInt8, int(Struct.ENormalized))
        )
        self.assertConversion(s, "<fff", "<bbb",
                              (0, np.float32(128 / 255.0), 1),
                              (-128, 0, 127))

    def test08_remapRange_3(self):
        s = StructConverter(
            Struct().append("v", Struct.EInt16, int(Struct.ENormalized)),
            Struct().append("v", Struct.EUInt8, int(Struct.ENormalized))
        )
        self.assertConversion(s, "<hhh", "<BBB",
                              (-32768, 0, 32767),
                              (0, 128, 255))

        s = StructConverter(
            Struct().append("v", Struct.EInt8, int(Struct.ENormalized)),
            Struct().append("v", Struct.EUInt16, int(Struct.ENormalized))
        )
        self.assertConversion(s, "<bbb", "<HHH",
                              (-128, 0, 127),
                              (0, 32896, 65535))

    def test09_gamma_0(self):
        s = StructConverter(
            Struct().append("v", Struct.EUInt8,
                            int(Struct.ENormalized) | int(Struct.EGamma)),
            Struct().append("v", Struct.EFloat32)
        )
        self.assertConversion(s, "<BBB", "<fff",
                              (0, 127, 255),
                              (0, np.float32(0.212230786), 1))

    def test09_gamma_1(self):
        s = StructConverter(
            Struct().append("v", Struct.EFloat32),
            Struct().append("v", Struct.EUInt8,
                            int(Struct.ENormalized) | int(Struct.EGamma)),
        )
        self.assertConversion(s, "<fff", "<BBB",
                              (0, np.float32(0.212230786), 1),
                              (0, 127, 255))

    def test09_fail(self):
        s = StructConverter(
            Struct().append("v", Struct.EInt32, default=10,
                            flags=int(Struct.EAssert)),
            Struct().append("v", Struct.EInt32)
        )
        self.assertConversion(s, "<i", "<i", (10,), (10,))
        with self.assertRaises(RuntimeError):
            self.assertConversion(s, "<i", "<i", (11,), (11,))

    def test10_fail(self):
        s = StructConverter(
            Struct().append("v", Struct.EFloat32, default=10,
                            flags=int(Struct.EAssert)),
            Struct().append("v", Struct.EFloat32)
        )
        self.assertConversion(s, "<f", "<f", (10,), (10,))
        with self.assertRaises(RuntimeError):
            self.assertConversion(s, "<f", "<f", (11,), (11,))

    def test11_fail(self):
        s = StructConverter(
            Struct().append("v1", Struct.EFloat32, default=10,
                            flags=int(Struct.EAssert))
                    .append("v2", Struct.EFloat32),
            Struct().append("v2", Struct.EFloat32)
        )
        self.assertConversion(s, "<ff", "<f", (10, 10,), (10,))
        with self.assertRaises(RuntimeError):
            self.assertConversion(s, "<ff", "<f", (11, 11), (11,))

if __name__ == '__main__':
    unittest.main()
