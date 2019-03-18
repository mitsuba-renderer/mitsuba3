#include <mitsuba/core/spectrum.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(Spectrum) {
    m.def("to_xyz", vectorize_wrapper(&to_xyz<Spectrumf>), "value"_a, "wavelengths"_a,
          "active"_a = true, D(to_xyz))
        .def("to_xyz", vectorize_wrapper(&to_xyz<SpectrumfP>), "value"_a, "wavelengths"_a,
             "active"_a = true, D(to_xyz))

        .def("cie1931_xyz", vectorize_wrapper(&cie1931_xyz<Spectrumf>), "wavelengths"_a,
             "active"_a = true, D(cie1931_xyz))
        .def("cie1931_xyz", vectorize_wrapper([](const SpectrumfP &wavelengths) {
                 return cie1931_xyz<SpectrumfP>(wavelengths);
             }),
             "wavelengths"_a, D(cie1931_xyz))

        .def("cie1931_y", vectorize_wrapper(&cie1931_y<Spectrumf>), "wavelengths"_a,
             "active"_a = true, D(cie1931_y))
        .def("cie1931_y", vectorize_wrapper([](const SpectrumfP &wavelengths) {
                 return cie1931_y<SpectrumfP>(wavelengths);
             }),
             "wavelengths"_a, D(cie1931_y))

        .def("sample_rgb_spectrum", &sample_rgb_spectrum<Float>, "sample"_a, D(sample_rgb_spectrum))
        .def("sample_rgb_spectrum", vectorize_wrapper(&sample_rgb_spectrum<FloatP>), "sample"_a,
             D(sample_rgb_spectrum))
        .def("pdf_rgb_spectrum", &pdf_rgb_spectrum<Float>, "wavelengths"_a, D(pdf_rgb_spectrum))
        .def("pdf_rgb_spectrum", vectorize_wrapper(&pdf_rgb_spectrum<FloatP>), "wavelengths"_a,
             D(pdf_rgb_spectrum))

        .def("sample_uniform_spectrum", &sample_uniform_spectrum<Float>, "sample"_a,
             D(sample_uniform_spectrum))
        .def("sample_uniform_spectrum", vectorize_wrapper(&sample_uniform_spectrum<FloatP>),
             "sample"_a, D(sample_uniform_spectrum))
        .def("pdf_uniform_spectrum", &pdf_uniform_spectrum<Float>, "wavelengths"_a,
             D(pdf_uniform_spectrum))
        .def("pdf_uniform_spectrum", vectorize_wrapper(&pdf_uniform_spectrum<FloatP>),
             "wavelengths"_a, D(pdf_uniform_spectrum));

    m.attr("MTS_WAVELENGTH_SAMPLES") = MTS_WAVELENGTH_SAMPLES;
    m.attr("MTS_WAVELENGTH_MIN")     = MTS_WAVELENGTH_MIN;
    m.attr("MTS_WAVELENGTH_MAX")     = MTS_WAVELENGTH_MAX;
}
