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
        .def_value(RayMask, Camera)
        .def_value(RayMask, All);
}
