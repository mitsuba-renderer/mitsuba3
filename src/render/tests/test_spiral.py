import pytest
import drjit as dr
import mitsuba as mi
import numpy as np

from drjit.scalar import ArrayXf as Float


def make_film(width = 156, height = 232):
    f = mi.load_dict({
        "type" : "hdrfilm",
        "width" : width,
        "height" : height
    })
    assert f is not None
    assert dr.all(f.size() == [width, height])
    return f


def extract_blocks(spiral, max_blocks = 1000):
    blocks = []
    b = spiral.next_block()

    while np.prod(b[1]) > 0:
        blocks.append(b)
        b = spiral.next_block()

        assert len(blocks) <= max_blocks,\
               "Too many blocks produced, implementation is probably wrong."
    return blocks


def check_first_blocks(blocks, expected, n_total = None):
    n_total = n_total or len(expected)
    assert len(blocks) == n_total
    for i in range(len(expected)):
        assert dr.all(blocks[i][0] == expected[i][0])
        assert dr.all(blocks[i][1] == expected[i][1])


def test01_construct(variant_scalar_rgb):
    film = make_film()
    s = mi.Spiral(film.size(), film.crop_offset())
    assert s is not None
    assert s.max_block_size() == 32


def test02_small_film(variant_scalar_rgb):
    f = make_film(15, 12)
    s = mi.Spiral(f.size(), f.crop_offset())

    (bo, bs, bi) = s.next_block()
    assert dr.all(bo == [0, 0])
    assert dr.all(bs == [15, 12])
    assert dr.all(bi == 0)
    # The whole image is covered by a single block
    assert dr.all(s.next_block()[1] == 0)


def test03_normal_film(variant_scalar_rgb):
    # Check the first few blocks' size and location
    center = np.array([160, 160])
    w = 32
    expected = [
        [center               , (w, w)], [center + [ 1*w,  0*w], (w, w)],
        [center + [ 1*w,  1*w], (w, w)], [center + [ 0*w,  1*w], (w, w)],
        [center + [-1*w,  1*w], (w, w)], [center + [-1*w,  0*w], (w, w)],
        [center + [-1*w, -1*w], (w, w)], [center + [ 0*w, -1*w], (w, w)],
        [center + [ 1*w, -1*w], (w, w)], [center + [ 2*w, -1*w], (w, w)],
        [center + [ 2*w,  0*w], (w, w)], [center + [ 2*w,  1*w], (w, w)]
    ]

    f = make_film(318, 322)
    s = mi.Spiral(f.size(), f.crop_offset())

    check_first_blocks(extract_blocks(s), expected, n_total=110)
    # Resetting and re-querying the blocks should yield the exact same results.
    s.reset()
    check_first_blocks(extract_blocks(s), expected, n_total=110)
