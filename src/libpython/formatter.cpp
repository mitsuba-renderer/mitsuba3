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
        .mdef(Formatter, format, "level"_a, "theClass"_a,
              "thread"_a, "file"_a, "line"_a,
              "msg"_a);

    MTS_PY_CLASS(DefaultFormatter, Formatter)
        .def(py::init<>())
        .mdef(DefaultFormatter, has_date)
        .mdef(DefaultFormatter, set_has_date)
        .mdef(DefaultFormatter, has_thread)
        .mdef(DefaultFormatter, set_has_thread)
        .mdef(DefaultFormatter, has_log_level)
        .mdef(DefaultFormatter, set_has_log_level)
        .mdef(DefaultFormatter, has_class)
        .mdef(DefaultFormatter, set_has_class);
}
