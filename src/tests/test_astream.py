try:
    import unittest2 as unittest
except:
    import unittest

import os
from os import path as PyPath
from mitsuba.core import AnnotatedStream, DummyStream, FileStream, \
    MemoryStream, Thread, EError
from mitsuba.core.filesystem import path

def touch(path):
    # Equivalent of `touch` that is compatible with Python 2
    open(str(path), 'a').close()

class AnnotatedStreamTest(unittest.TestCase):
    roPath = path('./read_only_file_for_annotated_stream')
    woPath = path('./write_only_file_for_annotated_stream')

    def assertCountEqual(self, a, b):
        self.assertEqual(sorted(a), sorted(b))

    def setUp(self):
        touch(AnnotatedStreamTest.roPath)
        # Provide a fresh instances of each kind of Stream implementation
        self.streams = {}
        self.streams['DummyStream'] = DummyStream()
        self.streams['MemoryStream'] = MemoryStream(64)
        self.streams['FileStream (read only)'] = FileStream(AnnotatedStreamTest.roPath, False)
        self.streams['FileStream (with write)'] = FileStream(AnnotatedStreamTest.woPath, True)

        # TODO: more types
        self.contents = {
            'a': 0,
            'b': 42,
            'sub1': {
                'a': 13.37,
                'b': 'hello',
                'c': 'world',
                'sub2': {
                    'a': 'another one',
                    'longer key with spaces?': True
                }
            },
            'other': {
                'a': 'potential naming conflicts'
            },
            'empty': {
                # Stays empty, shouldn't appear as a key
            }
        }

    def tearDown(self):
        for stream in self.streams.values():
            stream.close()
        os.remove(str(AnnotatedStreamTest.roPath))
        os.remove(str(AnnotatedStreamTest.woPath))

    # Writes example contents (with nested names, various types, etc)
    def writeContents(self, astream):
        def writeRecursively(contents):
            for (key, val) in sorted(contents.items()):
                if type(val) is dict:
                    astream.push(key)
                    writeRecursively(val)
                    astream.pop()
                else:
                    astream.set(key, val)

        writeRecursively(self.contents)

    # Checks that all keys & values from the example contents
    # can be read back correctly from the annotated stream
    def checkContents(self, astream):
        prefixes = []

        def pathFromPrefixes(p):
            return '.'.join(prefixes)

        def getKeysRecursively(keys, contents):
            prefix = pathFromPrefixes(prefixes)
            if len(prefix) > 0:
                prefix = prefix + '.'

            if len(contents) <= 0:
                return keys

            for (key, val) in sorted(contents.items()):
                if type(val) is dict:
                    prefixes.append(key)
                    getKeysRecursively(keys, val)
                    prefixes.pop()
                else:
                    keys.append(prefix + key)

            return keys

        def checkRecursively(contents):
            # Check the keys at each level
            self.assertCountEqual(astream.keys(),
                                  getKeysRecursively([], contents))

            for (key, val) in sorted(contents.items()):
                if type(val) is dict:
                    if len(val) > 0:
                        astream.push(key)
                        checkRecursively(val)
                        astream.pop()
                elif type(val) is float:
                    self.assertAlmostEqual(val, astream.get(key), places=6)
                else:
                    self.assertEqual(val, astream.get(key))

        checkRecursively(self.contents)

    # Simply writes all the example values to the stream (no annotations)
    def writeContentsToStream(self, stream):
        def writeRecursively(contents):
            for (_, val) in sorted(contents.items()):
                if type(val) is dict:
                    writeRecursively(val)
                else:
                    stream.write(val)

        writeRecursively(self.contents)

    def test01_basics(self):
        for (name, stream) in self.streams.items():
            with self.subTest(name):
                writeMode = stream.canWrite()
                astream = AnnotatedStream(stream, writeMode)

                # Should have read-only and write-only modes
                self.assertEqual(astream.canRead(), not writeMode)
                self.assertEqual(astream.canWrite(), writeMode)
                self.assertEqual(astream.size(), 0)

                # Cannot read or write to a closed astream
                # TODO: these assertions could be refined (there are other reasons to throw)
                astream.close()
                with self.assertRaises(Exception):
                    astream.get('some_field')
                with self.assertRaises(Exception):
                    astream.set('some_field', 42)

    def test02_construction(self):
        for (name, stream) in self.streams.items():
            with self.subTest(name):
                # Construct with throw_on_missing disabled
                writeMode = stream.canWrite()
                astream = AnnotatedStream(stream, writeMode, False)

                # Checking that both read/write mode are respected and throw_on_missing
                if writeMode:
                    with self.assertRaises(Exception):
                        v = 0
                        astream.get("some_field", v)
                else:
                    logger = Thread.thread().logger()
                    level = logger.logLevel()
                    logger.setLogLevel(EError)
                    astream.get("some_missing_field")
                    logger.setLogLevel(level)
                    with self.assertRaises(Exception):
                        astream.set("some_other_field", 42)

                # Construct with throw_on_missing enabled
                astream = AnnotatedStream(stream, writeMode, True)
                if not writeMode:
                    with self.assertRaises(Exception):
                        v = 0
                        astream.get("some_field_that_doesnt_exist", v)

        with self.subTest("Construct with nonempty stream that is not annotated"):
            mstream = MemoryStream()
            self.writeContentsToStream(mstream)
            with self.assertRaises(Exception):
                _ = AnnotatedStream(mstream, False)

    def test03_toc_management(self):
        for (name, stream) in self.streams.items():
            if not stream.canWrite():
                continue

            with self.subTest(name):
                astream = AnnotatedStream(stream, True)
                # Build a tree of prefixes
                self.assertCountEqual(astream.keys(), [])
                astream.set('a', 0)
                astream.set('b', 1)
                self.assertCountEqual(astream.keys(), ['a', 'b'])
                astream.push('level_2')
                self.assertCountEqual(astream.keys(), [])
                astream.set('a', 2)
                astream.set('b', 3)
                self.assertCountEqual(astream.keys(), ['a', 'b'])
                astream.pop()
                self.assertCountEqual(astream.keys(),
                                      ['a', 'b', 'level_2.a', 'level_2.b'])

                astream.push('level_2')
                self.assertCountEqual(astream.keys(), ['a', 'b'])
                astream.push('level_3')
                astream.set('c', 4)
                astream.set('d', 5)
                self.assertCountEqual(astream.keys(), ['c', 'd'])
                astream.pop()
                self.assertCountEqual(astream.keys(),
                                      ['a', 'b', 'level_3.c', 'level_3.d'])
                astream.push('other')
                self.assertCountEqual(astream.keys(), [])

    def test04_toc_save_and_load_back(self):
        with self.subTest("MemoryStream"):
            mstream = self.streams['MemoryStream']
            astream = AnnotatedStream(mstream, True)
            self.writeContents(astream)
            astream.close()  # Writes out the ToC

            # Will throw if the header is missing or corrupted
            astream = AnnotatedStream(mstream, False)
            # Will fail if contents do not match
            self.checkContents(astream)

        with self.subTest("FileStream"):
            fstream1 = self.streams['FileStream (with write)']
            astream = AnnotatedStream(fstream1, True)
            self.writeContents(astream)
            astream.close()  # Writes out the ToC
            fstream1.close()

            fstream2 = FileStream(AnnotatedStreamTest.woPath, False)
            astream = AnnotatedStream(fstream2, False)
            self.checkContents(astream)
            fstream2.close()

    @unittest.skip("Not implemented yet")
    def test05_gold_standard(self):
        # TODO: test read & write against reference serialized file ("gold
        #       standard") that is known to work. In this file, include:
        #       - Many types
        #       - Prefix hierarchy
        # TODO: make sure that a file written using the Python bindings can be
        #       read with C++, and the other way around.
        # TODO: also do read/write with endianness swap enabled
        for (name, stream) in self.streams.items():
            with self.subTest(name):
                pass

if __name__ == '__main__':
    unittest.main()
