#include <mitsuba/core/fwd.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(Spectrum) {
    MTS_PY_IMPORT_TYPES()

    m.def("luminance", [](const UnpolarizedSpectrum &value, const UnpolarizedSpectrum& w, Mask active) {
            return luminance<Float, UnpolarizedSpectrum::Size>(value, w, active);
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
    .def("sample_rgb_spectrum", &sample_rgb_spectrum<Float>, "sample"_a,
        D(sample_rgb_spectrum))
    .def("sample_rgb_spectrum", &sample_rgb_spectrum<Spectrum>, "sample"_a,
        D(sample_rgb_spectrum))
    .def("pdf_rgb_spectrum", &pdf_rgb_spectrum<Float>, "wavelengths"_a,
        D(pdf_rgb_spectrum))
    .def("pdf_rgb_spectrum", &pdf_rgb_spectrum<Spectrum>, "wavelengths"_a,
        D(pdf_rgb_spectrum))
    .def("sample_uniform_spectrum", &sample_uniform_spectrum<Float>,
        "sample"_a, D(sample_uniform_spectrum))
    .def("sample_uniform_spectrum", &sample_uniform_spectrum<Spectrum>,
        "sample"_a, D(sample_uniform_spectrum))
    .def("pdf_uniform_spectrum", &pdf_uniform_spectrum<Float>,
        "wavelengths"_a, D(pdf_uniform_spectrum))
    .def("pdf_uniform_spectrum", &pdf_uniform_spectrum<Spectrum>,
        "wavelengths"_a, D(pdf_uniform_spectrum));

    m.def("xyz_to_srgb", &xyz_to_srgb<Float>,
          "rgb"_a, "active"_a = true, D(xyz_to_srgb));
    m.def("srgb_to_xyz", &srgb_to_xyz<Float>,
            "rgb"_a, "active"_a = true, D(srgb_to_xyz));

    if constexpr (is_spectral_v<Spectrum> || is_monochromatic_v<Spectrum>) {
        m.def("spectrum_to_xyz", &spectrum_to_xyz<Float, ek::array_size_v<Spectrum>>,
              "value"_a, "wavelengths"_a, "active"_a = true, D(spectrum_to_xyz));
        m.def("spectrum_to_srgb", &spectrum_to_srgb<Float, ek::array_size_v<Spectrum>>,
              "value"_a, "wavelengths"_a, "active"_a = true, D(spectrum_to_srgb));

        m.def("sample_shifted",
              py::overload_cast<const ek::value_t<
                  ek::Array<Float, ek::array_size_v<Spectrum>>> &>(
                  &math::sample_shifted<
                      ek::Array<Float, ek::array_size_v<Spectrum>>>),
              "sample"_a);

        m.attr("MTS_WAVELENGTH_SAMPLES") = ek::array_size_v<Spectrum>;
        m.attr("MTS_WAVELENGTH_MIN")     = MTS_WAVELENGTH_MIN;
        m.attr("MTS_WAVELENGTH_MAX")     = MTS_WAVELENGTH_MAX;
    }

    m.def("unpolarized_spectrum", [](const Spectrum &s) { return unpolarized_spectrum(s); }, "");
    m.def("depolarizer", [](const Spectrum &s) { return depolarizer(s); }, "");
}
