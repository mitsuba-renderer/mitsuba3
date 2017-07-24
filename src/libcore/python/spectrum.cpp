#include <mitsuba/core/spectrum.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(Spectrum) {
    using Mask = ContinuousSpectrum::Mask;

    MTS_PY_CLASS(ContinuousSpectrum, Object)
        .def("integral", &ContinuousSpectrum::integral, D(ContinuousSpectrum, integral))

        // ---------------------------------------------------------------------

        .def("eval",
             vectorize_wrapper(py::overload_cast<const Spectrumf &>(
                 &ContinuousSpectrum::eval, py::const_)),
             "lambda"_a, D(ContinuousSpectrum, eval))

        .def("eval",
             vectorize_wrapper(py::overload_cast<const SpectrumfP &, const Mask &>(
                 &ContinuousSpectrum::eval, py::const_)),
             "lambda"_a, "active"_a = true, D(ContinuousSpectrum, eval, 2))

        // ---------------------------------------------------------------------
        //
        .def("pdf",
             vectorize_wrapper(py::overload_cast<const Spectrumf &>(
                 &ContinuousSpectrum::pdf, py::const_)),
             "lambda"_a, D(ContinuousSpectrum, pdf))

        .def("pdf",
             vectorize_wrapper(py::overload_cast<const SpectrumfP &, const Mask &>(
                 &ContinuousSpectrum::pdf, py::const_)),
             "lambda"_a, "active"_a = true, D(ContinuousSpectrum, pdf, 2))

        // ---------------------------------------------------------------------

        .def("sample",
             vectorize_wrapper(py::overload_cast<const Spectrumf &>(
                 &ContinuousSpectrum::sample, py::const_)),
             "sample"_a, D(ContinuousSpectrum, sample))

        .def("sample",
             vectorize_wrapper(py::overload_cast<const SpectrumfP &, const Mask &>(
                 &ContinuousSpectrum::sample, py::const_)),
             "sample"_a, "active"_a = true, D(ContinuousSpectrum, sample, 2));

        // ---------------------------------------------------------------------

    MTS_PY_CLASS(InterpolatedSpectrum, ContinuousSpectrum)
        .def("__init__", [](InterpolatedSpectrum &s, Float min, Float max, const std::vector<Float> &values) {
                new (&s) InterpolatedSpectrum(min, max, values.size(), values.data());
        });

    m.def("cie1931_xyz", vectorize_wrapper(&cie1931_xyz<const Spectrumf &>), "lambda"_a, "active"_a = true, D(cie1931_y));
    m.def("cie1931_xyz", vectorize_wrapper(&cie1931_xyz<SpectrumfP>), "lambda"_a, "active"_a = true);
    m.def("cie1931_y", vectorize_wrapper(&cie1931_y<const Spectrumf &>), "lambda"_a, "active"_a = true, D(cie1931_y));
    m.def("cie1931_y", vectorize_wrapper(&cie1931_y<SpectrumfP>), "lambda"_a, "active"_a = true);

    m.attr("MTS_WAVELENGTH_SAMPLES") = MTS_WAVELENGTH_SAMPLES;
}
