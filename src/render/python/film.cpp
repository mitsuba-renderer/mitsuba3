#include <mitsuba/render/film.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/python/python.h>

MI_PY_EXPORT(FilmFlags) {
    auto e = nb::enum_<FilmFlags>(m, "FilmFlags", nb::is_arithmetic(), D(FilmFlags))
        .def_value(FilmFlags, Empty)
        .def_value(FilmFlags, Alpha)
        .def_value(FilmFlags, Spectral)
        .def_value(FilmFlags, Special);
}
