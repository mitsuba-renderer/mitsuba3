#include <mitsuba/render/bsdf.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(BSDFContext) {
    py::class_<BSDFContext>(m, "BSDFContext", D(BSDFContext))
        .def(py::init<ETransportMode>(),
             "mode"_a = ERadiance, D(BSDFContext, BSDFContext))
        .def(py::init<ETransportMode, uint32_t, uint32_t>(),
             "mode"_a, "type_mak"_a, "component"_a, D(BSDFContext, BSDFContext, 2))
        .def(py::init<const BSDFContext &>(), "ctx"_a, "Copy constructor")
        .mdef(BSDFContext, reverse)
        .mdef(BSDFContext, is_enabled, "type"_a, "component"_a = 0)
        .def("__repr__",
            [](const BSDFContext &ctx) {
                std::ostringstream oss;
                oss << ctx;
                return oss.str();
            })
        .def_readwrite("mode", &BSDFContext::mode, D(BSDFContext, mode))
        .def_readwrite("type_mask", &BSDFContext::type_mask, D(BSDFContext, type_mask))
        .def_readwrite("component", &BSDFContext::component, D(BSDFContext, component))
        ;
}

MTS_PY_EXPORT_VARIANTS(BSDFSample3) {
    using Vector3f = typename BSDFSample3::Vector3f;

    py::class_<BSDFSample3>(m, "BSDFSample3f", D(BSDFSample3))
        .def(py::init<>(), D(BSDFSample3, BSDFSample3))
        .def(py::init<const Vector3f &>(), "wo"_a, D(BSDFSample3, BSDFSample3, 2))
        .def(py::init<const BSDFSample3 &>(), "bs"_a, "Copy constructor")
        .def("__repr__",
             [](const Type &bs) {
                 std::ostringstream oss;
                 oss << bs;
                 return oss.str();
             })
        .def_readwrite("wo", &BSDFSample3::wo, D(BSDFSample3, wo))
        .def_readwrite("pdf", &BSDFSample3::pdf, D(BSDFSample3, pdf))
        .def_readwrite("eta", &BSDFSample3::eta, D(BSDFSample3, eta))
        .def_readwrite("sampled_type", &BSDFSample3::sampled_type, D(BSDFSample3, sampled_type))
        .def_readwrite("sampled_component", &BSDFSample3::sampled_component,
                       D(BSDFSample3, sampled_component));
}

MTS_PY_EXPORT_VARIANTS(BSDF) {
    MTS_PY_CLASS(BSDF, Object)
        .def("sample", &BSDF::sample
             "ctx"_a, "si"_a, "sample1"_a, "sample2"_a, "active"_a = true, D(BSDF, sample))
        .def("sample", enoki::vectorize_wrapper(&BSDF::sample),
             "ctx"_a, "si"_a, "sample1"_a, "sample2"_a, "active"_a = true, D(BSDF, sample))

        .def("eval", &BSDF::eval,
             "ctx"_a, "si"_a, "wo"_a, "active"_a = true, D(BSDF, eval))
        .def("eval", enoki::vectorize_wrapper(&BSDF::eval),
             "ctx"_a, "si"_a, "wo"_a, "active"_a = true, D(BSDF, eval))

        .def("pdf", &BSDF::pdf,,
             "ctx"_a, "si"_a, "wo"_a, "active"_a = true, D(BSDF, pdf))
        .def("pdf", enoki::vectorize_wrapper(&BSDF::pdf),
             "ctx"_a, "si"_a, "wo"_a, "active"_a = true, D(BSDF, pdf))

        .def("eval_tr", &BSDF::eval_tr,
             "si"_a, "active"_a = true, D(BSDF, eval))
        .def("eval_tr", enoki::vectorize_wrapper(&BSDF::eval_tr),
             "si"_a, "active"_a = true, D(BSDF, eval))

        .def("flags", py::overload_cast<>(&BSDF::flags, py::const_), D(BSDF, flags))
        .def("flags", py::overload_cast<size_t>(&BSDF::flags, py::const_), D(BSDF, flags, 2))

        .mdef(BSDF, needs_differentials)
        .mdef(BSDF, component_count)
        .mdef(BSDF, id)
        .def("__repr__", &BSDF::to_string)
        ;

    // TODO is this necessary?
    // m.attr("BSDFFlags") = py::module::import("mitsuba.render.BSDFFlags");
}

MTS_PY_EXPORT(TransportMode) {
    py::enum_<TransportMode>(m, "TransportMode", D(TransportMode))
        .value("Radiance", Radiance, D(TransportMode, Radiance))
        .value("Importance", Importance, D(TransportMode, Importance))
        .export_values();
}

MTS_PY_EXPORT(BSDFFlags) {
    py::enum_<BSDFFlags>(m, "BSDFFlags", D(BSDFFlags), py::arithmetic())
        .value("None", BSDFFlags::ENull, D(BSDFFlags, None))
        .value("Null", BSDFFlags::ENull, D(BSDFFlags, Null))
        .value("DiffuseReflection", BSDFFlags::EDiffuseReflection, D(BSDFFlags, DiffuseReflection))
        .value("DiffuseTransmission", BSDFFlags::EDiffuseTransmission, D(BSDFFlags, DiffuseTransmission))
        .value("GlossyReflection", BSDFFlags::EGlossyReflection, D(BSDFFlags, GlossyReflection))
        .value("GlossyTransmission", BSDFFlags::EGlossyTransmission, D(BSDFFlags, GlossyTransmission))
        .value("DeltaReflection", BSDFFlags::EDeltaReflection, D(BSDFFlags, DeltaReflection))
        .value("DeltaTransmission", BSDFFlags::EDeltaTransmission, D(BSDFFlags, DeltaTransmission))
        .value("Anisotropic", BSDFFlags::EAnisotropic, D(BSDFFlags, Anisotropic))
        .value("SpatiallyVarying", BSDFFlags::ESpatiallyVarying, D(BSDFFlags, SpatiallyVarying))
        .value("NonSymmetric", BSDFFlags::ENonSymmetric, D(BSDFFlags, NonSymmetric))
        .value("FrontSide", BSDFFlags::EFrontSide, D(BSDFFlags, FrontSide))
        .value("BackSide", BSDFFlags::EBackSide, D(BSDFFlags, BackSide))
        .value("Reflection", BSDFFlags::EReflection, D(BSDFFlags, Reflection))
        .value("Transmission", BSDFFlags::ETransmission, D(BSDFFlags, Transmission))
        .value("Diffuse", BSDFFlags::EDiffuse, D(BSDFFlags, Diffuse))
        .value("Glossy", BSDFFlags::EGlossy, D(BSDFFlags, Glossy))
        .value("Smooth", BSDFFlags::ESmooth, D(BSDFFlags, Smooth))
        .value("Delta", BSDFFlags::EDelta, D(BSDFFlags, Delta))
        .value("Delta1D", BSDFFlags::EDelta1D, D(BSDFFlags, Delta1D))
        .value("All", BSDFFlags::EAll, D(BSDFFlags, All))
        .export_values();
}
