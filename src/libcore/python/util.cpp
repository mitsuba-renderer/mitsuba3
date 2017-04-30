#include <mitsuba/core/util.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(util) {
    MTS_PY_IMPORT_MODULE(util, "mitsuba.core.util");

    util.def("core_count", &util::core_count, D(util, core_count));
    util.def("time_string", &util::time_string, "time"_a,
          "precise"_a = false, D(util, time_string));
    util.def("mem_string", &util::mem_string, "size"_a,
          "precise"_a = false, D(util, mem_string));
    util.def("trap_debugger", &util::trap_debugger, D(util, trap_debugger));
}
