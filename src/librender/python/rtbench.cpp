#include <mitsuba/render/scene.h>
#include <mitsuba/render/kdtree.h>
#include <mitsuba/render/rtbench.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(rt) {
    MTS_PY_IMPORT_MODULE(rt, "mitsuba.render.rtbench");

#if !defined(MTS_USE_EMBREE)
    using namespace rtbench;
    rt.def("planar_morton_scalar", planar_morton_scalar, "kdtree"_a, "N"_a);
    rt.def("planar_morton_packet", planar_morton_packet, "kdtree"_a, "N"_a);
    rt.def("planar_morton_scalar_shadow", planar_morton_scalar_shadow, "kdtree"_a, "N"_a);
    rt.def("planar_morton_packet_shadow", planar_morton_packet_shadow, "kdtree"_a, "N"_a);
    rt.def("spherical_morton_scalar", spherical_morton_scalar, "kdtree"_a, "N"_a);
    rt.def("spherical_morton_packet", spherical_morton_packet, "kdtree"_a, "N"_a);
    rt.def("spherical_morton_scalar_shadow", spherical_morton_scalar_shadow, "kdtree"_a, "N"_a);
    rt.def("spherical_morton_packet_shadow", spherical_morton_packet_shadow, "kdtree"_a, "N"_a);
    rt.def("planar_independent_scalar", planar_independent_scalar, "kdtree"_a, "N"_a);
    rt.def("planar_independent_packet", planar_independent_packet, "kdtree"_a, "N"_a);
    rt.def("planar_independent_scalar_shadow", planar_independent_scalar_shadow, "kdtree"_a, "N"_a);
    rt.def("planar_independent_packet_shadow", planar_independent_packet_shadow, "kdtree"_a, "N"_a);
    rt.def("spherical_independent_scalar", spherical_independent_scalar, "kdtree"_a, "N"_a);
    rt.def("spherical_independent_packet", spherical_independent_packet, "kdtree"_a, "N"_a);
    rt.def("spherical_independent_scalar_shadow", spherical_independent_scalar_shadow, "kdtree"_a, "N"_a);
    rt.def("spherical_independent_packet_shadow", spherical_independent_packet_shadow, "kdtree"_a, "N"_a);

    rt.def("naive_planar_morton_scalar", naive_planar_morton_scalar, "kdtree"_a, "N"_a);
    rt.def("naive_planar_morton_packet", naive_planar_morton_packet, "kdtree"_a, "N"_a);
    rt.def("naive_planar_morton_scalar_shadow", naive_planar_morton_scalar_shadow, "kdtree"_a, "N"_a);
    rt.def("naive_planar_morton_packet_shadow", naive_planar_morton_packet_shadow, "kdtree"_a, "N"_a);
    rt.def("naive_spherical_morton_scalar", naive_spherical_morton_scalar, "kdtree"_a, "N"_a);
    rt.def("naive_spherical_morton_packet", naive_spherical_morton_packet, "kdtree"_a, "N"_a);
    rt.def("naive_spherical_morton_scalar_shadow", naive_spherical_morton_scalar_shadow, "kdtree"_a, "N"_a);
    rt.def("naive_spherical_morton_packet_shadow", naive_spherical_morton_packet_shadow, "kdtree"_a, "N"_a);
    rt.def("naive_planar_independent_scalar", naive_planar_independent_scalar, "kdtree"_a, "N"_a);
    rt.def("naive_planar_independent_packet", naive_planar_independent_packet, "kdtree"_a, "N"_a);
    rt.def("naive_planar_independent_scalar_shadow", naive_planar_independent_scalar_shadow, "kdtree"_a, "N"_a);
    rt.def("naive_planar_independent_packet_shadow", naive_planar_independent_packet_shadow, "kdtree"_a, "N"_a);
    rt.def("naive_spherical_independent_scalar", naive_spherical_independent_scalar, "kdtree"_a, "N"_a);
    rt.def("naive_spherical_independent_packet", naive_spherical_independent_packet, "kdtree"_a, "N"_a);
    rt.def("naive_spherical_independent_scalar_shadow", naive_spherical_independent_scalar_shadow, "kdtree"_a, "N"_a);
    rt.def("naive_spherical_independent_packet_shadow", naive_spherical_independent_packet_shadow, "kdtree"_a, "N"_a);
#endif
}
