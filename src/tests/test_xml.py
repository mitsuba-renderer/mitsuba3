try:
    import unittest2 as unittest
except:
    import unittest

from mitsuba.core.xml import loadString


class XMLTest(unittest.TestCase):
    def test01_invalidXML(self):
        with self.assertRaises(Exception):
            loadString("<?xml version='1.0'?>")

    def test02_invalidRootNode(self):
        with self.assertRaises(Exception):
            loadString("<?xml version='1.0'?><invalid></invalid>")

    def test04_validRootNode(self):
        loadString("<?xml version='1.0'?>\n<scene version='0.4.0'></scene>")


if __name__ == '__main__':
    unittest.main()
