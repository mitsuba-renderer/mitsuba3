"""
These tests intend to thoroughly check the accessors, slicing and assignment
mechanics for data structures that support dynamically-sized vectorization.
"""

import pytest
import drjit as dr
import mitsuba as mi
import numpy as np


def test01_position_sample_construction_single(variant_scalar_rgb):
    record = mi.PositionSample3f()
    record.p = [0, 42, 0]
    record.n = [0, 0, 0.4]
    record.uv = [1, 2]
    record.pdf = 0.002
    record.delta = False
    record.time = 0
    expected = """PositionSample[
  p=[0, 42, 0],
  n=[0, 0, 0.4],
  uv=[1, 2],
  time=0,
  pdf=0.002,
  delta=0
]"""
    assert str(record) == expected.strip()

    # SurfaceInteraction constructor
    si = mi.SurfaceInteraction3f()
    si.time = 50.5
    si.uv = [0.1, 0.8]
    record = mi.PositionSample3f(si)
    assert record.time == si.time \
           and dr.all(record.uv == si.uv) \
           and record.pdf == 0.0


def test02_position_sample_construction_vec(variants_vec_backends_once):
    n_records = 5

    records = dr.zeros(mi.PositionSample3f, n_records)
    records.p = np.array([
        [1.0, 0.9, 0.7, 1.2, 1.5],
        [1.0, 0.9, 0.7, 1.5, 1.5],
        [1.0, 0.9, 0.7, 1.1, 1.5]])
    records.time = [0.0, 0.5, 0.7, 1.0, 1.5]

    expected = """PositionSample[
  p=[[1, 1, 1],
     [0.9, 0.9, 0.9],
     [0.7, 0.7, 0.7],
     [1.2, 1.5, 1.1],
     [1.5, 1.5, 1.5]],
  n=[[0, 0, 0],
     [0, 0, 0],
     [0, 0, 0],
     [0, 0, 0],
     [0, 0, 0]],
  uv=[[0, 0],
      [0, 0],
      [0, 0],
      [0, 0],
      [0, 0]],
  time=[0, 0.5, 0.7, 1, 1.5],
  pdf=[0, 0, 0, 0, 0],
  delta=[0, 0, 0, 0, 0]
]"""

    assert str(records) == expected
    # SurfaceInteraction constructor
    si = dr.zeros(mi.SurfaceInteraction3f, n_records)
    si.time = [0.0, 0.5, 0.7, 1.0, 1.5]
    records = mi.PositionSample3f(si)
    assert dr.all(records.time == si.time)


def test04_direction_sample_construction_single(variants_vec_backends_once):
    # Default constructor
    record = mi.DirectionSample3f()
    record.p = [1, 2, 3]
    record.n = [4, 5, 6]
    record.uv = [7, 8]
    record.d = [0, 42, -1]
    record.pdf = 0.002
    record.dist = 0.13

    assert dr.allclose(record.d, [0, 42, -1])
    assert str(record) == """DirectionSample[
  p=[[1, 2, 3]],
  n=[[4, 5, 6]],
  uv=[[7, 8]],
  time=[],
  pdf=[0.002],
  delta=[],
  d=[[0, 42, -1]],
  dist=[0.13],
  emitter=[0x0]
]"""

    # Construct from two interactions: ds.d should start from the reference its.
    shape = mi.load_dict({
        'type': 'sphere',
        'emitter': { 'type' : 'area' }
    })
    its = dr.zeros(mi.SurfaceInteraction3f)
    its.p = [20, 3, 40.02]
    its.t = 1
    its.shape = shape
    ref = dr.zeros(mi.Interaction3f)
    ref.p = [1.6, -2, 35]
    record = mi.DirectionSample3f(None, its, ref)
    d = (its.p - ref.p) / dr.norm(its.p - ref.p)
    assert dr.allclose(record.d, d)


def test05_direction_sample_construction_vec(variants_vec_backends_once, np_rng):
    refs = np.array([[0.0, 0.5, 0.7],
                     [1.0, 1.5, 0.2],
                     [-1.3, 0.0, 99.1]])
    its = refs + np_rng.uniform(size=refs.shape)
    directions = its - refs
    directions /= np.expand_dims(np.linalg.norm(directions, axis=1), 0).T

    pdfs = [0.99, 1.0, 0.05]

    records_batch = dr.zeros(mi.DirectionSample3f, len(pdfs))
    records_batch.p = its
    records_batch.d = directions
    records_batch.pdf = pdfs

    # Switch back to scalar variant
    mi.set_variant('scalar_rgb')

    for i in range(len(pdfs)):
        it = mi.SurfaceInteraction3f()
        it.p = its[i, :]
        # Needs to be a "valid" (surface) interaction, otherwise interaction
        # will be assumed to have happened on an environment emitter.
        it.t = 0.1
        ref = dr.zeros(mi.Interaction3f)
        ref.p = refs[i, :]

    assert dr.allclose(records_batch.p, its)
    assert dr.allclose(records_batch.d, directions)
    assert dr.allclose(records_batch.pdf, pdfs)

