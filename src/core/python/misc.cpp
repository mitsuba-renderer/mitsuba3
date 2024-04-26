#include <mitsuba/core/util.h>
#include <mitsuba/python/python.h>
#include <nanobind/stl/string.h>

MI_PY_EXPORT(misc) {
    auto misc = m.def_submodule("misc", "Miscellaneous utility routines");

    misc.def("core_count", &util::core_count, D(util, core_count))
        .def("time_string", &util::time_string, D(util, time_string), "time"_a, "precise"_a = false)
        .def("mem_string", &util::mem_string, D(util, mem_string), "size"_a, "precise"_a = false)
        .def("trap_debugger", &util::trap_debugger, D(util, trap_debugger));
}
