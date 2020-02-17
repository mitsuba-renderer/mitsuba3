#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/render/film.h>
#include <mitsuba/render/imageblock.h>
#include <mitsuba/core/rfilter.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/spiral.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(Film) {
    MTS_PY_IMPORT_TYPES(Film)
    MTS_PY_CLASS(Film, Object)
        .def_method(Film, prepare, "channels"_a)
        .def_method(Film, put, "block"_a)
        .def_method(Film, set_destination_file, "filename"_a)
        .def("develop", py::overload_cast<>(&Film::develop))
        .def("develop", py::overload_cast<const ScalarPoint2i &, const ScalarVector2i &,
                                            const ScalarPoint2i &, Bitmap *>(
                &Film::develop, py::const_),
            "offset"_a, "size"_a, "target_offset"_a, "target"_a)
        .def_method(Film, destination_exists, "basename"_a)
        .def_method(Film, bitmap, "raw"_a = false)
        .def_method(Film, has_high_quality_edges)
        .def_method(Film, size)
        .def_method(Film, crop_size)
        .def_method(Film, crop_offset)
        .def_method(Film, set_crop_window)
        .def_method(Film, reconstruction_filter);
}
