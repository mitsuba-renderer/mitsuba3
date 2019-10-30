#include <mitsuba/core/util.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(util) {
    MTS_PY_IMPORT_MODULE(util, "mitsuba.core.util");

    util.mdef(util, core_count)
        .mdef(util, time_string, "time"_a, "precise"_a = false)
        .mdef(util, mem_string, "size"_a, "precise"_a = false)
        .mdef(util, trap_debugger);
}
