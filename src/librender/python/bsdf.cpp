#include <mitsuba/python/python.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/shape.h>

MTS_PY_EXPORT_STRUCT(BSDFSample) {
    MTS_IMPORT_TYPES()

    MTS_PY_CHECK_ALIAS(TransportMode, m) {
        py::enum_<TransportMode>(m, "TransportMode", D(TransportMode))
            .value("Radiance", TransportMode::Radiance, D(TransportMode, Radiance))
            .value("Importance", TransportMode::Importance, D(TransportMode, Importance));
    }

    MTS_PY_CHECK_ALIAS(BSDFFlags, m) {
        py::enum_<BSDFFlags>(m, "BSDFFlags", D(BSDFFlags))
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
            .def(py::self == py::self)
            .def(py::self | py::self)
            .def(uint() | py::self)
            .def(py::self & py::self)
            .def(uint() & py::self)
            .def(+py::self)
            .def(~py::self)
            .def("__pos__", [](const BSDFFlags &f) {
                return static_cast<uint32_t>(f);
            }, py::is_operator());
    }

    m.def("has_flag", [](UInt32 flags, BSDFFlags f) { return has_flag(flags, f); });

    MTS_PY_CHECK_ALIAS(BSDFContext, m) {
        py::class_<BSDFContext>(m, "BSDFContext", D(BSDFContext))
            .def(py::init<TransportMode>(),
                "mode"_a = TransportMode::Radiance, D(BSDFContext, BSDFContext))
            .def(py::init<TransportMode, uint32_t, uint32_t>(),
                "mode"_a, "type_mak"_a, "component"_a, D(BSDFContext, BSDFContext, 2))
            .def_method(BSDFContext, reverse)
            .def_method(BSDFContext, is_enabled, "type"_a, "component"_a = 0)
            .def_field(BSDFContext, mode,      D(BSDFContext, mode))
            .def_field(BSDFContext, type_mask, D(BSDFContext, type_mask))
            .def_field(BSDFContext, component, D(BSDFContext, component))
            .def_repr(BSDFContext);
    }

    MTS_PY_CHECK_ALIAS(BSDFSample3f, m) {
        py::class_<BSDFSample3f>(m, "BSDFSample3f", D(BSDFSample3))
            .def(py::init<>(), D(BSDFSample3, BSDFSample3))
            .def(py::init<const Vector3f &>(), "wo"_a, D(BSDFSample3, BSDFSample3, 2))
            .def(py::init<const BSDFSample3f &>(), "bs"_a, "Copy constructor")
            .def_readwrite("wo", &BSDFSample3f::wo, D(BSDFSample3, wo))
            .def_readwrite("pdf", &BSDFSample3f::pdf, D(BSDFSample3, pdf))
            .def_readwrite("eta", &BSDFSample3f::eta, D(BSDFSample3, eta))
            .def_readwrite("sampled_type", &BSDFSample3f::sampled_type, D(BSDFSample3, sampled_type))
            .def_readwrite("sampled_component", &BSDFSample3f::sampled_component, D(BSDFSample3, sampled_component))
            .def_repr(BSDFSample3f);
    }
}

MTS_PY_EXPORT(BSDF) {
    MTS_IMPORT_TYPES(BSDF, BSDFPtr)

    MTS_PY_CHECK_ALIAS(BSDF, m) {
        auto bsdf = MTS_PY_CLASS(BSDF, Object)
            .def("sample", vectorize<Float>(&BSDF::sample),
                "ctx"_a, "si"_a, "sample1"_a, "sample2"_a, "active"_a = true, D(BSDF, sample))
            .def("eval", vectorize<Float>(&BSDF::eval),
                "ctx"_a, "si"_a, "wo"_a, "active"_a = true, D(BSDF, eval))
            .def("pdf", vectorize<Float>(&BSDF::pdf),
                "ctx"_a, "si"_a, "wo"_a, "active"_a = true, D(BSDF, pdf))
            .def("eval_tr", vectorize<Float>(&BSDF::eval_tr),
                "si"_a, "active"_a = true, D(BSDF, eval_tr))
            .def("flags", py::overload_cast<Mask>(&BSDF::flags, py::const_),
                "active"_a = true, D(BSDF, flags))
            .def("flags", py::overload_cast<size_t, Mask>(&BSDF::flags, py::const_),
                "index"_a, "active"_a = true, D(BSDF, flags, 2))
            .def_method(BSDF, needs_differentials, "active"_a = true)
            .def_method(BSDF, component_count, "active"_a = true)
            .def_method(BSDF, id)
            .def("__repr__", &BSDF::to_string);

        if constexpr (is_cuda_array_v<Float>) {
            pybind11_type_alias<UInt64, BSDFPtr>();
            pybind11_type_alias<UInt32, replace_scalar_t<Float, BSDFFlags>>();
        }

        if constexpr (is_array_v<Float>) {
            bsdf.def_static(
                "sample_vec",
                vectorize<Float>([](const BSDFPtr &ptr, const BSDFContext &ctx,
                                    const SurfaceInteraction3f &si, Float s1, const Point2f &s2,
                                    Mask active) { return ptr->sample(ctx, si, s1, s2, active); }),
                "ptr"_a, "ctx"_a, "si"_a, "sample1"_a, "sample2"_a, "active"_a = true,
                D(BSDF, sample));
            bsdf.def_static(
                "eval_vec",
                vectorize<Float>([](const BSDFPtr &ptr, const BSDFContext &ctx,
                                    const SurfaceInteraction3f &si, const Vector3f &wo,
                                    Mask active) { return ptr->eval(ctx, si, wo, active); }),
                "ptr"_a, "ctx"_a, "si"_a, "wo"_a, "active"_a = true,
                D(BSDF, eval));
            bsdf.def_static(
                "pdf_vec",
                vectorize<Float>([](const BSDFPtr &ptr, const BSDFContext &ctx,
                                    const SurfaceInteraction3f &si, const Vector3f &wo,
                                    Mask active) { return ptr->pdf(ctx, si, wo, active); }),
                "ptr"_a, "ctx"_a, "si"_a, "wo"_a, "active"_a = true,
                D(BSDF, pdf));
            bsdf.def_static(
                "flags_vec",
                vectorize<Float>([](const BSDFPtr &ptr, Mask active) {
                    return ptr->flags(active); }),
                "ptr"_a, "active"_a = true,
                D(BSDF, flags));
        }
    }
}
