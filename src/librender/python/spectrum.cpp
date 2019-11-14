#include <mitsuba/python/python.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/spectrum.h>

MTS_PY_EXPORT(ContinuousSpectrum) {
    MTS_IMPORT_TYPES(ContinuousSpectrum)
    MTS_PY_CHECK_ALIAS(ContinuousSpectrum, m) {
        MTS_PY_CLASS(ContinuousSpectrum, Object)
            .def_static("D65", &ContinuousSpectrum::D65, "scale"_a = 1.f)
            .def("mean", &ContinuousSpectrum::mean, D(ContinuousSpectrum, mean))
            .def("eval",
                vectorize<Float>(py::overload_cast<const Wavelength&, Mask>(
                    &ContinuousSpectrum::eval, py::const_)),
                "wavelengths"_a, "active"_a = true, D(ContinuousSpectrum, eval))
            .def("eval",
                vectorize<Float>(py::overload_cast<const SurfaceInteraction3f&, Mask>(
                    &ContinuousSpectrum::eval, py::const_)),
                "si"_a, "active"_a = true, D(ContinuousSpectrum, eval))
            .def("sample",
                vectorize<Float>(py::overload_cast<const Wavelength&, Mask>(
                    &ContinuousSpectrum::sample, py::const_)),
                "sample"_a, "active"_a = true, D(ContinuousSpectrum, sample))
            .def("sample",
                vectorize<Float>(
                    py::overload_cast<const SurfaceInteraction3f&, const Spectrum&, Mask>(
                        &ContinuousSpectrum::sample, py::const_)),
                "si"_a, "sample"_a, "active"_a = true, D(ContinuousSpectrum, sample))
            .def("pdf",
                vectorize<Float>(py::overload_cast<const Wavelength&, Mask>(
                    &ContinuousSpectrum::pdf, py::const_)),
                "wavelengths"_a, "active"_a = true, D(ContinuousSpectrum, pdf))
            .def("pdf",
                vectorize<Float>(py::overload_cast<const SurfaceInteraction3f&, Mask>(
                    &ContinuousSpectrum::pdf, py::const_)),
                "si"_a, "active"_a = true, D(ContinuousSpectrum, pdf));
    }
}
