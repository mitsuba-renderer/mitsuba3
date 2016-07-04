try:
    import unittest2 as unittest
except:
    import unittest

import os
from os import path as PyPath
from mitsuba import Stream, DummyStream, FileStream, MemoryStream, ZStream
from mitsuba.filesystem import path


def touch(path):
    # Equivalent of `touch` that is compatible with Python 2
    open(str(path), 'a').close()


class CommonStreamTest(unittest.TestCase):
    roPath = path('./read_only_file_for_common_tests')
    rwPath = path('./write_enabled_file_for_common_tests')

    # TODO: more contents, exercise lots of types
    contents = [82.548, 999, 'some sentence', 424, 'hi', 13.3701, True, 'hey',
                42, 'c', False, '', 99.998]

    def writeContents(self, stream):
        for v in CommonStreamTest.contents:
            stream.write(v)
        stream.flush()

    def checkContents(self, stream):
        stream.seek(0)
        for v in CommonStreamTest.contents:
            if type(v) is str:
                self.assertEqual(v, stream.readString())
            elif type(v) is int:
                self.assertEqual(v, stream.readLong())
            elif type(v) is float:
                self.assertAlmostEqual(v, stream.readFloat(), places=5)
            elif type(v) is bool:
                self.assertEqual(v, stream.readBoolean())

    def setUp(self):
        touch(CommonStreamTest.roPath)
        # Provide a fresh instances of each kind of Stream implementation
        self.streams = {}
        self.streams['DummyStream'] = DummyStream()
        self.streams['MemoryStream'] = MemoryStream(64)
        self.streams['FileStream (read only)'] = FileStream(
            CommonStreamTest.roPath, False)
        self.streams['FileStream (with write)'] = FileStream(
            CommonStreamTest.rwPath, True)

    def tearDown(self):
        for stream in self.streams.values():
            stream.close()

        os.remove(str(CommonStreamTest.roPath))
        os.remove(str(CommonStreamTest.rwPath))

    def test01_size_and_pos(self):
        for (name, stream) in self.streams.items():
            with self.subTest(name):
                self.assertEqual(stream.size(), 0)
                self.assertEqual(stream.tell(), 0)

                if stream.canWrite():
                    # string length as a uint32_t (4) + string (5)
                    stream.write("hello")
                    stream.flush()
                    self.assertEqual(stream.size(), 9)
                    self.assertEqual(stream.tell(), 9)
                    stream.write(42)  # Long (8)
                    stream.flush()
                    self.assertEqual(stream.size(), 9 + 8)
                    self.assertEqual(stream.tell(), 9 + 8)

    def test02_truncate(self):
        for (name, stream) in self.streams.items():
            if not stream.canWrite():
                continue
            with self.subTest(name):
                self.assertEqual(stream.size(), 0)
                self.assertEqual(stream.tell(), 0)
                stream.truncate(100)
                self.assertEqual(stream.size(), 100)
                self.assertEqual(stream.tell(), 0)
                stream.seek(99)
                self.assertEqual(stream.tell(), 99)
                stream.truncate(50)
                self.assertEqual(stream.size(), 50)
                self.assertEqual(stream.tell(), 50)
                stream.write("hello")
                stream.flush()
                self.assertEqual(stream.size(), 50 + 9)
                self.assertEqual(stream.tell(), 50 + 9)

    def test03_seek(self):
        for (name, stream) in self.streams.items():
            with self.subTest(name):
                size = 0

                self.assertEqual(stream.size(), size)
                self.assertEqual(stream.tell(), 0)

                if stream.canWrite():
                    size = 5
                    stream.truncate(size)
                    self.assertEqual(stream.size(), size)
                    self.assertEqual(stream.tell(), 0)

                stream.seek(5)
                self.assertEqual(stream.size(), size)
                self.assertEqual(stream.tell(), 5)
                # Seeking beyond the end of the file is okay
                # but won't make it larger
                stream.seek(20)
                self.assertEqual(stream.size(), size)
                self.assertEqual(stream.tell(), 20)

                if stream.canWrite():
                    # A subsequent write should start at the correct position
                    # and update the size.
                    stream.write(13.37)
                    stream.flush()
                    self.assertEqual(stream.tell(), 20 + 4)
                    self.assertEqual(stream.size(), 20 + 4)

    def test04_read_back(self):
        # Write some values to be read back
        temporaryWriteStream = FileStream(CommonStreamTest.roPath, True)
        self.writeContents(temporaryWriteStream)
        del temporaryWriteStream

        for (name, stream) in self.streams.items():
            if not stream.canRead():
                continue
            if stream.canWrite():
                self.writeContents(stream)

            with self.subTest(name):
                self.checkContents(stream)

    def test04_read_back_with_swapping(self):
        otherEndianness = Stream.EBigEndian
        if Stream.hostByteOrder() == otherEndianness:
            otherEndianness = Stream.ELittleEndian

        for (_, stream) in self.streams.items():
            stream.setByteOrder(otherEndianness)

        temporaryWriteStream = FileStream(CommonStreamTest.roPath, True)
        temporaryWriteStream.setByteOrder(otherEndianness)
        self.writeContents(temporaryWriteStream)
        del temporaryWriteStream

        for (name, stream) in self.streams.items():
            if not stream.canRead():
                continue
            with self.subTest(name):
                if stream.canWrite():
                    self.writeContents(stream)
                self.checkContents(stream)

    def test05_read_back_through_zstream(self):
        for (name, stream) in self.streams.items():
            with self.subTest("ZStream with underlying " + name):
                if not stream.canRead():
                    continue

                zstream = ZStream(stream)
                if zstream.canWrite():
                    self.writeContents(stream)
                    self.checkContents(stream)

    def test06_close(self):
        for (name, stream) in self.streams.items():
            with self.subTest(name):
                stream.close()
                self.assertFalse(stream.canRead())
                self.assertFalse(stream.canWrite())

