"""
These tests intend to thoroughly check the accessors, slicing and assigmnent
mechanics for data structures that support dynamically-sized vectorization.
"""

import mitsuba
import numpy as np
import pytest

from mitsuba.render import EArea, ELength, EDiscrete, ESolidAngle
from mitsuba.render import PositionSample3f, PositionSample3fX
from mitsuba.render import DirectionSample3f, DirectionSample3fX
from mitsuba.render import DirectSample3f, DirectSample3fX

def test01_position_sample_construction_single():
    record = PositionSample3f()
    record.p = [0, 42, 0]
    assert np.array_equal(record.p, [0, 42, 0])
    record.n = [0, 0, 0.4]
    record.uv = [1, 2]
    record.pdf = 0.002
    record.measure = EArea
    assert str(record) == """PositionSample3f[
  p = [0, 42, 0],
  n = [0, 0, 0.4],
  uv = [1, 2],
  time = 0,
  pdf = 0.002,
  measure = area,
  object = nullptr
]"""

    # Time constructor
    record = PositionSample3f(50.5)
    assert record.time == 50.5 and np.array_equal(record.uv, [0, 0])

    # TODO: test (Intersection, Measure) constructor

def test02_position_sample_construction_dynamic():
    n_records = 5
    records = PositionSample3fX(n_records)
    # Set properties for all records at once (SoA style)
    records.n = np.zeros(shape=(n_records, 3))
    records.uv = np.zeros(shape=(n_records, 2))
    records.pdf = np.zeros(shape=(n_records))
    records.p = [[1.0, 1.0, 1.0], [0.9, 0.9, 0.9],
                [0.7, 0.7, 0.7], [1.2, 1.5, 1.1],
                [1.5, 1.5, 1.5]]
    records.time = [0.0, 0.5, 0.7, 1.0, 1.5]
    records.measure = [EArea, ELength, EDiscrete, ELength, EArea]
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
  measure = [area, length, discrete, length, area],
  object = [nullptr, nullptr, nullptr, nullptr, nullptr]
]"""

def test03_position_sample_construction_dynamic_slicing():
    n_records = 5
    records = PositionSample3fX(n_records)
    records.p = [[1.0, 1.0, 1.0], [0.9, 0.9, 0.9],
                [0.7, 0.7, 0.7], [1.2, 1.5, 1.1],
                [1.5, 1.5, 1.5]]
    records.time = [0.0, 0.5, 0.7, 1.0, 1.5]
    records.measure = [EArea, ELength, EDiscrete, ELength, EArea]

    # Each entry of a dynamic record is a standard record (single)
    assert type(records[3]) is PositionSample3f
    # Slicing allows us to get **a copy** of one of the entries
    one_record = records[2]
    assert (records[3].time == 1
            and np.allclose(records[3].p, [1.2, 1.5, 1.1])
            and records[3].measure == ELength
           )

    # Since it is a copy, changes are **not** carried out on the original entry
    records[3].time = 42.5
    assert records[3].time == 1

    # However, we can assign an entire record
    single = PositionSample3f(13.3)
    records[3] = single
    assert (np.allclose(records[3].time, 13.3)
            and np.allclose(records[3].time, single.time))


def test04_direction_sample_construction_single():
    # Default constructor
    record = DirectionSample3f()
    record.d = [0, 42, -1]
    record.pdf = 0.002
    record.measure = EArea

    assert np.allclose(record.d, [0, 42, -1])
    assert str(record) == """DirectionSample3f[
  d = [0, 42, -1],
  pdf = 0.002,
  measure = area
]"""

    # (Vector3, Measure) constructor (with default value)
    record = DirectionSample3f([3, 4, -5])
    assert np.allclose(record.d, [3, 4, -5])
    assert record.measure == ESolidAngle
    record = DirectionSample3f([3, 4, -5], EArea)
    assert record.measure == EArea

    # TODO: test (Intersection, Measure) constructor (with default value)

def test05_direction_sample_construction_dynamic_and_slicing():
    vectors = [[0.0, 0.5, 0.7],
               [1.0, 1.5, 0.2],
               [-1.3, 0.0, 99.1]]
    pdfs = [0.99, 1.0, 0.05]
    measures = [EArea, ESolidAngle, EDiscrete]

    records_batch = DirectionSample3fX(len(measures))
    records_batch.d = vectors
    records_batch.pdf = pdfs
    records_batch.measure = measures

    records_individual = DirectionSample3fX(len(measures))
    for i in range(len(measures)):
        r = DirectionSample3f(vectors[i], measures[i])
        r.pdf = pdfs[i]
        records_individual[i] = r

    assert np.allclose(records_batch.d, vectors)
    assert np.allclose(records_batch.d, records_individual.d)
    assert np.allclose(records_batch.pdf, pdfs)
    assert np.allclose(records_batch.pdf, records_individual.pdf)
    for i in range(len(measures)):
        assert records_batch.measure[i] == measures[i]
        assert records_batch.measure[i] == records_individual.measure[i]
    assert str(records_batch) == """DirectionSample3fX[
  d = [[0, 0.5, 0.7],
       [1, 1.5, 0.2],
       [-1.3, 0, 99.1]],
  pdf = [0.99, 1, 0.05],
  measure = [area, solidangle, discrete]
]"""

    # Slicing: get a copy of one of the entries
    single = records_individual[2]
    assert (np.allclose(single.d, vectors[2])
            and np.allclose(single.pdf, pdfs[2])
            and single.measure == single.measure)


def test06_direct_sample_construction_subclass():
    # DirectSample is a kind of PositionSample, but only if
    # the packet size matches.
    assert issubclass(DirectSample3f, PositionSample3f)
    assert issubclass(DirectSample3fX, PositionSample3fX)
    assert not issubclass(DirectSample3f, PositionSample3fX)
    assert not issubclass(DirectSample3fX, PositionSample3f)

def test07_direct_sample_construction_single():
    # Default constructor
    record = DirectSample3f()
    record.d = [1, 2, 3]
    record.pdf = 0.05    # A field from its parent class
    assert np.allclose(record.d, [1, 2, 3])
    assert np.allclose(record.pdf, 0.05)

    # (Point3, Float) constructor
    record = DirectSample3f([3, 2, 1], 0.03)
    assert np.allclose(record.ref_p, [3, 2, 1])
    assert np.allclose(record.ref_n, [0, 0, 0])
    assert np.allclose(record.time, 0.03)

    # TODO: test (Intersection) constructor

    # TODO: test (MediumSample) constructor

    # TODO: test (Ray3f, Intersection, Measure) constructor

def test08_direct_sample_construction_dynamic_and_slicing():
    ps     = [[0.0, 0.5, 0.7],   # From the parent class
              [1.0, 1.5, 0.2],
              [-1.3, 0.0, 99.1]]
    times  = [5.99, 0.1, -6.05]   # From the parent class
    ref_ps = [[0.0, 0.5, 0.7],
              [1.0, 1.5, 0.2],
              [-1.3, 0.0, 99.1]]
    ref_ns = [[0.0, 0.5, 0.7],
              [1.0, 1.5, 0.2],
              [-1.3, 0.0, 99.1]]
    dists  = [0.99, 1.0, 0.05]

    records_batch = DirectSample3fX(len(dists))
    records_batch.p = ps
    records_batch.time = times
    records_batch.ref_p = ref_ps
    records_batch.ref_n = ref_ns
    records_batch.dist = dists

    records_individual = DirectSample3fX(len(dists))
    for i in range(len(dists)):
        r = DirectSample3f(ref_ps[i], times[i])
        r.p = ps[i]
        r.ref_n = ref_ns[i]
        r.dist = dists[i]
        records_individual[i] = r

    assert np.allclose(records_batch.p, ps)
    assert np.allclose(records_batch.p, records_individual.p)
    assert np.allclose(records_batch.time, times)
    assert np.allclose(records_batch.time, records_individual.time)
    assert np.allclose(records_batch.ref_p, ref_ps)
    assert np.allclose(records_batch.ref_p, records_individual.ref_p)
    assert np.allclose(records_batch.ref_n, ref_ns)
    assert np.allclose(records_batch.ref_n, records_individual.ref_n)
    assert np.allclose(records_batch.dist, dists)
    assert np.allclose(records_batch.dist, records_individual.dist)
