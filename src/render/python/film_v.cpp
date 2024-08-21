#include <nanobind/nanobind.h> // Needs to be first, to get `ref<T>` caster
#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/render/film.h>
#include <mitsuba/render/imageblock.h>
#include <mitsuba/core/rfilter.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/spiral.h>
#include <mitsuba/python/python.h>
#include <nanobind/trampoline.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <drjit/python.h>

/// Trampoline for derived types implemented in Python
MI_VARIANT class PyFilm : public Film<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Film, ImageBlock)
    NB_TRAMPOLINE(Film, 11);

    PyFilm(const Properties &props) : Film(props) { }

    size_t base_channels_count() const override {
        NB_OVERRIDE_PURE(base_channels_count);
    }

    size_t prepare(const std::vector<std::string> &aovs) override {
        NB_OVERRIDE_PURE(prepare, aovs);
    }

    void put_block(const ImageBlock *block) override {
        NB_OVERRIDE_PURE(put_block, block);
    }

    void clear() override {
        NB_OVERRIDE_PURE(clear);
    }

    TensorXf develop(bool raw = false) const override {
        NB_OVERRIDE_PURE(develop, raw);
    }

    ref<Bitmap> bitmap(bool raw = false) const override {
        NB_OVERRIDE_PURE(bitmap, raw);
    }

    void write(const fs::path &path) const override {
        NB_OVERRIDE_PURE(write, path);
    }

    void schedule_storage() override {
        NB_OVERRIDE_PURE(schedule_storage);
    }

    void prepare_sample(const UnpolarizedSpectrum &spec,
                        const Wavelength &wavelengths,
                        Float* aovs, Float weight = 1.f,
                        Float alpha = 1.f, Mask active = true) const override {
        NB_OVERRIDE_PURE(prepare_sample, spec, wavelengths, aovs, weight, alpha, active);
    }

    ref<ImageBlock> create_block(const ScalarVector2u &size = 0,
                                 bool normalize = false,
                                 bool border = false) override {
        NB_OVERRIDE_PURE(create_block, size, normalize, border);
    }

    std::string to_string() const override {
        NB_OVERRIDE_PURE(to_string);
    }

    using Film::m_flags;
    using Film::m_size;
    using Film::m_crop_size;
    using Film::m_crop_offset;
    using Film::m_sample_border;
    using Film::m_filter;
    using Film::m_srf;
};

MI_PY_EXPORT(Film) {
    MI_PY_IMPORT_TYPES(Film)
    using PyFilm = PyFilm<Float, Spectrum>;
    using Properties = PropertiesV<Float>;

    m.def("has_flag", [](uint32_t flags, FilmFlags f) {return has_flag(flags, f);});
    m.def("has_flag", [](UInt32   flags, FilmFlags f) {return has_flag(flags, f);});

    auto film = MI_PY_TRAMPOLINE_CLASS(PyFilm, Film, Object)
        .def(nb::init<const Properties &>(), "props"_a)
        .def_method(Film, prepare, "aovs"_a)
        .def_method(Film, put_block, "block"_a)
        .def_method(Film, clear)
        .def_method(Film, develop, "raw"_a = false)
        .def_method(Film, bitmap, "raw"_a = false)
        .def_method(Film, write, "path"_a)
        .def_method(Film, sample_border)
        .def_method(Film, base_channels_count)
        // Make sure to return a copy of those members as they might also be
        // exposed by-references via `mi.traverse`. In which case the return
        // policy of `mi.traverse` might overrule the ones of those bindings.
        .def("size",
             [] (const Film *film) { return ScalarVector2u(film->size()); },
             D(Film, size))
        .def("crop_size",
             [] (const Film *film) { return ScalarVector2u(film->crop_size()); },
             D(Film, crop_size))
        .def("crop_offset",
             [] (const Film *film) { return ScalarPoint2u(film->crop_offset()); },
             D(Film, crop_offset))
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
        .def_method(Film, flags)
        .def_field(PyFilm, m_flags, D(Film, m_flags));

    MI_PY_REGISTER_OBJECT("register_film", Film)
}
