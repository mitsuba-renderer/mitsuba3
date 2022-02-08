#include <mitsuba/core/formatter.h>
#include <mitsuba/core/thread.h>
#include <mitsuba/python/python.h>

// Trampoline for derived types implemented in Python
class PyFormatter : public Formatter {
public:
    using Formatter::Formatter;

    virtual std::string format(mitsuba::LogLevel level, const Class *class_,
            const Thread *thread, const char *file, int line,
            const std::string &msg) override {
        PYBIND11_OVERRIDE_PURE(
            std::string,   // Return value
            Formatter,     // Parent class
            format,        // Function
            level, class_, thread, file, line, msg  // Arguments
        );
    }
};

MI_PY_EXPORT(Formatter) {
    MI_PY_TRAMPOLINE_CLASS(PyFormatter, Formatter, Object)
        .def(py::init<>())
        .def_method(Formatter, format, "level"_a, "class_"_a,
            "thread"_a, "file"_a, "line"_a,
            "msg"_a);

    MI_PY_CLASS(DefaultFormatter, Formatter)
        .def(py::init<>())
        .def_method(DefaultFormatter, has_date)
        .def_method(DefaultFormatter, set_has_date)
        .def_method(DefaultFormatter, has_thread)
        .def_method(DefaultFormatter, set_has_thread)
        .def_method(DefaultFormatter, has_log_level)
        .def_method(DefaultFormatter, set_has_log_level)
        .def_method(DefaultFormatter, has_class)
        .def_method(DefaultFormatter, set_has_class);
}
