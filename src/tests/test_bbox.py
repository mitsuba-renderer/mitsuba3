import unittest

from mitsuba import BoundingBox3f as BBox
import numpy as np


class BBoxTest(unittest.TestCase):
    def test01_basics(self):
        bbox1 = BBox()
        bbox2 = BBox([0, 1, 2])
        bbox3 = BBox([1, 2, 3], [2, 3, 5])
        self.assertTrue(bbox1 != bbox2)
        self.assertTrue(bbox2 == bbox2)
        self.assertFalse(bbox1.valid())
        self.assertFalse(bbox1.collapsed())
        self.assertTrue(bbox2.valid())
        self.assertTrue(bbox2.collapsed())
        self.assertTrue(bbox3.valid())
        self.assertFalse(bbox3.collapsed())
        self.assertEqual(bbox2.volume(), 0)
        self.assertEqual(bbox2.majorAxis(), 0)
        self.assertEqual(bbox2.minorAxis(), 0)
        self.assertTrue((bbox2.center() == [0, 1, 2]).all())
        self.assertTrue((bbox2.extents() == [0, 0, 0]).all())
        self.assertEqual(bbox2.surfaceArea(), 0)
        self.assertEqual(bbox3.volume(), 2)
        self.assertEqual(bbox3.surfaceArea(), 10)
        self.assertTrue((bbox3.center() == [1.5, 2.5, 4]).all())
        self.assertTrue((bbox3.extents() == [1, 1, 2]).all())
        self.assertEqual(bbox3.majorAxis(), 2)
        self.assertEqual(bbox3.minorAxis(), 0)
        self.assertTrue((bbox3.min == [1, 2, 3]).all())
        self.assertTrue((bbox3.max == [2, 3, 5]).all())
        self.assertTrue((bbox3.corner(0) == [1, 2, 3]).all())
        self.assertTrue((bbox3.corner(1) == [2, 2, 3]).all())
        self.assertTrue((bbox3.corner(2) == [1, 3, 3]).all())
        self.assertTrue((bbox3.corner(3) == [2, 3, 3]).all())
        self.assertTrue((bbox3.corner(4) == [1, 2, 5]).all())
        self.assertTrue((bbox3.corner(5) == [2, 2, 5]).all())
        self.assertTrue((bbox3.corner(6) == [1, 3, 5]).all())
        self.assertTrue((bbox3.corner(7) == [2, 3, 5]).all())
        self.assertEqual(str(bbox1), "BoundingBox[invalid]")
        self.assertEqual(str(bbox3), "BoundingBox[min = [1, 2, 3],"
                                     " max = [2, 3, 5]]")
        bbox4 = BBox.merge(bbox2, bbox3)
        self.assertTrue((bbox4.min == [0, 1, 2]).all())
        self.assertTrue((bbox4.max == [2, 3, 5]).all())
        bbox3.reset()
        self.assertFalse(bbox3.valid())
        bbox3.expand([0, 0, 0])
        self.assertEqual(BBox([0, 0, 0]), bbox3)
        bbox3.expand([1, 1, 1])
        self.assertEqual(BBox([0, 0, 0], [1, 1, 1]), bbox3)
        bbox3.expand(BBox([-1, -2, -3], [4, 5, 6]))
        self.assertEqual(BBox([-1, -2, -3], [4, 5, 6]), bbox3)
        bbox3.clip(bbox2)
        self.assertEqual(bbox2, bbox3)

    def test02_contains_variants(self):
        bbox = BBox([1, 2, 3], [2, 3, 5])
        self.assertTrue(bbox.contains([1.5, 2.5, 3.5]))
        self.assertTrue(bbox.contains([1.5, 2.5, 3.5], strict=True))
        self.assertTrue(bbox.contains([1, 2, 3]))
        self.assertFalse(bbox.contains([1, 2, 3], strict=True))
        self.assertTrue(bbox.contains(BBox([1.5, 2.5, 3.5], [1.8, 2.8, 3.8])))
        self.assertTrue(bbox.contains(BBox([1.5, 2.5, 3.5], [1.8, 2.8, 3.8]),
                                      strict=True))
        self.assertTrue(bbox.contains(BBox([1, 2, 3], [1.8, 2.8, 3.8])))
        self.assertFalse(bbox.contains(BBox([1, 2, 3], [1.8, 2.8, 3.8]),
                                       strict=True))
        self.assertTrue(bbox.overlaps(BBox([0, 1, 2], [1.5, 2.5, 3.5])))
        self.assertTrue(bbox.overlaps(BBox([0, 1, 2], [1.5, 2.5, 3.5]),
                                      strict=True))
        self.assertTrue(bbox.overlaps(BBox([0, 1, 2], [1, 2, 3])))
        self.assertFalse(bbox.overlaps(BBox([0, 1, 2], [1, 2, 3]),
                                       strict=True))

    def test03_distanceTo(self):
        self.assertEqual(
            BBox([1, 2, 3], [2, 3, 5]).distance(
                BBox([4, 2, 3], [5, 3, 5])), 2)
        self.assertAlmostEqual(
            BBox([1, 2, 3], [2, 3, 5]).distance(
                BBox([3, 4, 6], [7, 7, 7])), np.sqrt(3), places=7)
        self.assertEqual(
            BBox([1, 2, 3], [2, 3, 5]).distance(
                BBox([1.1, 2.2, 3.3], [1.8, 2.8, 3.8])), 0)
        self.assertEqual(
            BBox([1, 2, 3], [2, 3, 5]).distance([1.5, 2.5, 3.5]), 0)
        self.assertEqual(
            BBox([1, 2, 3], [2, 3, 5]).distance([3, 2.5, 3.5]), 1)
        self.assertAlmostEqual(
            BBox([1, 2, 3], [2, 3, 5]).distance([3, 4, 6]),
            np.sqrt(3), places=7)



if __name__ == '__main__':
    unittest.main()
