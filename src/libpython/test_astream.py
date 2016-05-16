import unittest
import os
from os import path as PyPath
from mitsuba import AnnotatedStream, DummyStream, FileStream, MemoryStream
from mitsuba.filesystem import path

def touch(path):
    # Equivalent of `touch` that is compatible with Python 2
    open(str(path), 'a').close()

class AnnotatedStreamTest(unittest.TestCase):
    roPath = path('./read_only_file_for_annotated_stream')
    woPath = path('./write_only_file_for_annotated_stream')

    # TODO: more contents, exercise lots of types
    contents = ['some sentence', 42, 13.37]

    def writeContents(self, stream):
        for v in AnnotatedStreamTest.contents:
            stream.writeValue(v)
        stream.flush()

    def setUp(self):
        touch(AnnotatedStreamTest.roPath)
        # Provide a fresh instances of each kind of Stream implementation
        self.streams = {}
        self.streams['DummyStream'] = DummyStream()
        self.streams['MemoryStream'] = MemoryStream(64)
        self.streams['FileStream (read only)'] = FileStream(AnnotatedStreamTest.roPath, False)
        self.streams['DummyStream (write only)'] = FileStream(AnnotatedStreamTest.woPath, True)

    def tearDown(self):
        os.remove(str(AnnotatedStreamTest.roPath))
        os.remove(str(AnnotatedStreamTest.woPath))

    def test01_basics(self):
        for (name, stream) in self.streams.items():
            with self.subTest(name):
                astream = AnnotatedStream(stream)

                # Should have the same read & write capabilities as the underlying stream
                self.assertEqual(astream.canRead(), stream.canRead())
                self.assertEqual(astream.canWrite(), stream.canWrite())
                self.assertEqual(astream.getSize(), 0)

    def test02_construction(self):
        # TODO: test construction boundary conditions (read / write
        # capabilities, correct / incorrect streams)
        self.assertEqual('Not implemented yet', '')
        for (name, stream) in self.streams.items():
            pass

    def test03_toc_management(self):
        # TODO: test TOC management
        self.assertEqual('Not implemented yet', '')
        for (name, stream) in self.streams.items():
            pass

    def test04_toc_load_save(self):
        # TODO: test TOC loading & reading back
        self.assertEqual('Not implemented yet', '')
        for (name, stream) in self.streams.items():
            pass

    def test05_read_write(self):
        # TODO: test writing and reading back values, including with prefixes
        self.assertEqual('Not implemented yet', '')
        for (name, stream) in self.streams.items():
            pass

    def test06_gold_standard(self):
        # TODO: test read & write against reference serialized file ("gold
        #       standard") that is known to work. In this file, include:
        #       - Many types
        #       - Prefix hierarchy
        # TODO: make sure that a file written using the Python bindings can be
        #       read with C++, and the other way around.
        self.assertEqual('Not implemented yet', '')
        for (name, stream) in self.streams.items():
            pass

    def test07_throw_on_missing(self):
        pass

if __name__ == '__main__':
    unittest.main()
