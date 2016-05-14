import unittest
from mitsuba import DummyStream, FileStream
from mitsuba.filesystem import path

class DummyStreamTest(unittest.TestCase):
    def setUp(self):
        # Provide a fresh DummyStream instance
        self.s = DummyStream()

    def test01_basics(self):
        self.assertTrue(self.s.canWrite())
        self.assertFalse(self.s.canRead())

    def test02_size_and_pos(self):
        self.assertEqual(self.s.getSize(), 0)
        self.assertEqual(self.s.getPos(), 0)
        self.s.writeValue("hello") # string length as a uint32_t (4) + string (5)
        self.assertEqual(self.s.getSize(), 9)
        self.assertEqual(self.s.getPos(), 9)
        self.s.writeValue(42) # int (1)
        self.assertEqual(self.s.getSize(), 9+1)
        self.assertEqual(self.s.getPos(), 9+1)

    def test03_truncate(self):
        self.assertEqual(self.s.getSize(), 0)
        self.assertEqual(self.s.getPos(), 0)
        self.s.truncate(100)
        self.assertEqual(self.s.getSize(), 100)
        self.assertEqual(self.s.getPos(), 0)
        self.s.seek(99)
        self.assertEqual(self.s.getPos(), 99)
        self.s.truncate(50)
        self.assertEqual(self.s.getSize(), 50)
        self.assertEqual(self.s.getPos(), 50)
        self.s.writeValue("hello")
        self.assertEqual(self.s.getSize(), 50 + 9)
        self.assertEqual(self.s.getPos(), 50 + 9)

    def test04_seek(self):
        self.s.truncate(5)
        with self.assertRaises(Exception):
            self.s.seek(5)
        self.s.seek(4)

    def test05_str(self):
        self.s.writeValue("hello world")
        # string length as a uint32_t (4) + string (11)
        self.assertEqual(str(self.s),
                        "DummyStream[" +
                           "hostByteOrder=little-endian, byteOrder=little-endian" +
                           ", size=15, pos=15]")

class FileStreamTest(unittest.TestCase):
    def setUp(self):
        # Provide read-only and write-only FileStream instances
        self.ro = FileStream(path("./test_file_read"), False)
        self.wo = FileStream(path("./test_file_write"), True)

    def test01_basics(self):
        self.assertTrue(self.ro.canRead())
        self.assertFalse(self.ro.canWrite())
        self.assertFalse(self.wo.canRead())
        self.assertTrue(self.wo.canWrite())

    def test02_todo(self):
        self.assertEqual('Not implemented yet', '')

if __name__ == '__main__':
    unittest.main()
