import unittest
from mitsuba.util import *


class UtilTest(unittest.TestCase):
    def test01_timeString(self):
        self.assertEqual(timeString(1, precise=True), '1ms')
        self.assertEqual(timeString(2010, precise=True), '2.01s')
        self.assertEqual(timeString(2*1000*60, precise=True), '2m')
        self.assertEqual(timeString(2*1000*60*60, precise=True), '2h')
        self.assertEqual(timeString(2*1000*60*60*24, precise=True), '2d')
        self.assertEqual(timeString(2*1000*60*60*24*7, precise=True), '2w')
        self.assertEqual(timeString(2*1000*60*60*24*7*52.1429, precise=True), '2y')

    def test01_memString(self):
        self.assertEqual(memString(2, precise=True), '2 B')
        self.assertEqual(memString(2*1024, precise=True), '2 KiB')
        self.assertEqual(memString(2*1024**2, precise=True), '2 MiB')
        self.assertEqual(memString(2*1024**3, precise=True), '2 GiB')
        self.assertEqual(memString(2*1024**4, precise=True), '2 TiB')
        self.assertEqual(memString(2*1024**5, precise=True), '2 PiB')
        self.assertEqual(memString(2*1024**6, precise=True), '2 EiB')

if __name__ == '__main__':
    unittest.main()
