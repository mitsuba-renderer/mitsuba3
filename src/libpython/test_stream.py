import unittest
from mitsuba import DummyStream, FileStream, MemoryStream
from mitsuba.filesystem import path

class CommonStreamTest(unittest.TestCase):
    def setUp(self):
        # Provide a fresh instances of each kind of Stream implementation
        self.streams = [DummyStream(), MemoryStream(64)]
        # TODO: add FileStream too

    def test01_size_and_pos(self):
        for stream in self.streams:
            self.assertEqual(stream.getSize(), 0)
            self.assertEqual(stream.getPos(), 0)
            stream.writeValue("hello") # string length as a uint32_t (4) + string (5)
            self.assertEqual(stream.getSize(), 9)
            self.assertEqual(stream.getPos(), 9)
            stream.writeValue(42) # int (1)
            self.assertEqual(stream.getSize(), 9+1)
            self.assertEqual(stream.getPos(), 9+1)

    def test02_truncate(self):
        for stream in self.streams:
            self.assertEqual(stream.getSize(), 0)
            self.assertEqual(stream.getPos(), 0)
            stream.truncate(100)
            self.assertEqual(stream.getSize(), 100)
            self.assertEqual(stream.getPos(), 0)
            stream.seek(99)
            self.assertEqual(stream.getPos(), 99)
            stream.truncate(50)
            self.assertEqual(stream.getSize(), 50)
            self.assertEqual(stream.getPos(), 50)
            stream.writeValue("hello")
            self.assertEqual(stream.getSize(), 50 + 9)
            self.assertEqual(stream.getPos(), 50 + 9)

class DummyStreamTest(unittest.TestCase):
    def setUp(self):
        self.s = DummyStream()

    def test01_basics(self):
        self.assertTrue(self.s.canWrite())
        self.assertFalse(self.s.canRead())

    def test02_seek(self):
        self.s.truncate(5)
        with self.assertRaises(Exception):
            self.s.seek(6)
        self.s.seek(5)

    def test03_str(self):
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


class MemoryStreamTest(unittest.TestCase):
    defaultCapacity = 64

    def setUp(self):
        # Provide a fresh MemoryStream instance
        self.s = MemoryStream(MemoryStreamTest.defaultCapacity)

    def test01_basics(self):
        self.assertTrue(self.s.canWrite())
        self.assertTrue(self.s.canRead())

    def test02_seek(self):
        # MemoryStream can seek further to its size and will adapt nicely
        self.s.truncate(5)
        self.s.seek(10)
        self.assertEqual(self.s.getSize(), 10)
        self.assertEqual(self.s.getPos(), 10)

    def test03_str(self):
        self.s.writeValue("hello world")
        # string length as a uint32_t (4) + string (11)
        self.assertEqual(str(self.s),
                         "MemoryStream[" +
                         "hostByteOrder=little-endian, byteOrder=little-endian" +
                         ", ownsBuffer=1, capacity=" + str(MemoryStreamTest.defaultCapacity) +
                         ", size=15, pos=15]")

if __name__ == '__main__':
    unittest.main()
