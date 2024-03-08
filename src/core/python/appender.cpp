#include <mitsuba/core/appender.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/python/python.h>
#include <nanobind/trampoline.h>
#include <nanobind/stl/string.h>

// Trampoline for derived types implemented in Python
class PyAppender : public Appender {
public:
    NB_TRAMPOLINE(Appender, 2);

    void append(mitsuba::LogLevel level, const std::string &text) override {
        NB_OVERRIDE_PURE(append, level, text);
    }

    void log_progress(float progress, const std::string &name,
                              const std::string &formatted,
                              const std::string &eta, const void *ptr) override {
        NB_OVERRIDE_PURE(log_progress, progress, name, formatted, eta, (void*)ptr);
    }
};

MI_PY_EXPORT(Appender) {
    nb::enum_<mitsuba::LogLevel>(m, "LogLevel", nb::is_arithmetic(), D(LogLevel))
        .value("Trace", Trace, D(LogLevel, Trace))
        .value("Debug", Debug, D(LogLevel, Debug))
        .value("Info", Info, D(LogLevel, Info))
        .value("Warn", Warn, D(LogLevel, Warn))
        .value("Error", Error, D(LogLevel, Error));

    MI_PY_TRAMPOLINE_CLASS(PyAppender, Appender, Object)
        .def(nb::init<>())
        .def_method(Appender, append, "level"_a, "text"_a)
        .def_method(Appender, log_progress, "progress"_a, "name"_a,
            "formatted"_a, "eta"_a, "ptr"_a = nb::none());

    MI_PY_CLASS(StreamAppender, Appender)
        .def(nb::init<const std::string &>(), D(StreamAppender, StreamAppender))
        .def_method(StreamAppender, logs_to_file)
        .def_method(StreamAppender, read_log);
}
