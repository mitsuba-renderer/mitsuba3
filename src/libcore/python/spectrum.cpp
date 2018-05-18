#include <mitsuba/core/spectrum.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(Spectrum) {
    MTS_PY_CLASS(ContinuousSpectrum, Object)
        .def("integral", &ContinuousSpectrum::integral, D(ContinuousSpectrum, integral))

        // ---------------------------------------------------------------------

        .def("eval",
             vectorize_wrapper(py::overload_cast<const Spectrumf &>(
                 &ContinuousSpectrum::eval, py::const_)),
             "wavelengths"_a, D(ContinuousSpectrum, eval))

        .def("eval",
             vectorize_wrapper(py::overload_cast<const SpectrumfP &, MaskP>(
                 &ContinuousSpectrum::eval, py::const_)),
             "wavelengths"_a, "active"_a = true, D(ContinuousSpectrum, eval, 2))

        // ---------------------------------------------------------------------

        .def("pdf",
             vectorize_wrapper(py::overload_cast<const Spectrumf &>(
                 &ContinuousSpectrum::pdf, py::const_)),
             "wavelengths"_a, D(ContinuousSpectrum, pdf))

        .def("pdf",
             vectorize_wrapper(py::overload_cast<const SpectrumfP &, MaskP>(
                 &ContinuousSpectrum::pdf, py::const_)),
             "wavelengths"_a, "active"_a = true, D(ContinuousSpectrum, pdf, 2))

        // ---------------------------------------------------------------------

        .def("sample",
             vectorize_wrapper(py::overload_cast<const Spectrumf &>(
                 &ContinuousSpectrum::sample, py::const_)),
             "sample"_a, D(ContinuousSpectrum, sample))

        .def("sample",
             vectorize_wrapper(py::overload_cast<const SpectrumfP &, MaskP>(
                 &ContinuousSpectrum::sample, py::const_)),
             "sample"_a, "active"_a = true, D(ContinuousSpectrum, sample, 2));

        // ---------------------------------------------------------------------

    m.def("cie1931_xyz", vectorize_wrapper(&cie1931_xyz<const Spectrumf &>),
          "wavelengths"_a, "active"_a = true, D(cie1931_xyz))
     .def("cie1931_xyz", vectorize_wrapper(&cie1931_xyz<SpectrumfP>),
          "wavelengths"_a, "active"_a = true, D(cie1931_xyz))

     .def("cie1931_y", vectorize_wrapper(&cie1931_y<const Spectrumf &>),
          "wavelengths"_a, "active"_a = true, D(cie1931_y))
     .def("cie1931_y", vectorize_wrapper(&cie1931_y<SpectrumfP>),
          "wavelengths"_a, "active"_a = true, D(cie1931_y))

     //.def("luminance", vectorize_wrapper(&luminance<const Spectrumf &>),
          //"spectrum"_a, "wavelengths"_a, "active"_a = true, D(luminance))
     //.def("luminance", vectorize_wrapper(&luminance<SpectrumfP>),
          //"spectrum"_a, "wavelengths"_a, "active"_a = true, D(luminance))

     .def("sample_rgb_spectrum", vectorize_wrapper(&sample_rgb_spectrum<Float>),
          "sample"_a, D(sample_rgb_spectrum))
     .def("sample_rgb_spectrum", vectorize_wrapper(&sample_rgb_spectrum<FloatP>),
          "sample"_a, D(sample_rgb_spectrum))
     ;

    m.attr("MTS_WAVELENGTH_SAMPLES") = MTS_WAVELENGTH_SAMPLES;
    m.attr("MTS_WAVELENGTH_MIN") = MTS_WAVELENGTH_MIN;
    m.attr("MTS_WAVELENGTH_MAX") = MTS_WAVELENGTH_MAX;
}
