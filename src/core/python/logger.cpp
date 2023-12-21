#include <mitsuba/core/logger.h>
#include <mitsuba/core/appender.h>
#include <mitsuba/core/formatter.h>
#include <mitsuba/python/python.h>

/// Submit a log message to the Mitusba logging system and tag it with the Python caller
static void PyLog(mitsuba::LogLevel level, const std::string &msg) {
#if PY_VERSION_HEX >= 0x03090000
    PyFrameObject *frame = PyThreadState_GetFrame(PyThreadState_Get());
    PyCodeObject *f_code = PyFrame_GetCode(frame);
#else
    PyFrameObject *frame = PyThreadState_Get()->frame;
    PyCodeObject *f_code = frame->f_code;
#endif

    std::string name =
        py::cast<std::string>(py::handle(f_code->co_name));
    std::string filename =
        py::cast<std::string>(py::handle(f_code->co_filename));
    std::string fmt = "%s: %s";
    int lineno = PyFrame_GetLineNumber(frame);

#if PY_VERSION_HEX >= 0x03090000
    Py_DECREF(f_code);
    Py_DECREF(frame);
#endif

    if (!name.empty() && name[0] != '<')
        fmt.insert(2, "()");

    if (!Thread::thread()->logger()) {
        Throw("No Logger instance is set on the current thread! This is likely due to Log being " 
              "called from a non-Mitsuba thread. You can manually set a thread's ThreadEnvironment " 
              "(which includes the logger) using ScopedSetThreadEnvironment e.g.\n"
              "# Main thread\n"
              "env = mi.ThreadEnvironment()\n"
              "# Secondary thread\n"
              "with mi.ScopedSetThreadEnvironment(env):\n"
              "   mi.set_log_level(mi.LogLevel.Info)\n"
              "   mi.Log(mi.LogLevel.Info, 'Message')\n");
    }

    Thread::thread()->logger()->log(
        level, nullptr /* class_ */,
        filename.c_str(), lineno,
        tfm::format(fmt.c_str(), name.c_str(), msg.c_str()));
}

MI_PY_EXPORT(Logger) {
    MI_PY_CLASS(Logger, Object)
        .def(py::init<mitsuba::LogLevel>(), D(Logger, Logger))
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
