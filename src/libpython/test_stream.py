import unittest
from mitsuba import DummyStream

class StreamTest(unittest.TestCase):

    def test00_not_implemented(self):
        self.assertEqual("Not implemented yet", "")

    def test101_dummy_stream_basics(self):
        s = DummyStream()
        self.assertTrue(s.canWrite())
        self.assertFalse(s.canRead())

    def test102_dummy_stream_size_and_pos(self):
        s = DummyStream()
        self.assertEqual(s.getSize(), 0)
        self.assertEqual(s.getPos(), 0)
        # TODO: write some data, check size and pos
        self.assertEqual("Not implemented yet", "")

    def test103_dummy_stream_truncate(self):
        s = DummyStream()
        self.assertEqual(s.getSize(), 0)
        self.assertEqual(s.getPos(), 0)
        s.truncate(100)
        self.assertEqual(s.getSize(), 100)
        self.assertEqual(s.getPos(), 0)
        s.seek(99)
        s.truncate(50)
        self.assertEqual(s.getSize(), 50)
        self.assertEqual(s.getPos(), 49)
        # TODO: write some data to increase the size
        self.assertEqual("Not implemented yet", "")

    def test104_dummy_stream_seek(self):
        s = DummyStream()
        s.truncate(5)
        with self.assertRaises(Exception):
            s.seek(5)
        s.seek(4)

if __name__ == '__main__':
    unittest.main()
