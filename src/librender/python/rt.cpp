#include <mitsuba/render/scene.h>
#include <mitsuba/render/kdtree.h>
#include <mitsuba/render/rt.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(rt) {
    MTS_PY_IMPORT_MODULE(rt, "mitsuba.render.rt");

    rt.def("rt_pbrt_planar_morton_scalar", rt::rt_pbrt_planar_morton_scalar, "kdtree"_a, "N"_a);
    rt.def("rt_pbrt_planar_morton_packet", rt::rt_pbrt_planar_morton_packet, "kdtree"_a, "N"_a);
    rt.def("rt_pbrt_planar_morton_scalar_shadow", rt::rt_pbrt_planar_morton_scalar_shadow, "kdtree"_a, "N"_a);
    rt.def("rt_pbrt_planar_morton_packet_shadow", rt::rt_pbrt_planar_morton_packet_shadow, "kdtree"_a, "N"_a);
    rt.def("rt_pbrt_spherical_morton_scalar", rt::rt_pbrt_spherical_morton_scalar, "kdtree"_a, "N"_a);
    rt.def("rt_pbrt_spherical_morton_packet", rt::rt_pbrt_spherical_morton_packet, "kdtree"_a, "N"_a);
    rt.def("rt_pbrt_spherical_morton_scalar_shadow", rt::rt_pbrt_spherical_morton_scalar_shadow, "kdtree"_a, "N"_a);
    rt.def("rt_pbrt_spherical_morton_packet_shadow", rt::rt_pbrt_spherical_morton_packet_shadow, "kdtree"_a, "N"_a);
    rt.def("rt_pbrt_planar_independent_scalar", rt::rt_pbrt_planar_independent_scalar, "kdtree"_a, "N"_a);
    rt.def("rt_pbrt_planar_independent_packet", rt::rt_pbrt_planar_independent_packet, "kdtree"_a, "N"_a);
    rt.def("rt_pbrt_planar_independent_scalar_shadow", rt::rt_pbrt_planar_independent_scalar_shadow, "kdtree"_a, "N"_a);
    rt.def("rt_pbrt_planar_independent_packet_shadow", rt::rt_pbrt_planar_independent_packet_shadow, "kdtree"_a, "N"_a);
    rt.def("rt_pbrt_spherical_independent_scalar", rt::rt_pbrt_spherical_independent_scalar, "kdtree"_a, "N"_a);
    rt.def("rt_pbrt_spherical_independent_packet", rt::rt_pbrt_spherical_independent_packet, "kdtree"_a, "N"_a);
    rt.def("rt_pbrt_spherical_independent_scalar_shadow", rt::rt_pbrt_spherical_independent_scalar_shadow, "kdtree"_a, "N"_a);
    rt.def("rt_pbrt_spherical_independent_packet_shadow", rt::rt_pbrt_spherical_independent_packet_shadow, "kdtree"_a, "N"_a);

    rt.def("rt_havran_planar_morton_scalar", rt::rt_havran_planar_morton_scalar, "kdtree"_a, "N"_a);
    rt.def("rt_havran_planar_morton_scalar_shadow", rt::rt_havran_planar_morton_scalar_shadow, "kdtree"_a, "N"_a);
    rt.def("rt_havran_spherical_morton_scalar", rt::rt_havran_spherical_morton_scalar, "kdtree"_a, "N"_a);
    rt.def("rt_havran_spherical_morton_scalar_shadow", rt::rt_havran_spherical_morton_scalar_shadow, "kdtree"_a, "N"_a);
    rt.def("rt_havran_planar_independent_scalar", rt::rt_havran_planar_independent_scalar, "kdtree"_a, "N"_a);
    rt.def("rt_havran_planar_independent_scalar_shadow", rt::rt_havran_planar_independent_scalar_shadow, "kdtree"_a, "N"_a);
    rt.def("rt_havran_spherical_independent_scalar", rt::rt_havran_spherical_independent_scalar, "kdtree"_a, "N"_a);
    rt.def("rt_havran_spherical_independent_scalar_shadow", rt::rt_havran_spherical_independent_scalar_shadow, "kdtree"_a, "N"_a);

    rt.def("rt_dummy_planar_morton_scalar", rt::rt_dummy_planar_morton_scalar, "kdtree"_a, "N"_a);
    rt.def("rt_dummy_planar_morton_packet", rt::rt_dummy_planar_morton_packet, "kdtree"_a, "N"_a);
    rt.def("rt_dummy_planar_morton_scalar_shadow", rt::rt_dummy_planar_morton_scalar_shadow, "kdtree"_a, "N"_a);
    rt.def("rt_dummy_planar_morton_packet_shadow", rt::rt_dummy_planar_morton_packet_shadow, "kdtree"_a, "N"_a);
    rt.def("rt_dummy_spherical_morton_scalar", rt::rt_dummy_spherical_morton_scalar, "kdtree"_a, "N"_a);
    rt.def("rt_dummy_spherical_morton_packet", rt::rt_dummy_spherical_morton_packet, "kdtree"_a, "N"_a);
    rt.def("rt_dummy_spherical_morton_scalar_shadow", rt::rt_dummy_spherical_morton_scalar_shadow, "kdtree"_a, "N"_a);
    rt.def("rt_dummy_spherical_morton_packet_shadow", rt::rt_dummy_spherical_morton_packet_shadow, "kdtree"_a, "N"_a);
    rt.def("rt_dummy_planar_independent_scalar", rt::rt_dummy_planar_independent_scalar, "kdtree"_a, "N"_a);
    rt.def("rt_dummy_planar_independent_packet", rt::rt_dummy_planar_independent_packet, "kdtree"_a, "N"_a);
    rt.def("rt_dummy_planar_independent_scalar_shadow", rt::rt_dummy_planar_independent_scalar_shadow, "kdtree"_a, "N"_a);
    rt.def("rt_dummy_planar_independent_packet_shadow", rt::rt_dummy_planar_independent_packet_shadow, "kdtree"_a, "N"_a);
    rt.def("rt_dummy_spherical_independent_scalar", rt::rt_dummy_spherical_independent_scalar, "kdtree"_a, "N"_a);
    rt.def("rt_dummy_spherical_independent_packet", rt::rt_dummy_spherical_independent_packet, "kdtree"_a, "N"_a);
    rt.def("rt_dummy_spherical_independent_scalar_shadow", rt::rt_dummy_spherical_independent_scalar_shadow, "kdtree"_a, "N"_a);
    rt.def("rt_dummy_spherical_independent_packet_shadow", rt::rt_dummy_spherical_independent_packet_shadow, "kdtree"_a, "N"_a);
}
