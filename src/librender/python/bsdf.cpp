#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/sampler.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/python/python.h>

template <typename Point3>
auto bind_bsdf_sample(py::module &m, const char *name) {
    using Type = BSDFSample<Point3>;
    using Vector3 = typename Type::Vector3;

    return py::class_<Type>(m, name, D(BSDFSample))
        .def(py::init<>(), D(BSDFSample, BSDFSample))
        .def(py::init<const Vector3 &>(), "wo"_a, D(BSDFSample, BSDFSample, 2))
        .def(py::init<const Type &>(), "bs"_a, "Copy constructor")
        .def("__repr__", [](const Type &bs) {
            std::ostringstream oss;
            oss << bs;
            return oss.str();
        })
        .def_readwrite("wo", &Type::wo, D(BSDFSample, wo))
        .def_readwrite("pdf", &Type::pdf, D(BSDFSample, pdf))
        .def_readwrite("eta", &Type::eta, D(BSDFSample, eta))
        .def_readwrite("sampled_type", &Type::sampled_type, D(BSDFSample, sampled_type))
        .def_readwrite("sampled_component", &Type::sampled_component, D(BSDFSample, sampled_component))
        ;
}

MTS_PY_EXPORT(BSDFContext) {
    py::class_<BSDFContext>(m, "BSDFContext", D(BSDFContext))
        .def(py::init<ETransportMode>(),
             "mode"_a = ERadiance, D(BSDFContext, BSDFContext))
        .def(py::init<ETransportMode, uint32_t, uint32_t>(),
             "mode"_a, "type_mak"_a, "component"_a, D(BSDFContext, BSDFContext, 2))
        .def(py::init<const BSDFContext &>(), "ctx"_a, "Copy constructor")
        .mdef(BSDFContext, reverse)
        .mdef(BSDFContext, is_enabled, "type"_a, "component"_a = 0)
        .def("__repr__", [](const BSDFContext &ctx) {
            std::ostringstream oss;
            oss << ctx;
            return oss.str();
        })
        .def_readwrite("mode", &BSDFContext::mode, D(BSDFContext, mode))
        .def_readwrite("type_mask", &BSDFContext::type_mask, D(BSDFContext, type_mask))
        .def_readwrite("component", &BSDFContext::component, D(BSDFContext, component))
        ;
}

MTS_PY_EXPORT(BSDFSample) {
    bind_bsdf_sample<Point3f>(m, "BSDFSample3f");
    auto bs3fx = bind_bsdf_sample<Point3fX>(m, "BSDFSample3fX");
    bind_slicing_operators<BSDFSample3fX, BSDFSample3f>(bs3fx);
}

