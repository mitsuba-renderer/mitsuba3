from mitsuba.core import Thread, FileResolver, Struct, float_dtype, Properties
from mitsuba.core.xml import load_string
from mitsuba.render import Shape, Scene, Mesh, rtbench as rt
from mitsuba.test.util import fresolver_append_path

import numpy as np
import os

# print results in a formatted manner
def print_result(routine, samples, result):
    print("{:<50}{:<20}{:<20}{} hits".format(
        routine, "[{} samples]:".format(samples),
        "{} msecs".format(result[0]),result[1])
    )

def run_and_print(f, scene, N):
    print_result(f.__name__, N, f(scene, N))


@fresolver_append_path
def run_benchmark(name, N):
    scene = load_string("""
        <scene version="2.0.0">
            <shape type="ply">
                <string name="filename" value="resources/data/ply/{}.ply"/>
            </shape>
        </scene>
    """.format(name))

    print("\n===============================================")
    print(  "=== Benchmarks - {} x {} samples - {} ===".format(N, N, name))
    print(  "===============================================")

    print("\n --- planar & spherical ray tracing ---")
    run_and_print(rt.planar_independent_scalar, scene, N)
    run_and_print(rt.planar_morton_scalar, scene, N)
    run_and_print(rt.planar_independent_packet, scene, N)
    run_and_print(rt.planar_morton_packet, scene, N)
    print("\n")
    run_and_print(rt.spherical_independent_scalar, scene, N)
    run_and_print(rt.spherical_morton_scalar, scene, N)
    run_and_print(rt.spherical_independent_packet, scene, N)
    run_and_print(rt.spherical_morton_packet, scene, N)

    print("\n --- planar & spherical shadow ray tracing ---")
    run_and_print(rt.planar_independent_scalar_shadow, scene, N)
    run_and_print(rt.planar_morton_scalar_shadow, scene, N)
    run_and_print(rt.planar_independent_packet_shadow, scene, N)
    run_and_print(rt.planar_morton_packet_shadow, scene, N)
    print("\n")
    run_and_print(rt.spherical_independent_scalar_shadow, scene, N)
    run_and_print(rt.spherical_morton_scalar_shadow, scene, N)
    run_and_print(rt.spherical_independent_packet_shadow, scene, N)
    run_and_print(rt.spherical_morton_packet_shadow, scene, N)
    print("\n")


if __name__ == "__main__":
     run_benchmark('bunny',  512)
     run_benchmark('sphere', 512)
     run_benchmark('teapot', 512)
