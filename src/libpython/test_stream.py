import unittest
import os
from os import path as PyPath
from mitsuba import DummyStream, FileStream, MemoryStream
from mitsuba.filesystem import path

class CommonStreamTest(unittest.TestCase):
    def setUp(self):
        # Provide a fresh instances of each kind of Stream implementation
        self.streams = [DummyStream(), MemoryStream(64)]
        # TODO: add FileStream too, read & write

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

    def test03_seek(self):
        for stream in self.streams:
            self.assertEqual(stream.getSize(), 0)
            self.assertEqual(stream.getPos(), 0)
            stream.truncate(5)
            self.assertEqual(stream.getSize(), 5)
            self.assertEqual(stream.getPos(), 0)
            stream.seek(5)
            self.assertEqual(stream.getSize(), 5)
            self.assertEqual(stream.getPos(), 5)
            # Seeking beyond the end of the file is okay, but won't make it larger
            # TODO: this behavior is inconsistent for MemoryStream
            stream.seek(10)
            self.assertEqual(stream.getSize(), 5)
            self.assertEqual(stream.getPos(), 10)

    # TODO: more read / write tests
    # TODO: tests where we read back from an existing file, exercising various types

class DummyStreamTest(unittest.TestCase):
    def setUp(self):
        self.s = DummyStream()

    def test01_basics(self):
        self.assertTrue(self.s.canWrite())
        self.assertFalse(self.s.canRead())

    def test02_str(self):
        self.s.writeValue("hello world")
        # string length as a uint32_t (4) + string (11)
        self.assertEqual(str(self.s),
                        "DummyStream[" +
                        "hostByteOrder=little-endian, byteOrder=little-endian" +
                        ", size=15, pos=15]")


class FileStreamTest(unittest.TestCase):
    roPath = path("./test_file_read")
    woPath = path("./test_file_write")
    newPath = path("./path_that_did_not_exist")


    def setUp(self):
        # Equivalent of `touch` that is compatible with Python 2
        open(str(FileStreamTest.roPath), 'a').close()
        open(str(FileStreamTest.woPath), 'a').close()
        if PyPath.exists(str(FileStreamTest.newPath)):
            os.remove(str(FileStreamTest.newPath))

        # Provide read-only and write-only FileStream instances on fresh files
        self.ro = FileStream(FileStreamTest.roPath, False)
        self.wo = FileStream(FileStreamTest.woPath, True)

    def tearDown(self):
        os.remove(str(FileStreamTest.roPath))
        os.remove(str(FileStreamTest.woPath))
        if PyPath.exists(str(FileStreamTest.newPath)):
            os.remove(str(FileStreamTest.newPath))

        # w = FileStream(path('./secret_hello'), True)
        # w.seek(0)
        # w.writeValue("hello world")
        # w.flush()

    def test01_basics(self):
        self.assertTrue(self.ro.canRead())
        self.assertFalse(self.ro.canWrite())
        self.assertFalse(self.wo.canRead())
        self.assertTrue(self.wo.canWrite())

        # Read / write modes should be enforced
        with self.assertRaises(Exception):
            self.ro.writeValue("hello")
        with self.assertRaises(Exception):
            v = 0
            self.wo.readValue(v)

    def test02_create_on_open(self):
        p = FileStreamTest.newPath
        # File should only be created when opening in write mode
        self.assertFalse(PyPath.exists(str(p)))
        _ = FileStream(p, True)
        self.assertTrue(PyPath.exists(str(p)))
        os.remove(str(p))
        # In read-only mode, throws if the file doesn't exist
        self.assertFalse(PyPath.exists(str(p)))
        with self.assertRaises(Exception):
            _ = FileStream(p, False)
        self.assertFalse(PyPath.exists(str(p)))

    def test03_truncate(self):
        # Cannot truncate a read-only stream
        with self.assertRaises(Exception):
            self.ro.truncate(5)

        # Truncating shouldn't change the position if not necessary
        self.assertEqual(self.wo.getPos(), 0)
        self.wo.truncate(5)
        self.assertEqual(self.wo.getPos(), 0)
        self.wo.writeValue("hello")
        self.wo.flush()
        self.assertEqual(self.wo.getPos(), 0 + 9) # TODO: why not 9? (= 4 + 5)
        self.wo.truncate(5)
        self.assertEqual(self.wo.getPos(), 5)

    def test04_seek(self):
        self.wo.truncate(5)
        self.wo.seek(5)
        self.assertEqual(self.wo.getPos(), 5)
        self.assertEqual(self.wo.getSize(), 5)
        self.wo.writeValue("hello world")
        self.wo.seek(5)
        self.assertEqual(self.wo.getPos(), 5)
        self.assertEqual(self.wo.getSize(), 5+4+11)
        self.wo.writeValue("dlrow olleh")
        self.assertEqual(self.wo.getPos(), 5+4+11)
        self.assertEqual(self.wo.getSize(), 5+4+11)

        # Seeking further that the limit of the file should be okay too
        self.ro.seek(10)
        self.assertEqual(self.ro.getPos(), 10)
        self.assertEqual(self.ro.getSize(), 0)

        self.wo.seek(40)
        self.assertEqual(self.wo.getPos(), 40)
        self.assertEqual(self.wo.getSize(), 5+4+11) # Actual size not changed

    def test05_str(self):
        self.assertEqual(str(self.ro),
                         "FileStream[" +
                         "hostByteOrder=little-endian, byteOrder=little-endian" +
                         ", path=" + str(FileStreamTest.roPath) +
                         ", writeOnly=false]")

        self.wo.writeValue("hello world")
        self.assertEqual(str(self.wo),
                         "FileStream[" +
                         "hostByteOrder=little-endian, byteOrder=little-endian" +
                         ", path=" + str(FileStreamTest.woPath) +
                         ", writeOnly=true]")


class MemoryStreamTest(unittest.TestCase):
    defaultCapacity = 64

    def setUp(self):
        # Provide a fresh MemoryStream instance
        self.s = MemoryStream(MemoryStreamTest.defaultCapacity)

    def test01_basics(self):
        self.assertTrue(self.s.canWrite())
        self.assertTrue(self.s.canRead())

    def test02_str(self):
        self.s.writeValue("hello world")
        # string length as a uint32_t (4) + string (11)
        self.assertEqual(str(self.s),
                         "MemoryStream[" +
                         "hostByteOrder=little-endian, byteOrder=little-endian" +
                         ", ownsBuffer=1, capacity=" + str(MemoryStreamTest.defaultCapacity) +
                         ", size=15, pos=15]")

if __name__ == '__main__':
    unittest.main()
