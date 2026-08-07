#include <mitsuba/render/dedge.h>
#include <mitsuba/python/python.h>

MI_PY_EXPORT(VertexFlags) {
    nb::enum_<VertexFlags>(m, "VertexFlags", nb::is_arithmetic(),
                           D(VertexFlags))
        .def_value(VertexFlags, Boundary)
        .def_value(VertexFlags, NonManifoldEdge)
        .def_value(VertexFlags, NonManifoldVertex)
        .def_value(VertexFlags, InconsistentOrientation);
}
