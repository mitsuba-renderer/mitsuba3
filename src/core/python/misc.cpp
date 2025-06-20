#include <mitsuba/core/util.h>
#include <mitsuba/python/python.h>
#include <nanobind/stl/string.h>

MI_PY_EXPORT(misc) {
    auto misc = m.def_submodule("misc", "Miscellaneous utility routines");

    misc.def("core_count", &util::core_count, D(util, core_count))
        .def("time_string", &util::time_string, D(util, time_string), "time"_a, "precise"_a = false)
        .def("mem_string", &util::mem_string, D(util, mem_string), "size"_a, "precise"_a = false)
        .def("trap_debugger", &util::trap_debugger, D(util, trap_debugger));

    // Bind util::Version struct
    nb::class_<util::Version>(m, "Version")
        .def(nb::init<>())
        .def(nb::init<int, int, int>(), "major"_a, "minor"_a, "patch"_a)
        .def(nb::init<std::string_view>(), "value"_a)
        .def_rw("major_version", &util::Version::major_version)
        .def_rw("minor_version", &util::Version::minor_version)
        .def_rw("patch_version", &util::Version::patch_version)
        .def("__eq__", &util::Version::operator==)
        .def("__ne__", &util::Version::operator!=)
        .def("__lt__", &util::Version::operator<)
        .def("__le__", &util::Version::operator<=)
        .def("__gt__", &util::Version::operator>)
        .def("__ge__", &util::Version::operator>=)
        .def("__repr__", [](const util::Version &v) {
            return tfm::format("%d.%d.%d",
                v.major_version, v.minor_version, v.patch_version);
        });
}
