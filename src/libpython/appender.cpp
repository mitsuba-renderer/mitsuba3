#include "python.h"
#include <mitsuba/core/appender.h>
    
MTS_PY_EXPORT(Appender) {
    MTS_PY_CLASS(Appender)
        .qdef(Appender, append)
        .qdef(Appender, logProgress, py::arg("progress"), py::arg("name"),
              py::arg("formatted"), py::arg("eta"), py::arg("ptr") = py::none());

    MTS_PY_CLASS(StreamAppender)
        .def(py::init<const std::string &>())
        .qdef(StreamAppender, logsToFile);
}
