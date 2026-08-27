#include <nanobind/nanobind.h> // Needs to be first, to get `ref<T>` caster
#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/render/dedge.h>
#include <mitsuba/python/python.h>
#include <drjit/python.h>

MI_PY_EXPORT(DirectedEdge) {
    MI_PY_IMPORT_TYPES(DirectedEdge)

    auto cls = MI_PY_CLASS(DirectedEdge, Object);
    cls.attr("Invalid") = DirectedEdge::Invalid;

    cls.def(nb::init<const DynamicBuffer<UInt32> &, uint32_t,
                     std::string_view, bool>(),
            "F"_a, "vertex_count"_a, "name"_a = "", "warn_defects"_a = true,
            D(DirectedEdge, DirectedEdge))

       .def_static("next", [](const UInt32 &e) { return DirectedEdge::next(e); },
                   "e"_a, D(DirectedEdge, next))
       .def_static("prev", [](const UInt32 &e) { return DirectedEdge::prev(e); },
                   "e"_a, D(DirectedEdge, prev))
       .def_static("face", [](const UInt32 &e) { return DirectedEdge::face(e); },
                   "e"_a, D(DirectedEdge, face))
       .def_static("corner", [](const UInt32 &e) { return DirectedEdge::corner(e); },
                   "e"_a, D(DirectedEdge, corner))

       .def_method(DirectedEdge, half_edge_count)
       .def_method(DirectedEdge, vertex_count)
       .def_method(DirectedEdge, name)

       .def("opposite", &DirectedEdge::opposite, "e"_a, "active"_a = true,
            D(DirectedEdge, opposite))
       .def("vertex_edge", &DirectedEdge::vertex_edge,
            "v"_a, "active"_a = true, D(DirectedEdge, vertex_edge))
       .def("vertex_valence", &DirectedEdge::vertex_valence,
            "v"_a, "active"_a = true, D(DirectedEdge, vertex_valence))
       .def("vertex_flags", &DirectedEdge::vertex_flags,
            "v"_a, "active"_a = true, D(DirectedEdge, vertex_flags))

       .def_method(DirectedEdge, E2E)
       .def_method(DirectedEdge, V2E)
       .def_method(DirectedEdge, valence)
       .def_method(DirectedEdge, flags)
       .def("flag_count", &DirectedEdge::flag_count, "flag"_a,
            D(DirectedEdge, flag_count));
}
