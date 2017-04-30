#include <mitsuba/core/appender.h>
#include <mitsuba/python/python.h>

/* Trampoline for derived types implemented in Python */
class PyAppender : public Appender {
public:
    using Appender::Appender;

    virtual void append(ELogLevel level, const std::string &text) override {
        PYBIND11_OVERLOAD_PURE(
            void,          /* Return value */
            Appender,      /* Parent class */
            append,        /* Function */
            level, text    /* Arguments */
        );
    }

    virtual void log_progress(Float progress, const std::string &name,
                              const std::string &formatted,
                              const std::string &eta, const void *ptr) override {
        PYBIND11_OVERLOAD_PURE(
            void,          /* Return value */
            Appender,      /* Parent class */
            append,        /* Function */
            progress, name, formatted, eta, ptr /* Arguments */
        );
    }
};

MTS_PY_EXPORT(Appender) {
    MTS_PY_TRAMPOLINE_CLASS(PyAppender, Appender, Object)
        .def(py::init<>())
        .mdef(Appender, append, "level"_a, "text"_a)
        .mdef(Appender, log_progress, "progress"_a, "name"_a,
              "formatted"_a, "eta"_a, "ptr"_a = py::none());

    MTS_PY_CLASS(StreamAppender, Appender)
        .def(py::init<const std::string &>(), D(StreamAppender, StreamAppender))
        .mdef(StreamAppender, logs_to_file)
        .mdef(StreamAppender, read_log);
}
