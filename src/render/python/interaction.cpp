#include <mitsuba/render/interaction.h>
#include <mitsuba/python/python.h>

MI_PY_EXPORT(RayFlags) {
    auto e = nb::enum_<RayFlags>(m, "RayFlags", nb::is_arithmetic(), D(RayFlags))
        .def_value(RayFlags, Minimal)
        .def_value(RayFlags, Shading)
        .def_value(RayFlags, NormalPartials)
        .def_value(RayFlags, Default)
        .def_value(RayFlags, FollowShape)
        .def_value(RayFlags, DetachShape);

    nb::enum_<RayMask>(m, "RayMask", nb::is_arithmetic(), D(RayMask))
        .def_value(RayMask, Opaque)
        .def_value(RayMask, Null)
        .def_value(RayMask, Primary)
        .def_value(RayMask, Secondary)
        .def_value(RayMask, All);

    nb::enum_<ShapeVisibility>(m, "ShapeVisibility", D(ShapeVisibility))
        .def_value(ShapeVisibility, Hidden)
        .def_value(ShapeVisibility, Primary)
        .def_value(ShapeVisibility, Secondary)
        .def_value(ShapeVisibility, All);
}
