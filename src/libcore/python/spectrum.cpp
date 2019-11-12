#include <mitsuba/core/spectrum.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT_VARIANTS(Spectrum) {
    m.def("to_xyz", vectorize<Float>(&to_xyz<SpectrumP>),
        "value"_a, "wavelengths"_a, "active"_a = true, D(to_xyz))
    .def("cie1931_xyz", vectorize<Float>([](const SpectrumP &wavelengths) {
            return cie1931_xyz<SpectrumP>(wavelengths);
        }), "wavelengths"_a, D(cie1931_xyz))
    .def("cie1931_y", vectorize<Float>([](const SpectrumP &wavelengths) {
            return cie1931_y<SpectrumP>(wavelengths);
        }), "wavelengths"_a, D(cie1931_y))
    .def("sample_rgb_spectrum", vectorize<Float>(&sample_rgb_spectrum<FloatP>), "sample"_a,
            D(sample_rgb_spectrum))
    .def("pdf_rgb_spectrum", vectorize<Float>(&pdf_rgb_spectrum<FloatP>), "wavelengths"_a,
            D(pdf_rgb_spectrum))

    .def("sample_uniform_spectrum", vectorize<Float>(&sample_uniform_spectrum<FloatP>),
            "sample"_a, D(sample_uniform_spectrum))
    .def("pdf_uniform_spectrum", vectorize<Float>(&pdf_uniform_spectrum<FloatP>),
            "wavelengths"_a, D(pdf_uniform_spectrum))
    ;

    m.attr("MTS_WAVELENGTH_SAMPLES") = MTS_WAVELENGTH_SAMPLES;
    m.attr("MTS_WAVELENGTH_MIN")     = MTS_WAVELENGTH_MIN;
    m.attr("MTS_WAVELENGTH_MAX")     = MTS_WAVELENGTH_MAX;
}
