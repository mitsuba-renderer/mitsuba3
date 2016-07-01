import unittest

from mitsuba import Struct


class StructConvTest(unittest.TestCase):
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

if __name__ == '__main__':
    unittest.main()
