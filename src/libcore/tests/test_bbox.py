import enoki as ek
import pytest
import mitsuba


def test01_basics(variant_scalar_rgb):
    from mitsuba.core import BoundingBox3f as BBox

    bbox1 = BBox()
    bbox2 = BBox([0, 1, 2])
    bbox3 = BBox([1, 2, 3], [2, 3, 5])
    assert bbox1 != bbox2
    assert bbox2 == bbox2
    assert not bbox1.valid()
    assert not bbox1.collapsed()
    assert bbox2.valid()
    assert bbox2.collapsed()
    assert bbox3.valid()
    assert not bbox3.collapsed()
    assert bbox2.volume() == 0
    assert bbox2.major_axis() == 0
    assert bbox2.minor_axis() == 0
    assert (bbox2.center() == [0, 1, 2])
    assert (bbox2.extents() == [0, 0, 0])
    assert bbox2.surface_area() == 0
    assert bbox3.volume() == 2
    assert bbox3.surface_area() == 10
    assert (bbox3.center() == [1.5, 2.5, 4])
    assert (bbox3.extents() == [1, 1, 2])
    assert bbox3.major_axis() == 2
    assert bbox3.minor_axis() == 0
    assert (bbox3.min == [1, 2, 3])
    assert (bbox3.max == [2, 3, 5])
    assert (bbox3.corner(0) == [1, 2, 3])
    assert (bbox3.corner(1) == [2, 2, 3])
    assert (bbox3.corner(2) == [1, 3, 3])
    assert (bbox3.corner(3) == [2, 3, 3])
    assert (bbox3.corner(4) == [1, 2, 5])
    assert (bbox3.corner(5) == [2, 2, 5])
    assert (bbox3.corner(6) == [1, 3, 5])
    assert (bbox3.corner(7) == [2, 3, 5])
    assert str(bbox1) == "BoundingBox3f[invalid]"
    assert str(bbox3) == "BoundingBox3f[\n  min = [1, 2, 3],\n" \
                         "  max = [2, 3, 5]\n]"
    bbox4 = BBox.merge(bbox2, bbox3)
    assert (bbox4.min == [0, 1, 2])
    assert (bbox4.max == [2, 3, 5])
    bbox3.reset()
    assert not bbox3.valid()
    bbox3.expand([0, 0, 0])
    assert BBox([0, 0, 0]) == bbox3
    bbox3.expand([1, 1, 1])
    assert BBox([0, 0, 0], [1, 1, 1]) == bbox3
    bbox3.expand(BBox([-1, -2, -3], [4, 5, 6]))
    assert BBox([-1, -2, -3], [4, 5, 6]) == bbox3
    bbox3.clip(bbox2)
    assert bbox2 == bbox3


def test02_contains_variants(variant_scalar_rgb):
    from mitsuba.core import BoundingBox3f as BBox

    bbox = BBox([1, 2, 3], [2, 3, 5])
    assert bbox.contains([1.5, 2.5, 3.5])
    assert bbox.contains([1.5, 2.5, 3.5], strict=True)
    assert bbox.contains([1, 2, 3])
    assert not bbox.contains([1, 2, 3], strict=True)
    assert bbox.contains(BBox([1.5, 2.5, 3.5], [1.8, 2.8, 3.8]))
    assert bbox.contains(BBox([1.5, 2.5, 3.5], [1.8, 2.8, 3.8]),
                         strict=True)
    assert bbox.contains(BBox([1, 2, 3], [1.8, 2.8, 3.8]))
    assert not bbox.contains(BBox([1, 2, 3], [1.8, 2.8, 3.8]),
                             strict=True)
    assert bbox.overlaps(BBox([0, 1, 2], [1.5, 2.5, 3.5]))
    assert bbox.overlaps(BBox([0, 1, 2], [1.5, 2.5, 3.5]),
                         strict=True)
    assert bbox.overlaps(BBox([0, 1, 2], [1, 2, 3]))
    assert not bbox.overlaps(BBox([0, 1, 2], [1, 2, 3]),
                             strict=True)


def test03_distance(variant_scalar_rgb):
    from mitsuba.core import BoundingBox3f as BBox

    assert BBox([1, 2, 3], [2, 3, 5]).distance(
        BBox([4, 2, 3], [5, 3, 5])) == 2

    assert ek.abs(BBox([1, 2, 3], [2, 3, 5]).distance(
        BBox([3, 4, 6], [7, 7, 7])) - ek.sqrt(3)) < 1e-6

    assert BBox([1, 2, 3], [2, 3, 5]).distance(
        BBox([1.1, 2.2, 3.3], [1.8, 2.8, 3.8])) == 0

    assert BBox([1, 2, 3], [2, 3, 5]).distance([1.5, 2.5, 3.5]) == 0

    assert BBox([1, 2, 3], [2, 3, 5]).distance([3, 2.5, 3.5]) == 1
