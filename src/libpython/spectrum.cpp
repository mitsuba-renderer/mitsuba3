#include <mitsuba/core/spectrum.h>
#include "python.h"

MTS_PY_EXPORT(Spectrum) {
    MTS_PY_CLASS(ContinuousSpectrum, Object)
        .def("eval",
             py::overload_cast<DiscreteSpectrum>(
                 &ContinuousSpectrum::eval, py::const_),
             "lambda"_a, D(ContinuousSpectrum, eval))
        .def("eval",
             vectorize_wrapper(py::overload_cast<DiscreteSpectrum>(
                 &ContinuousSpectrum::eval, py::const_)),
             "lambda"_a, D(ContinuousSpectrum, eval, 2))
        .def("pdf",
             py::overload_cast<DiscreteSpectrum>(
                 &ContinuousSpectrum::pdf, py::const_),
             "lambda"_a, D(ContinuousSpectrum, pdf))
        .def("pdf",
             vectorize_wrapper(py::overload_cast<DiscreteSpectrum>(
                 &ContinuousSpectrum::pdf, py::const_)),
             "lambda"_a, D(ContinuousSpectrum, pdf, 2))
        .def("sample",
             py::overload_cast<Float>(
                 &ContinuousSpectrum::sample, py::const_),
             "lambda"_a, D(ContinuousSpectrum, sample))
        .def("sample",
             vectorize_wrapper(py::overload_cast<FloatP>(
                 &ContinuousSpectrum::sample, py::const_)),
             "lambda"_a, D(ContinuousSpectrum, sample, 2))
        .def("integral", &ContinuousSpectrum::integral,
             D(ContinuousSpectrum, integral));

    MTS_PY_CLASS(InterpolatedSpectrum, ContinuousSpectrum)
        .def("__init__", [](InterpolatedSpectrum &s, Float min, Float max, const std::vector<Float> &values) {
                new (&s) InterpolatedSpectrum(min, max, values.size(), values.data());
        });

    m.def("cie1931_xyz", &cie1931_xyz<DiscreteSpectrum>, "lambda"_a, D(cie1931_y));
    m.def("cie1931_xyz", vectorize_wrapper(&cie1931_xyz<DiscreteSpectrumP>), "lambda"_a);
    m.def("cie1931_y", &cie1931_y<DiscreteSpectrum>, "lambda"_a, D(cie1931_y));
    m.def("cie1931_y", vectorize_wrapper(&cie1931_y<DiscreteSpectrumP>), "lambda"_a);

    m.attr("MTS_WAVELENGTH_SAMPLES") = MTS_WAVELENGTH_SAMPLES;
}
