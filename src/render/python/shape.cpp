#include <mitsuba/render/shape.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/python/python.h>

MI_PY_EXPORT(DiscontinuityFlags) {
    auto disc_flags = nb::enum_<DiscontinuityFlags>(m, "DiscontinuityFlags", nb::is_arithmetic(), D(DiscontinuityFlags))
        .def_value(DiscontinuityFlags, Empty)
        .def_value(DiscontinuityFlags, PerimeterType)
        .def_value(DiscontinuityFlags, InteriorType)
        .def_value(DiscontinuityFlags, DirectionLune)
        .def_value(DiscontinuityFlags, DirectionSphere)
        .def_value(DiscontinuityFlags, HeuristicWalk)
        .def_value(DiscontinuityFlags, AllTypes);

    auto shape_types = nb::enum_<ShapeType>(m, "ShapeType", nb::is_arithmetic(), D(ShapeType))
        .def_value(ShapeType, Mesh)
        .def_value(ShapeType, BSplineCurve)
        .def_value(ShapeType, Cylinder)
        .def_value(ShapeType, Disk)
        .def_value(ShapeType, LinearCurve)
        .def_value(ShapeType, Rectangle)
        .def_value(ShapeType, SDFGrid)
        .def_value(ShapeType, Sphere)
        .def_value(ShapeType, Other);
}
