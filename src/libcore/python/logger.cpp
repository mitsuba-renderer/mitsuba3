#include <mitsuba/core/logger.h>
#include <mitsuba/core/appender.h>
#include <mitsuba/core/formatter.h>
#include <mitsuba/python/python.h>

/// Submit a log message to the Mitusba logging system and tag it with the Python caller
static void PyLog(LogLevel level, const std::string &msg) {
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
        .def(py::init<LogLevel>(), D(Logger, Logger))
        .def_method(Logger, log_progress, "progress"_a, "name"_a,
            "formatted"_a, "eta"_a, "ptr"_a = py::none())
        .def_method(Logger, set_log_level)
        .def_method(Logger, log_level)
        .def_method(Logger, set_error_level)
        .def_method(Logger, error_level)
        .def_method(Logger, add_appender, py::keep_alive<1, 2>())
        .def_method(Logger, remove_appender)
        .def_method(Logger, clear_appenders)
        .def_method(Logger, appender_count)
        .def("appender", (Appender * (Logger::*)(size_t)) &Logger::appender, D(Logger, appender))
        .def("formatter", (Formatter * (Logger::*)()) &Logger::formatter, D(Logger, formatter))
        .def_method(Logger, set_formatter, py::keep_alive<1, 2>())
        .def_method(Logger, read_log);

    m.def("Log", &PyLog, "level"_a, "msg"_a);
}
