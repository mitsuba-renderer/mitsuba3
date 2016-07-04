try:
    import unittest2 as unittest
except:
    import unittest

from mitsuba import ArgParser


class ArgParserTest(unittest.TestCase):
    def test01_short_args(self):
        ap = ArgParser()
        a = ap.add("-a")
        b = ap.add("-b")
        c = ap.add("-c")
        d = ap.add("-d")
        ap.parse(['mitsuba', '-aba', '-d'])
        self.assertTrue(bool(a))
        self.assertEqual(a.count(), 2)
        self.assertTrue(bool(b))
        self.assertEqual(b.count(), 1)
        self.assertFalse(bool(c))
        self.assertEqual(c.count(), 0)
        self.assertTrue(bool(d))
        self.assertEqual(d.count(), 1)
        self.assertEqual(ap.executableName(), "mitsuba")

    def test02_parameterValue(self):
        ap = ArgParser()
        a = ap.add("-a", True)
        ap.parse(['mitsuba', '-a', 'abc', '-axyz'])
        self.assertTrue(bool(a))
        self.assertEqual(a.count(), 2)
        self.assertEqual(a.asString(), 'abc')
        with self.assertRaises(Exception):
            a.asInt()
        self.assertEqual(a.next().asString(), 'xyz')

    def test03_parameterMissing(self):
        ap = ArgParser()
        ap.add("-a", True)
        with self.assertRaises(Exception):
            ap.parse(['mitsuba', '-a'])

    def test04_parameterFloatAndExtraArgs(self):
        ap = ArgParser()
        f = ap.add("-f", True)
        other = ap.add("", True)
        ap.parse(['mitsuba', 'other', '-f0.25', 'arguments'])
        self.assertTrue(bool(f))
        self.assertEqual(f.asFloat(), 0.25)
        self.assertTrue(bool(other))
        self.assertEqual(other.count(), 2)

    def test05_longParametersFailure(self):
        ap = ArgParser()
        i = ap.add("--int", True)
        with self.assertRaises(Exception):
            ap.parse(['mitsuba', '--int1'])
        with self.assertRaises(Exception):
            ap.parse(['mitsuba', '--int'])
        ap.parse(['mitsuba', '--int', '34'])
        self.assertEqual(i.asInt(), 34)

if __name__ == '__main__':
    unittest.main()