# TODO: may need to test raw read & write functions since they are public
# TODO: test against gold standard (test asset to be created)


class DummyStreamTest(unittest.TestCase):
    def setUp(self):
        self.s = DummyStream()

    def test01_basics(self):
        self.assertTrue(self.s.canWrite())
        self.assertFalse(self.s.canRead())

    def test02_str(self):
        # string length as a uint32_t (4) + string (11)
        self.s.write("hello world")
        self.s.setByteOrder(Stream.EBigEndian)
        self.assertEqual(str(self.s),
                        "DummyStream[" +
                        "hostByteOrder=little-endian" +
                        ", byteOrder=big-endian, isClosed=0" +
                        ", size=15, pos=15]")


class FileStreamTest(unittest.TestCase):
    roPath = path("./test_file_read")
    rwPath = path("./test_file_write")
    newPath = path("./path_that_did_not_exist")

    def setUp(self):
        touch(FileStreamTest.roPath)
        touch(FileStreamTest.rwPath)
        if PyPath.exists(str(FileStreamTest.newPath)):
            os.remove(str(FileStreamTest.newPath))

        # Provide read-only and read-write FileStream instances on fresh files
        self.ro = FileStream(FileStreamTest.roPath, False)
        self.wo = FileStream(FileStreamTest.rwPath, True)

    def tearDown(self):
        self.ro.close()
        self.wo.close()

        os.remove(str(FileStreamTest.roPath))
        os.remove(str(FileStreamTest.rwPath))
        if PyPath.exists(str(FileStreamTest.newPath)):
            os.remove(str(FileStreamTest.newPath))

    def test01_basics(self):
        self.assertTrue(self.ro.canRead())
        self.assertFalse(self.ro.canWrite())
        self.assertTrue(self.wo.canRead())
        self.assertTrue(self.wo.canWrite())

        # Read-only mode should be enforced
        with self.assertRaises(Exception):
            self.ro.write("hello")

        self.wo.write(42)
        self.wo.flush()
        self.wo.seek(0)
        self.assertEqual(42, self.wo.readLong())

    def test02_create_on_open(self):
        p = FileStreamTest.newPath
        # File should only be created when opening in write mode
        self.assertFalse(PyPath.exists(str(p)))
        fstream = FileStream(p, True)
        self.assertTrue(PyPath.exists(str(p)))
        fstream.close()
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
        self.assertEqual(self.wo.tell(), 0)
        self.wo.truncate(5)
        self.assertEqual(self.wo.tell(), 0)
        self.wo.write("hello")
        self.wo.flush()
        self.assertEqual(self.wo.tell(), 0 + 9)
        self.wo.truncate(5)
        self.assertEqual(self.wo.tell(), 5)

    def test04_seek(self):
        self.wo.truncate(5)
        self.wo.seek(5)
        self.wo.flush()
        self.assertEqual(self.wo.tell(), 5)
        self.assertEqual(self.wo.size(), 5)
        self.wo.write("hello world")
        self.wo.flush()
        self.wo.seek(3)
        self.assertEqual(self.wo.tell(), 3)
        self.assertEqual(self.wo.size(), 5 + 4 + 11)
        self.wo.write("dlrow olleh")
        self.wo.flush()
        self.assertEqual(self.wo.tell(), 3 + 4 + 11)
        self.assertEqual(self.wo.size(), 5 + 4 + 11)

        # Seeking further that the limit of the file should be okay too, but
        # not update the size.
        self.ro.seek(10)
        self.assertEqual(self.ro.tell(), 10)
        self.assertEqual(self.ro.size(), 0)

        self.wo.seek(40)
        self.assertEqual(self.wo.tell(), 40)
        self.assertEqual(self.wo.size(), 5 + 4 + 11)  # Actual size

        # A subsequent write should start at the correct position
        self.wo.write(13.37)
        self.wo.flush()
        self.assertEqual(self.wo.tell(), 40 + 4)
        self.assertEqual(self.wo.size(), 40 + 4)

    def test05_str(self):
        self.assertEqual(str(self.ro),
                         "FileStream[" +
                         "hostByteOrder=little-endian" +
                         ", byteOrder=little-endian, isClosed=0" +
                         ", path=" + str(FileStreamTest.roPath) +
                         ", size=0" +
                         ", pos=0" +
                         ", writeEnabled=false]")

        self.wo.write("hello world")
        self.wo.flush()
        self.assertEqual(str(self.wo),
                         "FileStream[" +
                         "hostByteOrder=little-endian" +
                         ", byteOrder=little-endian, isClosed=0" +
                         ", path=" + str(FileStreamTest.rwPath) +
                         ", size=15" +
                         ", pos=15" +
                         ", writeEnabled=true]")
        self.wo.setByteOrder(Stream.EBigEndian)
        self.wo.close()
        self.assertEqual(str(self.wo),
                         "FileStream[" +
                         "hostByteOrder=little-endian" +
                         ", byteOrder=big-endian, isClosed=1" +
                         ", path=" + str(FileStreamTest.rwPath) +
                         ", size=?" +
                         ", pos=?" +
                         ", writeEnabled=true]")


