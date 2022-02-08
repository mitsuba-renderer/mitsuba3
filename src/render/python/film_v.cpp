#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/render/film.h>
#include <mitsuba/render/imageblock.h>
#include <mitsuba/core/rfilter.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/spiral.h>
#include <mitsuba/python/python.h>

MI_PY_EXPORT(Film) {
    MI_PY_IMPORT_TYPES(Film)
    MI_PY_CLASS(Film, Object)
        .def_method(Film, prepare, "aovs"_a)
        .def_method(Film, put_block, "block"_a)
        .def_method(Film, develop, "raw"_a = false)
        .def_method(Film, bitmap, "raw"_a = false)
        .def_method(Film, write, "path"_a)
        .def_method(Film, sample_border)
        .def_method(Film, size)
        .def_method(Film, crop_size)
        .def_method(Film, crop_offset)
        .def_method(Film, set_crop_window)
        .def_method(Film, rfilter)
        .def_method(Film, prepare_sample, "spec"_a, "wavelengths"_a, "aovs"_a,
                    "active"_a)
        .def_method(Film, create_block, "size"_a = ScalarVector2u(0, 0),
                    "normalize"_a = false, "borders"_a = false)
        .def_method(Film, schedule_storage)
        .def_method(Film, sensor_response_function)
        .def_method(Film, flags);
}
