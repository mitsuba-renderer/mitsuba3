#include <mitsuba/python/python.h>
#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/render/film.h>
#include <mitsuba/render/imageblock.h>
#include <mitsuba/core/rfilter.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/spiral.h>

MTS_PY_EXPORT_CLASS_VARIANTS(Film) {
    using Point2i = typename Film::Point2i;
    using Vector2i = typename Film::Vector2i;

    MTS_PY_CLASS(Film, Object)
        .mdef(Film, clear)
        .mdef(Film, put, "block"_a)
        .mdef(Film, set_bitmap, "bitmap"_a)
        .mdef(Film, add_bitmap, "bitmap"_a, "multiplier"_a = 1.0f)
        .mdef(Film, set_destination_file, "filename"_a, "block_size"_a)
        .def("develop", py::overload_cast<>(&Film::develop))
        .def("develop", py::overload_cast<const Point2i &, const Vector2i &,
                                          const Point2i &, Bitmap *>(
                &Film::develop, py::const_),
             "offset"_a, "size"_a, "target_offset"_a, "target"_a)
        .mdef(Film, destination_exists, "basename"_a)
        .mdef(Film, has_high_quality_edges)
        .mdef(Film, bitmap)
        .mdef(Film, size)
        .mdef(Film, crop_size)
        .mdef(Film, crop_offset)
        .mdef(Film, set_crop_window)
        .def("reconstruction_filter",
             py::overload_cast<>(&Film::reconstruction_filter, py::const_))
        ;
}
