#include <mitsuba/core/appender.h>
#include "python.h"

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

    virtual void logProgress(Float progress, const std::string &name,
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
        .mdef(Appender, append, py::arg("level"), py::arg("text"))
        .mdef(Appender, logProgress, py::arg("progress"), py::arg("name"),
              py::arg("formatted"), py::arg("eta"), py::arg("ptr") = py::none());

    MTS_PY_CLASS(StreamAppender, Appender)
        .def(py::init<const std::string &>(), DM(StreamAppender, StreamAppender))
        .mdef(StreamAppender, logsToFile)
        .mdef(StreamAppender, readLog);
}
