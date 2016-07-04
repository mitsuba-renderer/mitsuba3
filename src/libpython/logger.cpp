#include <mitsuba/core/logger.h>
#include <mitsuba/core/appender.h>
#include <mitsuba/core/formatter.h>
#include "python.h"

/// Submit a log message to the Mitusba logging system and tag it with the Python caller
static void PyLog(ELogLevel level, const std::string &msg) {
    PyFrameObject *frame = PyThreadState_Get()->frame;

    std::string name = (std::string) py::str(frame->f_code->co_name, true);
    std::string filename =
        (std::string) py::str(frame->f_code->co_filename, true);
    std::string fmt = "%s: %s";
    int lineno = PyFrame_GetLineNumber(frame);

    if (!name.empty() && name[0] != '<')
        fmt.insert(2, "()");

    Thread::thread()->logger()->log(
        level, nullptr, /* theClass */
        filename.c_str(), lineno,
        tfm::format(fmt.c_str(), name.c_str(), msg.c_str()));
}

MTS_PY_EXPORT(Logger) {
    MTS_PY_CLASS(Logger, Object)
        .def(py::init<ELogLevel>(), DM(Logger, Logger))
        .mdef(Logger, logProgress, py::arg("progress"), py::arg("name"),
              py::arg("formatted"), py::arg("eta"), py::arg("ptr") = py::none())
        .mdef(Logger, setLogLevel)
        .mdef(Logger, logLevel)
        .mdef(Logger, setErrorLevel)
        .mdef(Logger, errorLevel)
        .mdef(Logger, addAppender, py::keep_alive<1, 2>())
        .mdef(Logger, removeAppender)
        .mdef(Logger, clearAppenders)
        .mdef(Logger, appenderCount)
        .def("appender", (Appender * (Logger::*)(size_t)) &Logger::appender, DM(Logger, appender))
        .def("formatter", (Formatter * (Logger::*)()) &Logger::formatter, DM(Logger, formatter))
        .mdef(Logger, setFormatter, py::keep_alive<1, 2>())
        .mdef(Logger, readLog);

    py::enum_<ELogLevel>(m, "ELogLevel", DM(ELogLevel))
        .value("ETrace", ETrace)
        .value("EDebug", EDebug)
        .value("EInfo", EInfo)
        .value("EWarn", EWarn)
        .value("EError", EError)
        .export_values();

    m.def("Log", &PyLog, py::arg("level"), py::arg("msg"));
}
