#include <mitsuba/python/python.h>
#include <mitsuba/render/bsdf.h>

MTS_PY_EXPORT_STRUCT(BSDFContext) {
    MTS_IMPORT_TYPES()

    MTS_PY_CHECK_ALIAS(TransportMode, m) {
        py::enum_<TransportMode>(m, "TransportMode", D(TransportMode))
            .value("Radiance", TransportMode::Radiance, D(TransportMode, Radiance))
            .value("Importance", TransportMode::Importance, D(TransportMode, Importance))
            .export_values();
    }

    MTS_PY_CHECK_ALIAS(BSDFFlags, m) {
        py::enum_<BSDFFlags>(m, "BSDFFlags", D(BSDFFlags), py::arithmetic())
            .value("None", BSDFFlags::None, D(BSDFFlags, None))
            .value("Null", BSDFFlags::Null, D(BSDFFlags, Null))
            .value("DiffuseReflection", BSDFFlags::DiffuseReflection, D(BSDFFlags, DiffuseReflection))
            .value("DiffuseTransmission", BSDFFlags::DiffuseTransmission, D(BSDFFlags, DiffuseTransmission))
            .value("GlossyReflection", BSDFFlags::GlossyReflection, D(BSDFFlags, GlossyReflection))
            .value("GlossyTransmission", BSDFFlags::GlossyTransmission, D(BSDFFlags, GlossyTransmission))
            .value("DeltaReflection", BSDFFlags::DeltaReflection, D(BSDFFlags, DeltaReflection))
            .value("DeltaTransmission", BSDFFlags::DeltaTransmission, D(BSDFFlags, DeltaTransmission))
            .value("Anisotropic", BSDFFlags::Anisotropic, D(BSDFFlags, Anisotropic))
            .value("SpatiallyVarying", BSDFFlags::SpatiallyVarying, D(BSDFFlags, SpatiallyVarying))
            .value("NonSymmetric", BSDFFlags::NonSymmetric, D(BSDFFlags, NonSymmetric))
            .value("FrontSide", BSDFFlags::FrontSide, D(BSDFFlags, FrontSide))
            .value("BackSide", BSDFFlags::BackSide, D(BSDFFlags, BackSide))
            .value("Reflection", BSDFFlags::Reflection, D(BSDFFlags, Reflection))
            .value("Transmission", BSDFFlags::Transmission, D(BSDFFlags, Transmission))
            .value("Diffuse", BSDFFlags::Diffuse, D(BSDFFlags, Diffuse))
            .value("Glossy", BSDFFlags::Glossy, D(BSDFFlags, Glossy))
            .value("Smooth", BSDFFlags::Smooth, D(BSDFFlags, Smooth))
            .value("Delta", BSDFFlags::Delta, D(BSDFFlags, Delta))
            .value("Delta1D", BSDFFlags::Delta1D, D(BSDFFlags, Delta1D))
            .value("All", BSDFFlags::All, D(BSDFFlags, All))
            .export_values();
    }

    MTS_PY_CHECK_ALIAS(BSDFContext, m) {
        py::class_<BSDFContext>(m, "BSDFContext", D(BSDFContext))
            .def(py::init<TransportMode>(),
                "mode"_a = TransportMode::Radiance, D(BSDFContext, BSDFContext))
            .def(py::init<TransportMode, uint32_t, uint32_t>(),
                "mode"_a, "type_mak"_a, "component"_a, D(BSDFContext, BSDFContext, 2))
            .def_method(BSDFContext, reverse)
            .def_method(BSDFContext, is_enabled, "type"_a, "component"_a = 0)
            .def_field(BSDFContext, mode)
            .def_field(BSDFContext, type_mask)
            .def_field(BSDFContext, component)
            .def_repr(BSDFContext);
    }

    MTS_PY_CHECK_ALIAS(BSDFSample3f, m) {
        py::class_<BSDFSample3f>(m, "BSDFSample3ff", D(BSDFSample3f))
            .def(py::init<>(), D(BSDFSample3f, BSDFSample3f))
            .def(py::init<const Vector3f &>(), "wo"_a, D(BSDFSample3f, BSDFSample3f, 2))
            .def(py::init<const BSDFSample3f &>(), "bs"_a, "Copy constructor")
            .def_field(BSDFSample3f, wo)
            .def_field(BSDFSample3f, pdf)
            .def_field(BSDFSample3f, eta)
            .def_field(BSDFSample3f, sampled_type)
            .def_field(BSDFSample3f, sampled_component)
            .def_repr(BSDFSample3f);
    }
}

MTS_PY_EXPORT(BSDF) {
    MTS_IMPORT_TYPES()
    MTS_PY_CHECK_ALIAS(BSDF, m) {
        MTS_PY_CLASS(BSDF, Object)
            .def("sample", vectorize<Float>(&BSDF::sample),
                "ctx"_a, "si"_a, "sample1"_a, "sample2"_a, "active"_a = true, D(BSDF, sample))
                .def("eval", vectorize<Float>(&BSDF::eval),
                    "ctx"_a, "si"_a, "wo"_a, "active"_a = true, D(BSDF, eval))
                .def("pdf", vectorize<Float>(&BSDF::pdf),
                    "ctx"_a, "si"_a, "wo"_a, "active"_a = true, D(BSDF, pdf))
                .def("eval_tr", vectorize<Float>(&BSDF::eval_tr),
                    "si"_a, "active"_a = true, D(BSDF, eval_tr))
            .def("flags", py::overload_cast<Mask>(&BSDF::flags, py::const_),
                D(BSDF, flags))
            .def("flags", py::overload_cast<size_t, Mask>(&BSDF::flags, py::const_),
                D(BSDF, flags, 2))
            .def_method(BSDF, needs_differentials)
            .def_method(BSDF, component_count)
            .def_method(BSDF, id)
            .def("__repr__", &BSDF::to_string);
    }
}