#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/python/python.h>

template <typename Point3>
auto bind_bsdf_sample(py::module &m, const char *name) {
    using Type = BSDFSample<Point3>;
    using SurfaceInteraction = typename Type::SurfaceInteraction;
    using Vector3 = typename Type::Vector3;

    return py::class_<Type>(m, name, D(BSDFSample))
        .def(py::init<const SurfaceInteraction&, Sampler*, ETransportMode>(),
            "its"_a, "sampler"_a, "mode"_a = ERadiance,
            D(BSDFSample, BSDFSample))
        .def(py::init<const SurfaceInteraction&, const Vector3&, ETransportMode>(),
            "its"_a, "wo"_a, "mode"_a = ERadiance,
            D(BSDFSample, BSDFSample, 2))
        .def(py::init<const SurfaceInteraction&, const Vector3&, const Vector3&, ETransportMode>(),
            "its"_a, "wi"_a, "wo"_a, "mode"_a = ERadiance,
            D(BSDFSample, BSDFSample, 3))
        .def("reverse", &Type::reverse, D(BSDFSample, reverse))
        .def("__repr__", [](const Type &bs) {
            std::ostringstream oss;
            oss << bs;
            return oss.str();
        })
        .def_property_readonly("its",
            [](const Type &bs) -> const SurfaceInteraction &{ return bs.its; },
            py::return_value_policy::reference_internal)
        .def_readwrite("sampler", &Type::sampler, D(BSDFSample, sampler))
        .def_readwrite("wi", &Type::wi, D(BSDFSample, wi))
        .def_readwrite("wo", &Type::wo, D(BSDFSample, wo))
        .def_readwrite("eta", &Type::eta, D(BSDFSample, eta))
        .def_readwrite("mode", &Type::mode, D(BSDFSample, mode))
        .def_readwrite("type_mask", &Type::type_mask, D(BSDFSample, type_mask))
        .def_readwrite("component", &Type::component, D(BSDFSample, component))
        .def_readwrite("sampled_type", &Type::sampled_type, D(BSDFSample, sampled_type))
        .def_readwrite("sampled_component", &Type::sampled_component, D(BSDFSample, sampled_component));
}

MTS_PY_EXPORT(BSDFSample) {
    bind_bsdf_sample<Point3f>(m, "BSDFSample3f");
    auto bs3fx = bind_bsdf_sample<Point3fX>(m, "BSDFSample3fX");
    // TODO compilation error: attempting to reference a deleted function
    //bind_slicing_operators<BSDFSample3fX, BSDFSample3f>(bs3fx);
}

MTS_PY_EXPORT(BSDF) {
    auto bsdf = MTS_PY_CLASS(BSDF, Object)
       .def("sample",
           py::overload_cast<BSDFSample3f&, const Point2f&>(
                &BSDF::sample, py::const_),
           D(BSDF, sample), "bs"_a, "sample"_a = ESolidAngle)
        //.def("sample", enoki::vectorize_wrapper(
        //    py::overload_cast<BSDFSample3fP&, const Point2fP&, const mask_t<FloatP>&>(
        //        &BSDF::sample, py::const_)))
                      //D(BSDF, sample), "bs"_a, "sample"_a)

        .def("eval",
             py::overload_cast<const BSDFSample3f&, EMeasure>(
                 &BSDF::eval, py::const_),
             D(BSDF, eval), "bs"_a, "measure"_a = ESolidAngle)
        //.def("eval", enoki::vectorize_wrapper(
        //    py::overload_cast<const BSDFSample3fP&, EMeasure, const mask_t<FloatP>&>(
        //        &BSDF::eval, py::const_)))
                      //D(BSDF, eval), "bs"_a, "measure"_a)

        .def("pdf",
             py::overload_cast<const BSDFSample3f&, EMeasure>(
                 &BSDF::pdf, py::const_),
             D(BSDF, pdf), "bs"_a, "measure"_a = ESolidAngle)
        //.def("pdf", enoki::vectorize_wrapper(
        //    py::overload_cast<const BSDFSample3fP&, EMeasure, mask_t<FloatP>>(
        //        &BSDF::pdf, py::const_)))
                      //D(BSDF, pdf), "bs"_a, "measure"_a)

        .def("needs_differentials", &BSDF::needs_differentials,
             D(BSDF, needs_differentials))
        .def("flags", &BSDF::flags, D(BSDF, flags))
    ;

    py::enum_<BSDF::EFlags>(bsdf, "EFlags",
        "This list of flags is used to classify the different"
        " types of lobes that are implemented in a BSDF instance.",
        py::arithmetic())
        .value("ENull", BSDF::ENull)
                       //D(BSDF_EFlags, ENull))
        .value("EDiffuseReflection", BSDF::EDiffuseReflection)
                       //D(BSDF_EFlags, EDiffuseReflection))
        .value("EDiffuseTransmission", BSDF::EDiffuseTransmission)
                       //D(BSDF_EFlags, EDiffuseTransmission))
        .value("EGlossyReflection", BSDF::EGlossyReflection)
                       //D(BSDF_EFlags, EGlossyReflection))
        .value("EGlossyTransmission", BSDF::EGlossyTransmission)
                       //D(BSDF_EFlags, EGlossyTransmission))
        .value("EDeltaReflection", BSDF::EDeltaReflection)
                       //D(BSDF_EFlags, EDeltaReflection))
        .value("EDeltaTransmission", BSDF::EDeltaTransmission)
                       //D(BSDF_EFlags, EDeltaTransmission))
        .value("EAnisotropic", BSDF::EAnisotropic)
                       //D(BSDF_EFlags, EAnisotropic))
        .value("ESpatiallyVarying", BSDF::ESpatiallyVarying)
                       //D(BSDF_EFlags, ESpatiallyVarying))
        .value("ENonSymmetric", BSDF::ENonSymmetric)
                       //D(BSDF_EFlags, ENonSymmetric))
        .value("EFrontSide", BSDF::EFrontSide)
                       //D(BSDF_EFlags, EFrontSide))
        .value("EBackSide", BSDF::EBackSide)
                       //D(BSDF_EFlags, EBackSide))
        .value("EUsesSampler", BSDF::EUsesSampler)
                       //D(BSDF_EFlags, EUsesSampler))
        .export_values();
    py::enum_<BSDF::EFlagCombinations>(bsdf, "EFlagCombinations",
        "Convenient combinations of flags from \ref EBSDFType",
        py::arithmetic())
        .value("EReflection", BSDF::EReflection)
                       //D(BSDF_EFlagCombinations, EReflection))
        .value("ETransmission", BSDF::ETransmission)
                       //D(BSDF_EFlagCombinations, ETransmission))
        .value("EDiffuse", BSDF::EDiffuse)
                       //D(BSDF_EFlagCombinations, EDiffuse))
        .value("EGlossy", BSDF::EGlossy)
                       //D(BSDF_EFlagCombinations, EGlossy))
        .value("ESmooth", BSDF::ESmooth)
                       //D(BSDF_EFlagCombinations, ESmooth))
        .value("EDelta", BSDF::EDelta)
                       //D(BSDF_EFlagCombinations, EDelta))
        .value("EDelta1D", BSDF::EDelta1D)
                       //D(BSDF_EFlagCombinations, EDelta1D))
        .value("EAll", BSDF::EAll)
                       //D(BSDF_EFlagCombinations, EAll))
        .export_values();
}
