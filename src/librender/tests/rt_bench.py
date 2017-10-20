from mitsuba.core import Thread, FileResolver, Struct, float_dtype, Properties
from mitsuba.render import ShapeKDTree, Mesh
from mitsuba.core.xml import load_string
from mitsuba.render.rt import *

import numpy as np
import os

def find_resource(fname):
    path = os.path.dirname(os.path.realpath(__file__))
    while True:
        full = os.path.join(path, fname)
        if os.path.exists(full):
            return full
        if path == '' or path == '/':
            raise Exception("find_resource(): could not find \"%s\"" % fname);
        path = os.path.dirname(path)


# print results in a formatted manner
def print_result(routine, samples, result):
    samples_str = "[" + str(samples) + " samples]:"
    print(routine.ljust(50) + samples_str.ljust(20) + (str(result[0]) + " msecs.").ljust(20) + str(result[1]) + " hits" )
    #print(routine.ljust(50) + samples_str.ljust(20) + str(result[0]) + " msecs.")

# parallel benchmarks
def rtbench_parallel_pbrt(kdtree, N):
    print_result("rt_pbrt_planar_morton_scalar", N, rt_pbrt_planar_morton_scalar(kdtree, N))

def rtbench_parallel_pbrt_v(kdtree, N):
    print_result("rt_pbrt_planar_morton_packet", N, rt_pbrt_planar_morton_packet(kdtree, N))

def rtbench_parallel_havran(kdtree, N):
    print_result("rt_havran_planar_morton_scalar", N, rt_havran_planar_morton_scalar(kdtree, N))

def rtbench_parallel_dummy(kdtree, N):
    print_result("rt_dummy_planar_morton_scalar", N, rt_dummy_planar_morton_scalar(kdtree, N))

def rtbench_parallel_dummy_v(kdtree, N):
    print_result("rt_dummy_planar_morton_packet", N, rt_dummy_planar_morton_packet(kdtree, N))


# parallel shadow benchmarks
def rtbench_parallel_pbrt_shadow(kdtree, N):
    print_result("rt_pbrt_planar_morton_scalar_shadow", N, rt_pbrt_planar_morton_scalar_shadow(kdtree, N))

def rtbench_parallel_pbrt_shadow_v(kdtree, N):
    print_result("rt_pbrt_planar_morton_packet_shadow", N, rt_pbrt_planar_morton_packet_shadow(kdtree, N))

def rtbench_parallel_havran_shadow(kdtree, N):
    print_result("rt_havran_planar_morton_scalar_shadow", N, rt_havran_planar_morton_scalar_shadow(kdtree, N))

 # planar independent benchmarks
def rtbench_planar_independent_pbrt(kdtree, N):
    print_result("rt_pbrt_planar_independent_scalar", N, rt_pbrt_planar_independent_scalar(kdtree, N))

def rtbench_planar_independent_pbrt_v(kdtree, N):
    print_result("rt_pbrt_planar_independent_packet", N, rt_pbrt_planar_independent_packet(kdtree, N))

def rtbench_planar_independent_havran(kdtree, N):
    print_result("rt_havran_planar_independent_scalar", N, rt_havran_planar_independent_scalar(kdtree, N))


# planar independent shadow benchmarks
def rtbench_planar_independent_pbrt_shadow(kdtree, N):
    print_result("rt_pbrt_planar_independent_scalar_shadow", N, rt_pbrt_planar_independent_scalar_shadow(kdtree, N))

def rtbench_planar_independent_pbrt_shadow_v(kdtree, N):
    print_result("rt_pbrt_planar_independent_packet_shadow", N, rt_pbrt_planar_independent_packet_shadow(kdtree, N))

def rtbench_planar_independent_havran_shadow(kdtree, N):
    print_result("rt_havran_planar_independent_scalar_shadow", N, rt_havran_planar_independent_scalar_shadow(kdtree, N))

# spherical morton benchmarks
def rtbench_spherical_pbrt(kdtree, N):
    print_result("rt_pbrt_spherical_morton_scalar", N, rt_pbrt_spherical_morton_scalar(kdtree, N))

def rtbench_spherical_pbrt_v(kdtree, N):
    print_result("rt_pbrt_spherical_morton_packet", N, rt_pbrt_spherical_morton_packet(kdtree, N))

def rtbench_spherical_havran(kdtree, N):
    print_result("rt_havran_spherical_morton_scalar", N, rt_havran_spherical_morton_scalar(kdtree, N))

