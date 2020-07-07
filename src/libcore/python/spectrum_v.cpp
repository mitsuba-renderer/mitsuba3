#include <mitsuba/core/fwd.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(Spectrum) {
    MTS_PY_IMPORT_TYPES()

    m.def("luminance", vectorize([](const UnpolarizedSpectrum &value, const UnpolarizedSpectrum& w, Mask active) {
            return luminance<Float, UnpolarizedSpectrum::Size>(value, w, active);
        }), "value"_a, "wavelengths"_a, "active"_a = true, D(luminance))
    .def("luminance", vectorize([](Color<Float, 3> c) {
            return luminance(c);
        }), "c"_a, D(luminance))
    .def("cie1931_xyz", vectorize([](Float wavelengths) {
            return cie1931_xyz(wavelengths);
        }), "wavelength"_a, D(cie1931_xyz))
    .def("cie1931_y", vectorize([](Float wavelengths) {
            return cie1931_y(wavelengths);
        }), "wavelength"_a, D(cie1931_y))
    .def("sample_rgb_spectrum", vectorize(&sample_rgb_spectrum<Float>), "sample"_a,
        D(sample_rgb_spectrum))
    .def("sample_rgb_spectrum", vectorize(&sample_rgb_spectrum<Spectrum>), "sample"_a,
        D(sample_rgb_spectrum))
    .def("pdf_rgb_spectrum", vectorize(&pdf_rgb_spectrum<Float>), "wavelengths"_a,
        D(pdf_rgb_spectrum))
    .def("pdf_rgb_spectrum", vectorize(&pdf_rgb_spectrum<Spectrum>), "wavelengths"_a,
        D(pdf_rgb_spectrum))
    .def("sample_uniform_spectrum", vectorize(&sample_uniform_spectrum<Float>),
        "sample"_a, D(sample_uniform_spectrum))
    .def("sample_uniform_spectrum", vectorize(&sample_uniform_spectrum<Spectrum>),
        "sample"_a, D(sample_uniform_spectrum))
    .def("pdf_uniform_spectrum", vectorize(&pdf_uniform_spectrum<Float>),
        "wavelengths"_a, D(pdf_uniform_spectrum))
    .def("pdf_uniform_spectrum", vectorize(&pdf_uniform_spectrum<Spectrum>),
        "wavelengths"_a, D(pdf_uniform_spectrum));

    m.def("xyz_to_srgb", vectorize(&xyz_to_srgb<Float>),
          "rgb"_a, "active"_a = true, D(xyz_to_srgb));

    if constexpr (is_rgb_v<Spectrum>) {
        m.def("srgb_to_xyz", vectorize(&srgb_to_xyz<Float>),
              "rgb"_a, "active"_a = true, D(srgb_to_xyz));
    }

    if constexpr (is_spectral_v<Spectrum>) {
        m.def("spectrum_to_xyz", vectorize(&spectrum_to_xyz<Float, array_size_v<Spectrum>>),
              "value"_a, "wavelengths"_a, "active"_a = true, D(spectrum_to_xyz));

        m.def("sample_shifted",
            vectorize(
                py::overload_cast<const value_t<Array<Float, array_size_v<Spectrum>>> &>(
                    &math::sample_shifted<Array<Float, array_size_v<Spectrum>>>)),
            "sample"_a);

        m.attr("MTS_WAVELENGTH_SAMPLES") = array_size_v<Spectrum>;
        m.attr("MTS_WAVELENGTH_MIN")     = MTS_WAVELENGTH_MIN;
        m.attr("MTS_WAVELENGTH_MAX")     = MTS_WAVELENGTH_MAX;
    }

    m.def("unpolarized", vectorize([](const Spectrum &s) { return unpolarized(s); }), "");
    m.def("depolarize", vectorize([](const Spectrum &s) { return depolarize(s); }), "");

    if constexpr (is_cuda_array_v<Float>) {
        py::module::import("enoki");
        pybind11_type_alias<Array<Float, 3>, Color3f>();
        pybind11_type_alias<Array<Float, array_size_v<Spectrum>>, Spectrum>();
    }
}
