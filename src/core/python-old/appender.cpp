#include <mitsuba/core/appender.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/python/python.h>

// Trampoline for derived types implemented in Python
class PyAppender : public Appender {
public:
    using Appender::Appender;

    virtual void append(mitsuba::LogLevel level, const std::string &text) override {
        PYBIND11_OVERRIDE_PURE(
            void,          // Return value
            Appender,      // Parent class
            append,        // Function
            level, text    // Arguments
        );
    }

    virtual void log_progress(float progress, const std::string &name,
                              const std::string &formatted,
                              const std::string &eta, const void *ptr) override {
        PYBIND11_OVERRIDE_PURE(
            void,          // Return value
            Appender,      // Parent class
            log_progress,  // Function
            progress, name, formatted, eta, ptr // Arguments
        );
    }
};

MI_PY_EXPORT(Appender) {
    py::enum_<mitsuba::LogLevel>(m, "LogLevel", D(LogLevel))
        .value("Trace", Trace, D(LogLevel, Trace))
        .value("Debug", Debug, D(LogLevel, Debug))
        .value("Info", Info, D(LogLevel, Info))
        .value("Warn", Warn, D(LogLevel, Warn))
        .value("Error", Error, D(LogLevel, Error));
    py::implicitly_convertible<unsigned, mitsuba::LogLevel>();

    MI_PY_TRAMPOLINE_CLASS(PyAppender, Appender, Object)
        .def(py::init<>())
        .def_method(Appender, append, "level"_a, "text"_a)
        .def_method(Appender, log_progress, "progress"_a, "name"_a,
            "formatted"_a, "eta"_a, "ptr"_a = py::none());

    MI_PY_CLASS(StreamAppender, Appender)
        .def(py::init<const std::string &>(), D(StreamAppender, StreamAppender))
        .def_method(StreamAppender, logs_to_file)
        .def_method(StreamAppender, read_log);
}
