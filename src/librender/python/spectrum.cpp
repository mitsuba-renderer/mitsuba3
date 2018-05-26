#include <mitsuba/render/spectrum.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(ContinuousSpectrum) {
    MTS_PY_CLASS(ContinuousSpectrum, Object)
        .def("mean", &ContinuousSpectrum::mean, D(ContinuousSpectrum, mean))

        // ---------------------------------------------------------------------

        .def("eval",
             vectorize_wrapper(py::overload_cast<const Spectrumf &>(
                 &ContinuousSpectrum::eval, py::const_)),
             "wavelengths"_a, D(ContinuousSpectrum, eval))

        .def("eval",
             vectorize_wrapper(py::overload_cast<const SpectrumfP &, MaskP>(
                 &ContinuousSpectrum::eval, py::const_)),
             "wavelengths"_a, "active"_a = true)

        // ---------------------------------------------------------------------

        .def("eval",
             vectorize_wrapper(py::overload_cast<const SurfaceInteraction3f &>(
                 &ContinuousSpectrum::eval, py::const_)), "si"_a, D(ContinuousSpectrum, eval))

        .def("eval",
             vectorize_wrapper(py::overload_cast<const SurfaceInteraction3fP &, MaskP>(
                 &ContinuousSpectrum::eval, py::const_)), "si"_a, "active"_a = true)

        // ---------------------------------------------------------------------

        .def("sample",
             vectorize_wrapper(py::overload_cast<const Spectrumf &>(
                 &ContinuousSpectrum::sample, py::const_)),
             "sample"_a, D(ContinuousSpectrum, sample))

        .def("sample",
             vectorize_wrapper(py::overload_cast<const SpectrumfP &, MaskP>(
                 &ContinuousSpectrum::sample, py::const_)),
             "sample"_a, "active"_a = true)

        // ---------------------------------------------------------------------

        .def("sample",
             vectorize_wrapper(py::overload_cast<const SurfaceInteraction3f &, const Spectrumf &>(
                 &ContinuousSpectrum::sample, py::const_)),
             "si"_a, "sample"_a, D(ContinuousSpectrum, sample))

        .def("sample",
             vectorize_wrapper(py::overload_cast<const SurfaceInteraction3fP &, const SpectrumfP &, MaskP>(
                 &ContinuousSpectrum::sample, py::const_)),
             "si"_a, "sample"_a, "active"_a = true)


        // ---------------------------------------------------------------------

        .def("pdf",
             vectorize_wrapper(py::overload_cast<const Spectrumf &>(
                 &ContinuousSpectrum::pdf, py::const_)),
             "wavelengths"_a, D(ContinuousSpectrum, pdf))

        .def("pdf",
             vectorize_wrapper(py::overload_cast<const SpectrumfP &, MaskP>(
                 &ContinuousSpectrum::pdf, py::const_)),
             "wavelengths"_a, "active"_a = true)

        // ---------------------------------------------------------------------

        .def("pdf",
             vectorize_wrapper(py::overload_cast<const SurfaceInteraction3f &>(
                 &ContinuousSpectrum::pdf, py::const_)),
             "si"_a, D(ContinuousSpectrum, pdf))

        .def("pdf",
             vectorize_wrapper(py::overload_cast<const SurfaceInteraction3fP &, MaskP>(
                 &ContinuousSpectrum::pdf, py::const_)),
             "si"_a, "active"_a = true);

        // ---------------------------------------------------------------------
}
