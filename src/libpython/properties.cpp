#include "python.h"

#include <mitsuba/core/properties.h>

MTS_PY_EXPORT(Properties) {
    py::class_<Properties>(m, "Properties", DM(Properties))
        .def(py::init<>(), DM(Properties, Properties))
        .def(py::init<const std::string &>(), DM(Properties, Properties, 2))
        .def(py::init<const Properties &>(), DM(Properties, Properties, 3))
        .mdef(Properties, hasProperty)
        .mdef(Properties, removeProperty)
        .mdef(Properties, markQueried);

}