MTS_PY_EXPORT(BSDF) {
    auto bsdf = MTS_PY_CLASS(BSDF, Object)
        .def("sample",
             py::overload_cast<const BSDFContext &, const SurfaceInteraction3f &,
                               Float, const Point2f &>(
                  &BSDF::sample, py::const_),
             "ctx"_a, "si"_a, "sample1"_a, "sample2"_a, D(BSDF, sample))
        .def("sample", enoki::vectorize_wrapper(
                 py::overload_cast<const BSDFContext &,
                                   const SurfaceInteraction3fP &,
                                   FloatP, const Point2fP &,
                                   MaskP>(&BSDF::sample, py::const_)),
             "ctx"_a, "si"_a, "sample1"_a, "sample2"_a, "active"_a = true,
             D(BSDF, sample))

        .def("eval",
             py::overload_cast<const BSDFContext &, const SurfaceInteraction3f &,
                               const Vector3f &>(
                 &BSDF::eval, py::const_),
              "ctx"_a, "si"_a, "wo"_a, D(BSDF, eval))
        .def("eval", enoki::vectorize_wrapper(
                 py::overload_cast<const BSDFContext &, const SurfaceInteraction3fP &,
                                   const Vector3fP &,
                                   MaskP>(&BSDF::eval, py::const_)),
             "ctx"_a, "si"_a, "wo"_a, "active"_a = true, D(BSDF, eval))

        .def("pdf",
             py::overload_cast<const BSDFContext &, const SurfaceInteraction3f &,
                               const Vector3f &>(&BSDF::pdf, py::const_),
             "ctx"_a, "si"_a, "wo"_a, D(BSDF, pdf))
        .def("pdf", enoki::vectorize_wrapper(
                 py::overload_cast<const BSDFContext &, const SurfaceInteraction3fP &,
                                   const Vector3fP &,
                                   MaskP>(&BSDF::pdf, py::const_)
             ),
             "ctx"_a, "si"_a, "wo"_a, "active"_a = true, D(BSDF, pdf))

        .def("sample_pol",
            py::overload_cast<const BSDFContext &, const SurfaceInteraction3f &,
                              Float, const Point2f &>(
                &BSDF::sample_pol, py::const_),
            "ctx"_a, "si"_a, "sample1"_a, "sample2"_a, D(BSDF, sample_pol))
        .def("sample_pol", enoki::vectorize_wrapper(
            py::overload_cast<const BSDFContext &,
                              const SurfaceInteraction3fP &,
                              FloatP, const Point2fP &,
                              MaskP>(&BSDF::sample_pol, py::const_)),
            "ctx"_a, "si"_a, "sample1"_a, "sample2"_a, "active"_a = true,
            D(BSDF, sample_pol))
        .def("eval_pol",
            py::overload_cast<const BSDFContext &, const SurfaceInteraction3f &,
                              const Vector3f &>(
                &BSDF::eval_pol, py::const_),
            "ctx"_a, "si"_a, "wo"_a, D(BSDF, eval_pol))
        .def("eval_pol", enoki::vectorize_wrapper(
            py::overload_cast<const BSDFContext &, const SurfaceInteraction3fP &,
                              const Vector3fP &,
                              MaskP>(&BSDF::eval_pol, py::const_)),
            "ctx"_a, "si"_a, "wo"_a, "active"_a = true, D(BSDF, eval_pol))

        .mdef(BSDF, needs_differentials)
        .mdef(BSDF, component_count)
        .def("flags", py::overload_cast<>(&BSDF::flags, py::const_), D(BSDF, flags))
        .def("flags", py::overload_cast<size_t>(&BSDF::flags, py::const_), D(BSDF, flags, 2))
        ;

    py::enum_<BSDF::EFlags>(bsdf, "EFlags", D(BSDF, EFlags), py::arithmetic())
        .value("ENull", BSDF::ENull, D(BSDF, EFlags, ENull))
        .value("EDiffuseReflection", BSDF::EDiffuseReflection, D(BSDF, EFlags, EDiffuseReflection))
        .value("EDiffuseTransmission", BSDF::EDiffuseTransmission, D(BSDF, EFlags, EDiffuseTransmission))
        .value("EGlossyReflection", BSDF::EGlossyReflection, D(BSDF, EFlags, EGlossyReflection))
        .value("EGlossyTransmission", BSDF::EGlossyTransmission, D(BSDF, EFlags, EGlossyTransmission))
        .value("EDeltaReflection", BSDF::EDeltaReflection, D(BSDF, EFlags, EDeltaReflection))
        .value("EDeltaTransmission", BSDF::EDeltaTransmission, D(BSDF, EFlags, EDeltaTransmission))
        .value("EAnisotropic", BSDF::EAnisotropic, D(BSDF, EFlags, EAnisotropic))
        .value("ESpatiallyVarying", BSDF::ESpatiallyVarying, D(BSDF, EFlags, ESpatiallyVarying))
        .value("ENonSymmetric", BSDF::ENonSymmetric, D(BSDF, EFlags, ENonSymmetric))
        .value("EFrontSide", BSDF::EFrontSide, D(BSDF, EFlags, EFrontSide))
        .value("EBackSide", BSDF::EBackSide, D(BSDF, EFlags, EBackSide))
        .export_values();

    py::enum_<BSDF::EFlagCombinations>(bsdf, "EFlagCombinations",
                                       D(BSDF, EFlagCombinations), py::arithmetic())
        .value("EReflection", BSDF::EReflection, D(BSDF, EFlagCombinations, EReflection))
        .value("ETransmission", BSDF::ETransmission, D(BSDF, EFlagCombinations, ETransmission))
        .value("EDiffuse", BSDF::EDiffuse, D(BSDF, EFlagCombinations, EDiffuse))
        .value("EGlossy", BSDF::EGlossy, D(BSDF, EFlagCombinations, EGlossy))
        .value("ESmooth", BSDF::ESmooth, D(BSDF, EFlagCombinations, ESmooth))
        .value("EDelta", BSDF::EDelta, D(BSDF, EFlagCombinations, EDelta))
        .value("EDelta1D", BSDF::EDelta1D, D(BSDF, EFlagCombinations, EDelta1D))
        .value("EAll", BSDF::EAll, D(BSDF, EFlagCombinations, EAll))
        .export_values();
}