class MemoryStreamTest(unittest.TestCase):
    defaultCapacity = 64

    def setUp(self):
        # Provide a fresh MemoryStream instance
        self.s = MemoryStream(MemoryStreamTest.defaultCapacity)

    def test01_basics(self):
        self.assertTrue(self.s.canWrite())
        self.assertTrue(self.s.canRead())

    def test02_str(self):
        self.s.write("hello world")
        self.s.setByteOrder(Stream.EBigEndian)
        self.assertEqual(str(self.s),
                         "MemoryStream[" +
                         "hostByteOrder=little-endian" +
                         ", byteOrder=big-endian, isClosed=0" +
                         ", ownsBuffer=1, capacity=" +
                         str(MemoryStreamTest.defaultCapacity) +
                         ", size=15, pos=15]")


class ZStreamTest(unittest.TestCase):
    roPath = path('./read_only_file_for_zstream_tests')
    rwPath = path('./write_enabled_file_for_zstream_tests')

    def setUp(self):
        touch(ZStreamTest.roPath)
        # Provide a fresh instances of ZStream with various underlying streams
        self.streams = {}
        self.streams['ZStream (MemoryStream)'] = ZStream(MemoryStream(64))
        self.streams['ZStream (FileStream[read only])'] = ZStream(
            FileStream(ZStreamTest.roPath, False))
        self.streams['ZStream (FileStream[with write])'] = ZStream(
            FileStream(ZStreamTest.rwPath, True))

    def tearDown(self):
        for stream in self.streams.values():
            stream.close()

        os.remove(str(ZStreamTest.roPath))
        os.remove(str(ZStreamTest.rwPath))

    def test01_construction(self):
        for (name, stream) in self.streams.items():
            with self.subTest(name):
                self.assertEqual(stream.canRead(),
                                 stream.childStream().canRead())
                self.assertEqual(stream.canWrite(),
                                 stream.childStream().canWrite())

    # TODO: test construction with different enum values
    # (EDeflateStream, EGZipStream)
    # TODO: test against a gold standard (compressed file)

if __name__ == '__main__':
    unittest.main()
