#include <mitsuba/core/util.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(util) {
    MTS_PY_IMPORT_MODULE(util, "mitsuba.core.util");

    util.def_method(util, core_count)
        .def_method(util, time_string, "time"_a, "precise"_a = false)
        .def_method(util, mem_string, "size"_a, "precise"_a = false)
        .def_method(util, trap_debugger);
}
