try:
    import unittest2 as unittest
except:
    import unittest

from mitsuba.core.math import log2i
from mitsuba.core.math import isPowerOfTwo
from mitsuba.core.math import roundToPowerOfTwo


class MathTest(unittest.TestCase):
    def test01_log2i(self):
        self.assertEqual(log2i(1), 0)
        self.assertEqual(log2i(2), 1)
        self.assertEqual(log2i(3), 1)
        self.assertEqual(log2i(4), 2)
        self.assertEqual(log2i(5), 2)
        self.assertEqual(log2i(6), 2)
        self.assertEqual(log2i(7), 2)
        self.assertEqual(log2i(8), 3)

    def test01_isPowerOfTwo(self):
        self.assertEqual(isPowerOfTwo(0), False)
        self.assertEqual(isPowerOfTwo(1), True)
        self.assertEqual(isPowerOfTwo(2), True)
        self.assertEqual(isPowerOfTwo(3), False)
        self.assertEqual(isPowerOfTwo(4), True)
        self.assertEqual(isPowerOfTwo(5), False)
        self.assertEqual(isPowerOfTwo(6), False)
        self.assertEqual(isPowerOfTwo(7), False)
        self.assertEqual(isPowerOfTwo(8), True)

    def test01_roundToPowerOfTwo(self):
        self.assertEqual(roundToPowerOfTwo(1), 1)
        self.assertEqual(roundToPowerOfTwo(2), 2)
        self.assertEqual(roundToPowerOfTwo(3), 4)
        self.assertEqual(roundToPowerOfTwo(4), 4)
        self.assertEqual(roundToPowerOfTwo(5), 8)
        self.assertEqual(roundToPowerOfTwo(6), 8)
        self.assertEqual(roundToPowerOfTwo(7), 8)
        self.assertEqual(roundToPowerOfTwo(8), 8)

        self.assertEqual(roundToPowerOfTwo(0), 1)

if __name__ == '__main__':
    unittest.main()
