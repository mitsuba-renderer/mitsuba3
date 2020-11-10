"""
These tests intend to thoroughly check the accessors, slicing and assigmnent
mechanics for data structures that support dynamically-sized vectorization.
"""

import mitsuba
import pytest
import enoki as ek
from enoki.scalar import ArrayXf as Float
import numpy as np


def test01_position_sample_construction_single(variant_scalar_rgb):
    from mitsuba.render import PositionSample3f, SurfaceInteraction3f

    record = PositionSample3f()
    record.p = [0, 42, 0]
    record.n = [0, 0, 0.4]
    record.uv = [1, 2]
    record.pdf = 0.002
    record.delta = False
    record.time = 0
    record.object = None
    assert str(record) == """PositionSample3f[
  p = [0, 42, 0],
  n = [0, 0, 0.4],
  uv = [1, 2],
  time = 0,
  pdf = 0.002,
  delta = 0,
  object = nullptr
]"""

    # SurfaceInteraction constructor
    si = SurfaceInteraction3f()
    si.time = 50.5
    si.uv = [0.1, 0.8]
    record = PositionSample3f(si)
    assert record.time == si.time \
           and ek.all(record.uv == si.uv) \
           and record.pdf == 0.0


def test02_position_sample_construction_vec(variants_vec_rgb):
    from mitsuba.render import PositionSample3f, SurfaceInteraction3f

    n_records = 5

    records = ek.zero(PositionSample3f, n_records)
    records.p = np.array([[1.0, 1.0, 1.0], [0.9, 0.9, 0.9], [0.7, 0.7, 0.7],
                 [1.2, 1.5, 1.1], [1.5, 1.5, 1.5]])
    records.time = [0.0, 0.5, 0.7, 1.0, 1.5]

    assert  """[
  p = [[1, 1, 1],
       [0.9, 0.9, 0.9],
       [0.7, 0.7, 0.7],
       [1.2, 1.5, 1.1],
       [1.5, 1.5, 1.5]],
  n = [[0, 0, 0],
       [0, 0, 0],
       [0, 0, 0],
       [0, 0, 0],
       [0, 0, 0]],
  uv = [[0, 0],
        [0, 0],
        [0, 0],
        [0, 0],
        [0, 0]],
  time = [0, 0.5, 0.7, 1, 1.5],
  pdf = [0, 0, 0, 0, 0],
  delta = [0, 0, 0, 0, 0],
  object = [0x0, 0x0, 0x0, 0x0, 0x0]
]""" in str(records)

    # SurfaceInteraction constructor
    si = ek.zero(SurfaceInteraction3f, n_records)
    si.time = [0.0, 0.5, 0.7, 1.0, 1.5]
    records = PositionSample3f(si)
    assert ek.all(records.time == si.time)


def test04_direction_sample_construction_single(variant_scalar_rgb):
    from mitsuba.render import Interaction3f, DirectionSample3f, SurfaceInteraction3f

    # Default constructor
    record = DirectionSample3f()
    record.p = [1, 2, 3]
    record.n = [4, 5, 6]
    record.uv = [7, 8]
    record.d = [0, 42, -1]
    record.pdf = 0.002
    record.dist = 0.13

    assert ek.allclose(record.d, [0, 42, -1])
    assert str(record) == """DirectionSample3f[
  p = [1, 2, 3],
  n = [4, 5, 6],
  uv = [7, 8],
  time = 0,
  pdf = 0.002,
  delta = 0,
  object = nullptr,
  d = [0, 42, -1],
  dist = 0.13
]"""

    # Construct from two interactions: ds.d should start from the reference its.
    its = ek.zero(SurfaceInteraction3f)
    its.p = [20, 3, 40.02]
    its.t = 1
    ref = ek.zero(Interaction3f)
    ref.p = [1.6, -2, 35]
    record = DirectionSample3f(its, ref)
    d = (its.p - ref.p) / ek.norm(its.p - ref.p)
    assert ek.allclose(record.d, d)


def test05_direction_sample_construction_vec(variants_vec_rgb):
    from mitsuba.render import DirectionSample3f, SurfaceInteraction3f
    import numpy as np

    np.random.seed(12345)
    refs = np.array([[0.0, 0.5, 0.7],
                     [1.0, 1.5, 0.2],
                     [-1.3, 0.0, 99.1]])
    its = refs + np.random.uniform(size=refs.shape)
    directions = its - refs
    directions /= np.expand_dims(np.linalg.norm(directions, axis=1), 0).T

    pdfs = [0.99, 1.0, 0.05]

    records_batch = ek.zero(DirectionSample3f, len(pdfs))
    records_batch.p = its
    records_batch.d = directions
    records_batch.pdf = pdfs

    # Switch back to scalar variant
    mitsuba.set_variant('scalar_rgb')
    from mitsuba.render import SurfaceInteraction3f, DirectionSample3f, Interaction3f

    for i in range(len(pdfs)):
        it = SurfaceInteraction3f()
        it.p = its[i, :]
        # Needs to be a "valid" (surface) interaction, otherwise interaction
        # will be assumed to have happened on an environment emitter.
        it.t = 0.1
        ref = ek.zero(Interaction3f)
        ref.p = refs[i, :]

    assert ek.allclose(records_batch.p, its)
    assert ek.allclose(records_batch.d, directions)
    assert ek.allclose(records_batch.pdf, pdfs)

