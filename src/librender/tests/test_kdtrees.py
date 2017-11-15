from mitsuba.core import Properties, Ray3f, Ray3fX
from mitsuba.render import ShapeKDTree, Mesh
from mitsuba.core.xml import load_string
from mitsuba.render.rt import *
from mitsuba.test.util import fresolver_append_path

import math
import numpy as np
import os
import pytest


from .mesh_generation import *

def compare_results(resA, resB, atol=0):
    assert np.all(resA[0] == resB[0])

    (ma, a), (mb, b) = resA, resB
    if np.isscalar(a):
        a = np.array([a])
        ma = np.array([resA[0]])
    if np.isscalar(b):
        b = np.array([b])
        mb = np.array([resB[0]])

    # The `t` return value is only meaningful when there was an intersection
    assert np.allclose(a[ma], b[mb], atol=atol), \
           '\n' + '\n'.join([str(a), str(b)])

# ------------------------------------------------------------------------------

@fresolver_append_path
def test01_bunny_pbrt():
    # Create kdtree
    kdtree = load_string("""
        <scene version="0.5.0">
            <shape type="ply">
                <string name="filename" value="resources/data/ply/bunny_lowres.ply"/>
            </shape>
        </scene>
    """).kdtree()

    N = 128

    # Trace rays
    (time_dummy, count_dummy)   = rt_dummy_planar_morton_scalar(kdtree,  N)
    (time_pbrt, count_pbrt)     = rt_pbrt_planar_morton_scalar(kdtree,   N)
    (time_pbrt_v, count_pbrt_v) = rt_pbrt_planar_morton_packet(kdtree,   N)
    (time_havran, count_havran) = rt_havran_planar_morton_scalar(kdtree, N)

    assert count_dummy > 0
    assert count_dummy == count_pbrt
    assert count_dummy == count_pbrt_v
    assert count_dummy == count_havran


def test02_depth_scalar_stairs():
    N_steps = 20

    # Create kdtree
    kdtree = ShapeKDTree(Properties())
    kdtree.add_shape(create_stairs(N_steps))
    kdtree.build()

    N = 128
    invN = 1.0 / (N-1)

    for x in range(N-1):
        for y in range(N-1):
            o = np.array([x * invN,  y * invN,  2])
            d = np.array([0,  0,  -1])
            r = Ray3f(o, d)

            res_dummy  = kdtree.ray_intersect_dummy_scalar(r, 0., 100.)
            res_pbrt   = kdtree.ray_intersect_pbrt_scalar(r, 0., 100.)
            res_havran = kdtree.ray_intersect_havran_scalar(r, 0., 100.)

            step_id = math.floor((y * invN) * N_steps)


            compare_results(res_dummy, (True, 2 - (step_id / N_steps)), atol=1e-9)
            compare_results(res_dummy, res_pbrt)
            compare_results(res_dummy, res_havran)


@fresolver_append_path
def test03_depth_scalar_bunny():
    kdtree = load_string("""
        <scene version="0.5.0">
            <shape type="ply">
                <string name="filename" value="resources/data/ply/bunny_lowres.ply"/>
            </shape>
        </scene>
    """).kdtree()

    b = kdtree.bbox()

    N = 100
    invN = 1.0 / (N - 1)

    for x in range(N):
        for y in range(N):
            o = np.array([b.min[0] * (1 - x * invN) + b.max[0] * x * invN, b.min[1] * (1 - y * invN) + b.max[1] * y * invN, b.min[2]])
            d = np.array([0,  0,  1])
            r = Ray3f(o, d)

            res_dummy  = kdtree.ray_intersect_dummy_scalar(r, 0., 100.)
            res_pbrt   = kdtree.ray_intersect_pbrt_scalar(r, 0., 100.)
            res_havran = kdtree.ray_intersect_havran_scalar(r, 0., 100.)

            compare_results(res_dummy, res_pbrt)
            compare_results(res_dummy, res_havran)


def test04_depth_packet_stairs():
    # Create kdtree
    kdtree = ShapeKDTree(Properties())
    kdtree.add_shape(create_stairs(11))
    kdtree.build()
    N = 4
    invN = 1.0 / (N - 1)
    mint = np.full(N*N, 0.)
    maxt = np.full(N*N, 100.)
    rays = Ray3fX(N*N)

    for x in range(N):
        for y in range(N):
            o = np.array([x * invN,  y * invN,  2])
            d = np.array([0,  0,  -1])
            rays[x*N + y] =  Ray3f(o, d)

    res_dummy = kdtree.ray_intersect_dummy_packet(rays, mint, maxt)
    res_pbrt  = kdtree.ray_intersect_pbrt_packet(rays, mint, maxt)

    # TODO: spot-check for errors
    compare_results(res_dummy, res_pbrt, atol=1e-4)
