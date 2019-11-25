#include <mitsuba/core/spectrum.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(Spectrum) {
   m.def("cie1931_xyz", vectorize<Float>([](Float wavelengths) {
            return cie1931_xyz(wavelengths);
        }), "wavelength"_a, D(cie1931_xyz))
    .def("cie1931_y", vectorize<Float>([](Float wavelengths) {
            return cie1931_y(wavelengths);
        }), "wavelength"_a, D(cie1931_y))
    .def("sample_rgb_spectrum", vectorize<Float>(&sample_rgb_spectrum<Float>), "sample"_a,
        D(sample_rgb_spectrum))
    .def("pdf_rgb_spectrum", vectorize<Float>(&pdf_rgb_spectrum<Float>), "wavelengths"_a,
        D(pdf_rgb_spectrum))
    .def("sample_uniform_spectrum", vectorize<Float>(&sample_uniform_spectrum<Float>),
        "sample"_a, D(sample_uniform_spectrum))
    .def("pdf_uniform_spectrum", vectorize<Float>(&pdf_uniform_spectrum<Float>),
        "wavelengths"_a, D(pdf_uniform_spectrum));

    if constexpr (is_spectral_v<Spectrum>) {
        m.def("spectrum_to_xyz", vectorize<Float>(&spectrum_to_xyz<Float, array_size_v<Spectrum>>),
              "value"_a, "wavelengths"_a, "active"_a = true, D(to_xyz));

        m.attr("MTS_WAVELENGTH_SAMPLES") = array_size_v<Spectrum>;
        m.attr("MTS_WAVELENGTH_MIN")     = MTS_WAVELENGTH_MIN;
        m.attr("MTS_WAVELENGTH_MAX")     = MTS_WAVELENGTH_MAX;
    }
}
