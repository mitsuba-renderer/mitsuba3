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
             "lambda"_a, D(ContinuousSpectrum, sample, 2));

    m.def("cie1931_xyz", &cie1931_xyz, "lambda"_a, D(cie1931_y));
    m.def("cie1931_y", &cie1931_y, "lambda"_a, D(cie1931_y));
}
