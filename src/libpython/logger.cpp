#include "python.h"
#include <mitsuba/core/logger.h>
#include <mitsuba/core/appender.h>
#include <mitsuba/core/formatter.h>

MTS_PY_EXPORT(Logger) {
    py::class_<Logger, ref<Logger>>(m, "Logger", DM(Logger))
        .def(py::init<ELogLevel>())
        .qdef(Logger, logProgress, py::arg("progress"), py::arg("name"),
              py::arg("formatted"), py::arg("eta"), py::arg("ptr") = py::none())
        .qdef(Logger, setLogLevel)
        .qdef(Logger, getLogLevel)
        .qdef(Logger, setErrorLevel)
        .qdef(Logger, getErrorLevel)
        .qdef(Logger, addAppender)
        .qdef(Logger, removeAppender)
        .qdef(Logger, clearAppenders)
        .qdef(Logger, getAppenderCount)
        .def("getAppender", (Appender * (Logger::*)(size_t)) &Logger::getAppender, DM(Logger, getAppender))
        .def("getFormatter", (Formatter * (Logger::*)()) &Logger::getFormatter, DM(Logger, getFormatter))
        .qdef(Logger, setFormatter)
        .qdef(Logger, readLog);
}
