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

    m.def("has_flag", [](UInt32 flags, FilmFlags f) {return has_flag(flags, f);});

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
        .def("prepare_sample",
        [] (const Film *film, const UnpolarizedSpectrum &spec,
            const Wavelength &wavelengths, size_t nChannels,
            Float weight, Float alpha, Mask active) {
            std::vector<Float> aovs(nChannels);
            film->prepare_sample(spec, wavelengths, aovs.data(), weight, alpha, active);
            return aovs;
        },
        "spec"_a, "wavelengths"_a, "nChannels"_a,
        "weight"_a = 1.f, "alpha"_a = 1.f, "active"_a = true,
        D(Film, prepare_sample))
        .def_method(Film, create_block, "size"_a = ScalarVector2u(0, 0),
                    "normalize"_a = false, "borders"_a = false)
        .def_method(Film, schedule_storage)
        .def_method(Film, sensor_response_function)
        .def_method(Film, flags);
}
