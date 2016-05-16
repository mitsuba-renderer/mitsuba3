#include <mitsuba/core/formatter.h>
#include <mitsuba/core/thread.h>
#include "python.h"

/* Trampoline for derived types implemented in Python */
class PyFormatter : public Formatter {
public:
    using Formatter::Formatter;

    virtual std::string format(ELogLevel level, const Class *theClass,
            const Thread *thread, const char *file, int line,
            const std::string &msg) override {
        PYBIND11_OVERLOAD_PURE(
            std::string,   /* Return value */
            Formatter,     /* Parent class */
            format,        /* Function */
            level, theClass, thread, file, line, msg  /* Arguments */
        );
    }
};

MTS_PY_EXPORT(Formatter) {
    MTS_PY_TRAMPOLINE_CLASS(PyFormatter, Formatter, Object)
        .def(py::init<>())
        .mdef(Formatter, format, py::arg("level"), py::arg("theClass"),
              py::arg("thread"), py::arg("file"), py::arg("line"),
              py::arg("msg"));

    MTS_PY_CLASS(DefaultFormatter, Formatter)
        .def(py::init<>())
        .mdef(DefaultFormatter, getHaveDate)
        .mdef(DefaultFormatter, setHaveDate)
        .mdef(DefaultFormatter, getHaveThread)
        .mdef(DefaultFormatter, setHaveThread)
        .mdef(DefaultFormatter, getHaveLogLevel)
        .mdef(DefaultFormatter, setHaveLogLevel)
        .mdef(DefaultFormatter, getHaveClass)
        .mdef(DefaultFormatter, setHaveClass);
}