# spherical morton shadow benchmarks
def rtbench_spherical_pbrt_shadow(kdtree, N):
    print_result("rt_pbrt_spherical_morton_scalar_shadow", N, rt_pbrt_spherical_morton_scalar_shadow(kdtree, N))

def rtbench_spherical_pbrt_shadow_v(kdtree, N):
    print_result("rt_pbrt_spherical_morton_packet_shadow", N, rt_pbrt_spherical_morton_packet_shadow(kdtree, N))

def rtbench_spherical_havran_shadow(kdtree, N):
    print_result("rt_havran_spherical_morton_scalar_shadow", N, rt_havran_spherical_morton_scalar_shadow(kdtree, N))


# spherical independent benchmarks
def rtbench_spherical_independent_pbrt(kdtree, N):
    print_result("rt_pbrt_spherical_independent_scalar", N, rt_pbrt_spherical_independent_scalar(kdtree, N))

def rtbench_spherical_independent_pbrt_v(kdtree, N):
    print_result("rt_pbrt_spherical_independent_packet", N, rt_pbrt_spherical_independent_packet(kdtree, N))

def rtbench_spherical_independent_havran(kdtree, N):
    print_result("rt_havran_spherical_independent_scalar", N, rt_havran_spherical_independent_scalar(kdtree, N))

# spherical independent shadow benchmarks
def rtbench_spherical_independent_pbrt_shadow(kdtree, N):
    print_result("rt_pbrt_spherical_independent_scalar_shadow", N, rt_pbrt_spherical_independent_scalar_shadow(kdtree, N))

def rtbench_spherical_independent_pbrt_shadow_v(kdtree, N):
    print_result("rt_pbrt_spherical_independent_packet_shadow", N, rt_pbrt_spherical_independent_packet_shadow(kdtree, N))

def rtbench_spherical_independent_havran_shadow(kdtree, N):
    print_result("rt_havran_spherical_independent_scalar_shadow", N, rt_havran_spherical_independent_scalar_shadow(kdtree, N))




def run_benchmark(name, N):
    kdtree = load_string("""
        <scene version="0.5.0">
            <shape type="ply">
                <string name="filename" value=\"""" + find_resource('resources/data/ply/' + name + '.ply') + """"/>
            </shape>
        </scene>
    """).kdtree()

    print("\n===============================================")
    print(  "=== Benchmarks - " + str(N) + "x" + str(N) + " samples - " + name + " ===")
    print(  "===============================================")

    print("\n --- parallel morton benchmarks ---")
    rtbench_parallel_pbrt(kdtree, N)
    rtbench_parallel_pbrt_v(kdtree, N)
    rtbench_parallel_havran(kdtree, N)

    print("\n --- parallel morton shadow benchmarks ---")
    rtbench_parallel_pbrt_shadow(kdtree, N)
    rtbench_parallel_pbrt_shadow_v(kdtree, N)
    rtbench_parallel_havran_shadow(kdtree, N)

    print("\n --- planar independent benchmarks ---")
    rtbench_planar_independent_pbrt(kdtree, N)
    rtbench_planar_independent_pbrt_v(kdtree, N)
    rtbench_planar_independent_havran(kdtree, N)

    print("\n --- planar independent shadow benchmarks ---")
    rtbench_planar_independent_pbrt_shadow(kdtree, N)
    rtbench_planar_independent_pbrt_shadow_v(kdtree, N)
    rtbench_planar_independent_havran_shadow(kdtree, N)

    print("\n --- spherical morton benchmarks ---")
    rtbench_spherical_pbrt(kdtree, N)
    rtbench_spherical_pbrt_v(kdtree, N)
    rtbench_spherical_havran(kdtree, N)

    print("\n --- spherical morton shadow benchmarks ---")
    rtbench_spherical_pbrt_shadow(kdtree, N)
    rtbench_spherical_pbrt_shadow_v(kdtree, N)
    rtbench_spherical_havran_shadow(kdtree, N)

    print("\n --- spherical independent benchmarks ---")
    rtbench_spherical_independent_pbrt(kdtree, N)
    rtbench_spherical_independent_pbrt_v(kdtree, N)
    rtbench_spherical_independent_havran(kdtree, N)

    print("\n --- spherical independent shadow benchmarks ---")
    rtbench_spherical_independent_pbrt_shadow(kdtree, N)
    rtbench_spherical_independent_pbrt_shadow_v(kdtree, N)
    rtbench_spherical_independent_havran_shadow(kdtree, N)
    print("\n")


if __name__ == "__main__":
     run_benchmark('bunny',  512)
     run_benchmark('sphere', 512)
     run_benchmark('teapot', 512)
