#include <mitsuba/render/common.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(ETransportMode) {
    py::enum_<ETransportMode>(m, "ETransportMode", D(ETransportMode))
        .value("ERadiance", ERadiance, D(ETransportMode, ERadiance))
        .value("EImportance", EImportance, D(ETransportMode, EImportance))
        .export_values();
}

MTS_PY_EXPORT(EMeasure) {
    py::enum_<EMeasure>(m, "EMeasure", D(EMeasure))
        .value("EInvalidMeasure", EInvalidMeasure, D(EMeasure, EInvalidMeasure))
        .value("ESolidAngle", ESolidAngle, D(EMeasure, ESolidAngle))
        .value("ELength", ELength, D(EMeasure, ELength))
        .value("EArea", EArea, D(EMeasure, EArea))
        .value("EDiscrete", EDiscrete, D(EMeasure, EDiscrete))
        .export_values();
}
