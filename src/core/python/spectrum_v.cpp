#include <mitsuba/core/fwd.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/python/python.h>

#include <nanobind/stl/list.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

MI_PY_EXPORT(Spectrum) {
    MI_PY_IMPORT_TYPES()

    m.def("luminance", [](const UnpolarizedSpectrum &value, const wavelength_t<UnpolarizedSpectrum> &w, Mask active) {
            return luminance(value, w, active);
        }, "value"_a, "wavelengths"_a, "active"_a = true, D(luminance))
    .def("luminance", [](Color<Float, 3> c) {
            return luminance(c);
        }, "c"_a, D(luminance))

    .def("cie1931_xyz", [](Float wavelengths) {
            return cie1931_xyz(wavelengths);
        }, "wavelength"_a, D(cie1931_xyz))
    .def("linear_rgb_rec", [](Float wavelengths) {
            return linear_rgb_rec(wavelengths);
        }, "wavelength"_a, D(linear_rgb_rec))
    .def("cie1931_y", [](Float wavelengths) {
            return cie1931_y(wavelengths);
        }, "wavelength"_a, D(cie1931_y))
    .def("cie_d65", [](Float wavelengths) {
            return cie_d65(wavelengths);
        }, "wavelength"_a, D(cie_d65))

    .def("sample_rgb_spectrum", &sample_rgb_spectrum<Float>, "sample"_a,
        D(sample_rgb_spectrum))
    .def("sample_rgb_spectrum", &sample_rgb_spectrum<Spectrum>, "sample"_a,
        D(sample_rgb_spectrum))
    .def("pdf_rgb_spectrum", &pdf_rgb_spectrum<Float>, "wavelengths"_a,
        D(pdf_rgb_spectrum))
    .def("pdf_rgb_spectrum", &pdf_rgb_spectrum<Spectrum>, "wavelengths"_a,
        D(pdf_rgb_spectrum));

    m.def("xyz_to_srgb", &xyz_to_srgb<Float>,
          "rgb"_a, "active"_a = true, D(xyz_to_srgb));
    m.def("srgb_to_xyz", &srgb_to_xyz<Float>,
            "rgb"_a, "active"_a = true, D(srgb_to_xyz));

    if constexpr (is_spectral_v<Spectrum> || is_monochromatic_v<Spectrum>) {
        m.def("spectrum_to_xyz", &spectrum_to_xyz<Float, dr::size_v<Spectrum>>,
              "value"_a, "wavelengths"_a, "active"_a = true, D(spectrum_to_xyz));
        m.def("spectrum_to_srgb", &spectrum_to_srgb<Float, dr::size_v<Spectrum>>,
              "value"_a, "wavelengths"_a, "active"_a = true, D(spectrum_to_srgb));

        m.def("sample_shifted",
              nb::overload_cast<const dr::value_t<
                  dr::Array<Float, dr::size_v<Spectrum>>> &>(
                  &math::sample_shifted<
                      dr::Array<Float, dr::size_v<Spectrum>>>),
              "sample"_a);

        m.attr("MI_WAVELENGTH_SAMPLES") = dr::size_v<Spectrum>;
    }

    m.attr("MI_CIE_MIN") = MI_CIE_MIN;
    m.attr("MI_CIE_MAX") = MI_CIE_MAX;
    m.attr("MI_CIE_Y_NORMALIZATION") = MI_CIE_Y_NORMALIZATION;
    m.attr("MI_CIE_D65_NORMALIZATION") = MI_CIE_D65_NORMALIZATION;

    m.def("unpolarized_spectrum", [](const Spectrum &s) { return unpolarized_spectrum(s); }, "");
    m.def("depolarizer", [](const Spectrum &s) { return depolarizer(s); }, "");

    m.def("spectrum_list_to_srgb", &spectrum_list_to_srgb<ScalarFloat>,
          "wavelengths"_a, "values"_a, "bounded"_a=true, "d65"_a=false);

    m.def("spectrum_from_file",
        [] (const fs::path &filename) {
            std::vector<ScalarFloat> wavelengths, values;
            spectrum_from_file(filename, wavelengths, values);
            return std::make_pair(wavelengths, values);
        },
        "filename"_a, D(spectrum_from_file));

    m.def("spectrum_to_file", &spectrum_to_file<ScalarFloat>,
          "filename"_a, "wavelengths"_a, "values"_a, D(spectrum_to_file));
}
