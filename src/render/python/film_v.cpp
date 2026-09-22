#include <nanobind/nanobind.h> // Needs to be first, to get `ref<T>` caster
#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/render/film.h>
#include <mitsuba/render/imageblock.h>
#include <mitsuba/render/postprocess.h>
#include <mitsuba/core/rfilter.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/spiral.h>
#include <mitsuba/python/python.h>
#include <nanobind/trampoline.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <drjit/python.h>
#include <drjit/traversable_base.h>

/// Trampoline for derived types implemented in Python
MI_VARIANT class PyFilm : public Film<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Film, ImageBlock)
    NB_TRAMPOLINE(Film);

    PyFilm(const Properties &props) : Film(props) { this->alloc_storage(); }

    void prepare_sample(const UnpolarizedSpectrum &spec,
                        const Wavelength &wavelengths,
                        Float *out, Mask valid = true,
                        Mask active = true) const override {
        constexpr uint64_t nb_hash = nanobind::detail::str_hash("prepare_sample");
        nanobind::detail::ticket nb_ticket(nb_trampoline, "prepare_sample", nb_hash, false);
        if (!nb_ticket.key.is_valid()) {
            Film::prepare_sample(spec, wavelengths, out, valid, active);
            return;
        }

        std::vector<Float> values = nanobind::cast<std::vector<Float>>(
            nb_trampoline.base().attr(nb_ticket.key)(spec, wavelengths, valid, active));
        if (values.size() != this->base_channels().size())
            throw std::runtime_error("prepare_sample(): the returned list "
                                     "must have one entry per base channel.");
        std::copy(values.begin(), values.end(), out);
    }

    void write(const fs::path &path) const override {
        NB_OVERRIDE(write, path);
    }

    std::string to_string() const override {
        NB_OVERRIDE_PURE(to_string);
    }

    void traverse(TraversalCallback *cb) override {
        NB_OVERRIDE(traverse, cb);
    }

    void parameters_changed(const std::vector<std::string> &keys) override {
        NB_OVERRIDE(parameters_changed, keys);
    }

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
    using Properties = mitsuba::Properties;

    auto film = MI_PY_TRAMPOLINE_CLASS(PyFilm, Film, Object)
        .def(nb::init<const Properties &>(), "props"_a)
        .def_method(Film, prepare, "aovs"_a, "pixel_weight"_a = 0.f)
        .def_method(Film, pixel_weight)
        .def_method(Film, put_block, "block"_a)
        .def_method(Film, clear)
        .def_method(Film, develop, "postprocess"_a = true)
        .def_method(Film, bitmap, "postprocess"_a = true)
        .def_method(Film, storage)
        .def_method(Film, write, "path"_a)
        .def_method(Film, channels)
        .def_method(Film, sample_border)
        .def_method(Film, base_channels)
        // Make sure to return a copy of those members as they might also be
        // exposed by-references via `mi.traverse`. In which case the return
        // policy of `mi.traverse` might overrule the ones of those bindings.
        .def("size",
             [] (const Film *film) { return ScalarVector2u(film->size()); },
             D(Film, size))
        .def("crop_size",
             [] (const Film *film) { return ScalarVector2u(film->crop_size()); },
             D(Film, crop_size))
        .def("set_size", &Film::set_size, D(Film, set_size), "size"_a)
        .def("crop_offset",
             [] (const Film *film) { return ScalarPoint2u(film->crop_offset()); },
             D(Film, crop_offset))
        .def_method(Film, rfilter)
        .def_method(Film, postprocess)
        .def("apply_postprocess",
             nb::overload_cast<const TensorXf &, const std::vector<std::string> &>(
                 &Film::apply_postprocess, nb::const_),
             "image"_a, "channels"_a, D(Film, apply_postprocess))
        .def("prepare_sample",
            [] (const Film *film, const UnpolarizedSpectrum &spec,
                const Wavelength &wavelengths, Mask valid, Mask active) {
                std::vector<Float> values(film->base_channels().size());
                film->prepare_sample(spec, wavelengths, values.data(), valid, active);
                return values;
            },
            "spec"_a, "wavelengths"_a, "valid"_a = true, "active"_a = true,
            D(Film, prepare_sample))
        .def_method(Film, create_block, "size"_a = ScalarVector2u(0, 0),
                    "normalize"_a = false, "borders"_a = false)
        .def_method(Film, sensor_response_function);

    drjit::bind_traverse(film);
}
