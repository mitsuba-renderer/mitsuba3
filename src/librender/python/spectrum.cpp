#include <mitsuba/python/python.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/spectrum.h>

MTS_PY_EXPORT_CLASS_VARIANTS(ContinuousSpectrum) {
    using Wavelength = typename ContinuousSpectrum::Wavelength;
    using Mask = typename ContinuousSpectrum::Mask;
    using SurfaceInteraction3f = typename ContinuousSpectrum::SurfaceInteraction3f;

    auto cs = MTS_PY_CLASS(ContinuousSpectrum, Object)
        .def_static("D65", &ContinuousSpectrum::D65, "scale"_a = 1.f, "monochrome"_a = false)
        .def("mean", &ContinuousSpectrum::mean, D(ContinuousSpectrum, mean))

        .def("eval",
             py::overload_cast<const Wavelength &, Mask>(&ContinuousSpectrum::eval, py::const_),
             "wavelengths"_a, "active"_a = true, D(ContinuousSpectrum, eval))

        .def("eval",
             py::overload_cast<const SurfaceInteraction3f &, Mask>(
                 &ContinuousSpectrum::eval, py::const_),
             "si"_a, "active"_a = true, D(ContinuousSpectrum, eval))

        .def("sample",
             py::overload_cast<const Wavelength &, Mask>(&ContinuousSpectrum::sample, py::const_),
             "sample"_a, "active"_a = true, D(ContinuousSpectrum, sample))

        .def("sample",
             py::overload_cast<const SurfaceInteraction3f &, const Spectrum &, Mask>(
                 &ContinuousSpectrum::sample, py::const_),
             "si"_a, "sample"_a, "active"_a = true, D(ContinuousSpectrum, sample))

        .def("pdf",
             py::overload_cast<const Wavelength &, Mask>(
                 &ContinuousSpectrum::pdf, py::const_),
             "wavelengths"_a, "active"_a = true, D(ContinuousSpectrum, pdf))

        .def("pdf",
             py::overload_cast<const SurfaceInteraction3f &, Mask>(
                 &ContinuousSpectrum::pdf, py::const_),
             "si"_a, "active"_a = true, D(ContinuousSpectrum, pdf))
        ;

    // TODO
    // if constexpr (is_array_v<Float> && !is_dynamic_v<Float>) {
    //     cs.def("eval",
    //          vectorize_wrapper(py::overload_cast<const WavelengthP &, MaskP>(
    //              &ContinuousSpectrum::eval, py::const_)),
    //          "wavelengths"_a, "active"_a = true)
    //     .def("eval",
    //          vectorize_wrapper(py::overload_cast<const SurfaceInteraction3fP &, MaskP>(
    //              &ContinuousSpectrum::eval, py::const_)),
    //          "si"_a, "active"_a = true)
    //     .def("sample",
    //          vectorize_wrapper(py::overload_cast<const SpectrumfP &, MaskP>(
    //              &ContinuousSpectrum::sample, py::const_)),
    //          "sample"_a, "active"_a = true)
    //     .def("sample",
    //          vectorize_wrapper(
    //              py::overload_cast<const SurfaceInteraction3fP &, const SpectrumfP &, MaskP>(
    //                 &ContinuousSpectrum::sample, py::const_)),
    //          "si"_a, "sample"_a, "active"_a = true)
    //     .def("pdf",
    //          vectorize_wrapper(py::overload_cast<const SpectrumfP &, MaskP>(
    //              &ContinuousSpectrum::pdf, py::const_)),
    //          "wavelengths"_a, "active"_a = true)
    //     .def("pdf",
    //          vectorize_wrapper(py::overload_cast<const SurfaceInteraction3fP &, MaskP>(
    //              &ContinuousSpectrum::pdf, py::const_)),
    //          "si"_a, "active"_a = true)
    //     ;
    // }
}
