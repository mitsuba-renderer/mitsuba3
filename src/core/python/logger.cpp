#include <mitsuba/core/logger.h>
#include <mitsuba/core/appender.h>
#include <mitsuba/core/formatter.h>
#include <mitsuba/python/python.h>
#include <nanobind/stl/string.h>

/// Submit a log message to the Mitusba logging system and tag it with the Python caller
static void PyLog(mitsuba::LogLevel level, const std::string &msg) {

    // Do not log if no logger is available. This is consistent with the
    // C++-side "Log" function in logger.h.
    if (!Thread::thread()->logger())
        return;

#if PY_VERSION_HEX >= 0x03090000
    PyFrameObject *frame = PyThreadState_GetFrame(PyThreadState_Get());
    PyCodeObject *f_code = PyFrame_GetCode(frame);
#else
    PyFrameObject *frame = PyThreadState_Get()->frame;
    PyCodeObject *f_code = frame->f_code;
#endif

    std::string name =
        nb::borrow<nb::str>(nb::handle(f_code->co_name)).c_str();
    std::string filename =
        nb::borrow<nb::str>(nb::handle(f_code->co_filename)).c_str();
    std::string fmt = "%s: %s";
    int lineno = PyFrame_GetLineNumber(frame);

#if PY_VERSION_HEX >= 0x03090000
    Py_DECREF(f_code);
    Py_DECREF(frame);
#endif

    if (!name.empty() && name[0] != '<')
        fmt.insert(2, "()");

    Thread::thread()->logger()->log(
        level, nullptr /* class_ */,
        filename.c_str(), lineno,
        tfm::format(fmt.c_str(), name.c_str(), msg.c_str()));
}

MI_PY_EXPORT(Logger) {
    MI_PY_CLASS(Logger, Object)
        .def(nb::init<mitsuba::LogLevel>(), D(Logger, Logger))
        .def_method(Logger, log_progress, "progress"_a, "name"_a,
            "formatted"_a, "eta"_a, "ptr"_a = nb::none())
        .def_method(Logger, set_log_level)
        .def_method(Logger, log_level)
        .def_method(Logger, set_error_level)
        .def_method(Logger, error_level)
        .def_method(Logger, add_appender)
        .def_method(Logger, remove_appender)
        .def_method(Logger, clear_appenders)
        .def_method(Logger, appender_count)
        .def("appender", (Appender * (Logger::*)(size_t)) &Logger::appender, D(Logger, appender))
        .def("formatter", (Formatter * (Logger::*)()) &Logger::formatter, D(Logger, formatter))
        .def_method(Logger, set_formatter)
        .def_method(Logger, read_log);

    m.def("Log", &PyLog, "level"_a, "msg"_a);
}
