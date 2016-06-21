import unittest
from mitsuba import Frame

class FrameTest(unittest.TestCase):
    def test01_construction(self):
        f = Frame()
        f = Frame([1, 2, 3])

    def test02_more_tests(self):
        self.fail("Not implemented yet")

if __name__ == '__main__':
    unittest.main()
