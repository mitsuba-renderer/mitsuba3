import unittest
from mitsuba.xml import loadString


class XMLTest(unittest.TestCase):
    def test01_invalidXML(self):
        with self.assertRaises(Exception):
            loadString("<?xml version='1.0'?>")

    def test01_invalidRootNode(self):
        with self.assertRaises(Exception):
            loadString("<?xml version='1.0'?><invalid></invalid>")


if __name__ == '__main__':
    unittest.main()
