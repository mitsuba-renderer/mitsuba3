#include <mitsuba/core/logger.h>
#include <mitsuba/core/appender.h>
#include <mitsuba/core/formatter.h>
#include <mitsuba/python/python.h>

/// Submit a log message to the Mitusba logging system and tag it with the Python caller
static void PyLog(ELogLevel level, const std::string &msg) {
    PyFrameObject *frame = PyThreadState_Get()->frame;

    std::string name =
        py::cast<std::string>(py::handle(frame->f_code->co_name));
    std::string filename =
        py::cast<std::string>(py::handle(frame->f_code->co_filename));
    std::string fmt = "%s: %s";
    int lineno = PyFrame_GetLineNumber(frame);

    if (!name.empty() && name[0] != '<')
        fmt.insert(2, "()");

    Thread::thread()->logger()->log(
        level, nullptr /* class_ */,
        filename.c_str(), lineno,
        tfm::format(fmt.c_str(), name.c_str(), msg.c_str()));
}

MTS_PY_EXPORT(Logger) {
    MTS_PY_CLASS(Logger, Object)
        .def(py::init<ELogLevel>(), D(Logger, Logger))
        .mdef(Logger, log_progress, "progress"_a, "name"_a,
              "formatted"_a, "eta"_a, "ptr"_a = py::none())
        .mdef(Logger, set_log_level)
        .mdef(Logger, log_level)
        .mdef(Logger, set_error_level)
        .mdef(Logger, error_level)
        .mdef(Logger, add_appender, py::keep_alive<1, 2>())
        .mdef(Logger, remove_appender)
        .mdef(Logger, clear_appenders)
        .mdef(Logger, appender_count)
        .def("appender", (Appender * (Logger::*)(size_t)) &Logger::appender, D(Logger, appender))
        .def("formatter", (Formatter * (Logger::*)()) &Logger::formatter, D(Logger, formatter))
        .mdef(Logger, set_formatter, py::keep_alive<1, 2>())
        .mdef(Logger, read_log);

    py::enum_<ELogLevel>(m, "ELogLevel", D(ELogLevel))
        .value("ETrace", ETrace, D(ELogLevel, ETrace))
        .value("EDebug", EDebug, D(ELogLevel, EDebug))
        .value("EInfo", EInfo, D(ELogLevel, EInfo))
        .value("EWarn", EWarn, D(ELogLevel, EWarn))
        .value("EError", EError, D(ELogLevel, EError))
        .export_values();

    m.def("Log", &PyLog, "level"_a, "msg"_a);
}
