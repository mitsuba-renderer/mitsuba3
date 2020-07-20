#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(BSDFSample) {
    MTS_PY_IMPORT_TYPES_DYNAMIC()

    m.def("has_flag", [](UInt32 flags, BSDFFlags f) { return has_flag(flags, f); });

    py::class_<BSDFSample3f>(m, "BSDFSample3f", D(BSDFSample3))
        .def(py::init<>())
        .def(py::init<const Vector3f &>(), "wo"_a, D(BSDFSample3, BSDFSample3))
        .def(py::init<const BSDFSample3f &>(), "bs"_a, "Copy constructor")
        .def_readwrite("wo", &BSDFSample3f::wo, D(BSDFSample3, wo))
        .def_readwrite("pdf", &BSDFSample3f::pdf, D(BSDFSample3, pdf))
        .def_readwrite("eta", &BSDFSample3f::eta, D(BSDFSample3, eta))
        .def_readwrite("sampled_type", &BSDFSample3f::sampled_type, D(BSDFSample3, sampled_type))
        .def_readwrite("sampled_component", &BSDFSample3f::sampled_component, D(BSDFSample3, sampled_component))
        .def_repr(BSDFSample3f);
}

/// Trampoline for derived types implemented in Python
MTS_VARIANT class PyBSDF : public BSDF<Float, Spectrum> {
public:
    MTS_IMPORT_TYPES(BSDF)

    PyBSDF(const Properties &props) : BSDF(props) { }

    std::pair<BSDFSample3f, Spectrum>
    sample(const BSDFContext &ctx, const SurfaceInteraction3f &si,
           Float sample1, const Point2f &sample2,
           Mask active) const override {
        using Return = std::pair<BSDFSample3f, Spectrum>;
        PYBIND11_OVERLOAD_PURE(Return, BSDF, sample, ctx, si, sample1, sample2, active);
    }

    Spectrum eval(const BSDFContext &ctx,
                  const SurfaceInteraction3f &si,
                  const Vector3f &wo,
                  Mask active) const override {
        PYBIND11_OVERLOAD_PURE(Spectrum, BSDF, eval, ctx, si, wo, active);
    }

    Float pdf(const BSDFContext &ctx,
              const SurfaceInteraction3f &si,
              const Vector3f &wo,
              Mask active) const override {
        PYBIND11_OVERLOAD_PURE(Float, BSDF, pdf, ctx, si, wo, active);
    }

    std::string to_string() const override {
        PYBIND11_OVERLOAD_PURE(std::string, BSDF, to_string,);
    }

    using BSDF::m_flags;
    using BSDF::m_components;
};

MTS_PY_EXPORT(BSDF) {
    MTS_PY_IMPORT_TYPES(BSDF, BSDFPtr)
    using PyBSDF = PyBSDF<Float, Spectrum>;

    auto bsdf = py::class_<BSDF, PyBSDF, Object, ref<BSDF>>(m, "BSDF", D(BSDF))
        .def(py::init<const Properties&>(), "props"_a)
        .def("sample", vectorize(&BSDF::sample),
            "ctx"_a, "si"_a, "sample1"_a, "sample2"_a, "active"_a = true, D(BSDF, sample))
        .def("eval", vectorize(&BSDF::eval),
            "ctx"_a, "si"_a, "wo"_a, "active"_a = true, D(BSDF, eval))
        .def("pdf", vectorize(&BSDF::pdf),
            "ctx"_a, "si"_a, "wo"_a, "active"_a = true, D(BSDF, pdf))
        .def("eval_null_transmission", vectorize(&BSDF::eval_null_transmission),
            "si"_a, "active"_a = true, D(BSDF, eval_null_transmission))
        .def("flags", py::overload_cast<Mask>(&BSDF::flags, py::const_),
            "active"_a = true, D(BSDF, flags))
        .def("flags", py::overload_cast<size_t, Mask>(&BSDF::flags, py::const_),
            "index"_a, "active"_a = true, D(BSDF, flags, 2))
        .def_method(BSDF, needs_differentials, "active"_a = true)
        .def_method(BSDF, component_count, "active"_a = true)
        .def_method(BSDF, id)
        .def_readwrite("m_flags",      &PyBSDF::m_flags)
        .def_readwrite("m_components", &PyBSDF::m_components)
        .def("__repr__", &BSDF::to_string)
        ;

    if constexpr (is_cuda_array_v<Float>) {
        pybind11_type_alias<UInt64, BSDFPtr>();
        pybind11_type_alias<UInt32, replace_scalar_t<Float, BSDFFlags>>();
    }

    if constexpr (is_array_v<Float>) {
        bsdf.def_static(
            "sample_vec",
            vectorize([](const BSDFPtr &ptr, const BSDFContext &ctx,
                                const SurfaceInteraction3f &si, Float s1, const Point2f &s2,
                                Mask active) { return ptr->sample(ctx, si, s1, s2, active); }),
            "ptr"_a, "ctx"_a, "si"_a, "sample1"_a, "sample2"_a, "active"_a = true,
            D(BSDF, sample));
        bsdf.def_static(
            "eval_vec",
            vectorize([](const BSDFPtr &ptr, const BSDFContext &ctx,
                                const SurfaceInteraction3f &si, const Vector3f &wo,
                                Mask active) { return ptr->eval(ctx, si, wo, active); }),
            "ptr"_a, "ctx"_a, "si"_a, "wo"_a, "active"_a = true,
            D(BSDF, eval));
        bsdf.def_static(
            "pdf_vec",
            vectorize([](const BSDFPtr &ptr, const BSDFContext &ctx,
                                const SurfaceInteraction3f &si, const Vector3f &wo,
                                Mask active) { return ptr->pdf(ctx, si, wo, active); }),
            "ptr"_a, "ctx"_a, "si"_a, "wo"_a, "active"_a = true,
            D(BSDF, pdf));
        bsdf.def_static(
            "flags_vec",
            vectorize([](const BSDFPtr &ptr, Mask active) {
                return ptr->flags(active); }),
            "ptr"_a, "active"_a = true,
            D(BSDF, flags));
    }

    MTS_PY_REGISTER_OBJECT("register_bsdf", BSDF)
}
