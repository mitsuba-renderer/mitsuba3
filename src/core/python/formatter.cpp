#include <mitsuba/core/formatter.h>
#include <mitsuba/python/python.h>

#include <nanobind/trampoline.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>

// Trampoline for derived types implemented in Python
class PyFormatter : public Formatter {
public:
    NB_TRAMPOLINE(Formatter, 1);

    std::string format(mitsuba::LogLevel level, const char *cname,
            const char *fname, int line,
            std::string_view msg) override {
        NB_OVERRIDE_PURE(format, level, cname, fname, line, msg);
    }
};

MI_PY_EXPORT(Formatter) {
    MI_PY_TRAMPOLINE_CLASS(PyFormatter, Formatter, Object)
        .def(nb::init<>())
        .def_method(Formatter, format, "level"_a,
            nb::arg("cname").none(),
            "fname"_a, "line"_a,
            "msg"_a);

    MI_PY_CLASS(DefaultFormatter, Formatter)
        .def(nb::init<>())
        .def_method(DefaultFormatter, has_date)
        .def_method(DefaultFormatter, set_has_date)
        .def_method(DefaultFormatter, has_thread)
        .def_method(DefaultFormatter, set_has_thread)
        .def_method(DefaultFormatter, has_log_level)
        .def_method(DefaultFormatter, set_has_log_level)
        .def_method(DefaultFormatter, has_class)
        .def_method(DefaultFormatter, set_has_class);
}
