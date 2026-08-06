#include <mitsuba/render/shape.h>
#include <mitsuba/render/mesh_utils.h>
#include <mitsuba/render/dedge.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/python/python.h>

MI_PY_EXPORT(DiscontinuityFlags) {
    nb::enum_<Layout>(m, "Layout", nb::is_arithmetic(),
                            D(Layout))
        .def_value(Layout, Positions)
        .def_value(Layout, Normals)
        .def_value(Layout, Texcoords)
        .def_value(Layout, Tangents)
        .def_value(Layout, FaceBSDFs);

    nb::enum_<VertexFlags>(m, "VertexFlags", nb::is_arithmetic(),
                           D(VertexFlags))
        .def_value(VertexFlags, Boundary)
        .def_value(VertexFlags, NonManifoldEdge)
        .def_value(VertexFlags, NonManifoldVertex)
        .def_value(VertexFlags, InconsistentOrientation);

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
        .def_value(ShapeType, Rectangle)
        .def_value(ShapeType, BSplineCurve)
        .def_value(ShapeType, Cylinder)
        .def_value(ShapeType, Disk)
        .def_value(ShapeType, LinearCurve)
        .def_value(ShapeType, SDFGrid)
        .def_value(ShapeType, Sphere)
        .def_value(ShapeType, Ellipsoids)
        .def_value(ShapeType, EllipsoidsMesh)
        .def_value(ShapeType, Invalid);
}
