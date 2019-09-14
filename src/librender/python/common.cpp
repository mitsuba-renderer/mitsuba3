#include <mitsuba/render/common.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(TransportMode) {
    py::enum_<TransportMode>(m, "TransportMode", D(TransportMode))
        .value("ERadiance", ERadiance, D(TransportMode, ERadiance))
        .value("EImportance", EImportance, D(TransportMode, EImportance))
        .export_values();
}
