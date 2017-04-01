#include <mitsuba/render/common.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(ETransportMode) {
    py::enum_<ETransportMode>(m, "ETransportMode", D(ETransportMode))
        .value("ERadiance", ERadiance)
        .value("EImportance", EImportance)
        .export_values();
}

MTS_PY_EXPORT(EMeasure) {
    py::enum_<EMeasure>(m, "EMeasure", D(EMeasure))
        .value("EInvalidMeasure", EInvalidMeasure)
        .value("ESolidAngle", ESolidAngle)
        .value("ELength", ELength)
        .value("EArea", EArea)
        .value("EDiscrete", EDiscrete)
        .export_values();
}
