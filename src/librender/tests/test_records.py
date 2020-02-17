"""
These tests intend to thoroughly check the accessors, slicing and assigmnent
mechanics for data structures that support dynamically-sized vectorization.
"""

import mitsuba
import pytest
import enoki as ek
from enoki.dynamic import Float32 as Float
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

def test02_position_sample_construction_dynamic(variant_packet_rgb):
    from mitsuba.render import PositionSample3f, SurfaceInteraction3f

    n_records = 5

    records = PositionSample3f.zero(n_records)
    # Set properties for all records at once (SoA style)
    # records.n = np.zeros(shape=(n_records, 3))
    # records.pdf = np.zeros(shape=(n_records,))
    # records.uv = np.zeros(shape=(n_records, 2))
    # records.delta = np.zeros(shape=(n_records,), dtype=np.bool)
    records.p = np.array([[1.0, 1.0, 1.0], [0.9, 0.9, 0.9], [0.7, 0.7, 0.7],
                 [1.2, 1.5, 1.1], [1.5, 1.5, 1.5]])
    records.time = [0.0, 0.5, 0.7, 1.0, 1.5]
    assert str(records) == """PositionSample3fX[
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
  object = [nullptr, nullptr, nullptr, nullptr, nullptr]
]"""

    # SurfaceInteraction constructor
    si = SurfaceInteraction3f.zero(n_records)
    si.time = [0.0, 0.5, 0.7, 1.0, 1.5]
    records = PositionSample3f(si)
    assert ek.all(records.time == si.time)


def test03_position_sample_construction_dynamic_slicing(variant_packet_rgb):
    from mitsuba.render import PositionSample3f

    n_records = 5
    records = PositionSample3f.zero(n_records)
    records.p = np.array([[1.0, 1.0, 1.0], [0.9, 0.9, 0.9],
                [0.7, 0.7, 0.7], [1.2, 1.5, 1.1],
                [1.5, 1.5, 1.5]])
    records.time = [0.0, 0.5, 0.7, 1.0, 1.5]

    # Switch back to scalar variant
    mitsuba.set_variant('scalar_rgb')
    from mitsuba.render import PositionSample3f

    # Each entry of a dynamic record is a standard record (single)
    assert type(records[3]) is PositionSample3f

    # Slicing allows us to get **a copy** of one of the entries
    one_record = records[2]
    assert records[3].time == 1 \
           and ek.allclose(records[3].p, [1.2, 1.5, 1.1])
    # Warning!
    # Since slices create a copy, changes are **not** carried out in the original entry
    records[3].time = 42.5
    assert records[3].time == 1

    # However, assigning an entire record works as expected
    single = PositionSample3f()
    single.time = 13.3
    records[3] = single
    assert (ek.allclose(records[3].time, 13.3)
            and ek.allclose(records[3].time, single.time))


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
    its = SurfaceInteraction3f.zero()
    its.p = [20, 3, 40.02]
    its.t = 1
    ref = Interaction3f.zero()
    ref.p = [1.6, -2, 35]
    record = DirectionSample3f(its, ref)
    d = (its.p - ref.p) / ek.norm(its.p - ref.p)
    assert ek.allclose(record.d, d)

def test05_direction_sample_construction_dynamic_and_slicing(variant_packet_rgb):
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

    records_batch = DirectionSample3f.zero(len(pdfs))
    records_batch.p = its
    records_batch.d = directions
    records_batch.pdf = pdfs

    records_individual = DirectionSample3f.zero(len(pdfs))


    # Switch back to scalar variant
    mitsuba.set_variant('scalar_rgb')
    from mitsuba.render import SurfaceInteraction3f, DirectionSample3f, Interaction3f

    for i in range(len(pdfs)):
        it = SurfaceInteraction3f()
        it.p = its[i, :]
        # Needs to be a "valid" (surface) interaction, otherwise interaction
        # will be assumed to have happened on an environment emitter.
        it.t = 0.1
        ref = Interaction3f.zero()
        ref.p = refs[i, :]

        r = DirectionSample3f(it, ref)
        r.pdf = pdfs[i]
        records_individual[i] = r

    assert ek.allclose(records_batch.p, its)
    assert ek.allclose(records_batch.p, records_individual.p)
    assert ek.allclose(records_batch.d, directions)
    assert ek.allclose(records_batch.d, records_individual.d, atol=1e-6)
    assert ek.allclose(records_batch.pdf, pdfs)
    assert ek.allclose(records_batch.pdf, records_individual.pdf)

    # Slicing: get a copy of one of the entries
    single = records_individual[2]
    assert (ek.allclose(single.d, directions[2])
            and ek.allclose(single.pdf, pdfs[2]))
