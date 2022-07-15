#include <mitsuba/render/film.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/python/python.h>

MI_PY_EXPORT(FilmFlags) {
    auto e = py::enum_<FilmFlags>(m, "FilmFlags", D(FilmFlags))
        .def_value(FilmFlags, Empty)
        .def_value(FilmFlags, Alpha)
        .def_value(FilmFlags, Spectral)
        .def_value(FilmFlags, Special);

    MI_PY_DECLARE_ENUM_OPERATORS(FilmFlags, e)
}
