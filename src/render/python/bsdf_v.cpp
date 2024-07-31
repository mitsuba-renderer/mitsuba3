#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/python/python.h>
#include <nanobind/trampoline.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/tuple.h>
#include <drjit/python.h>

MI_PY_EXPORT(BSDFSample) {
    MI_PY_IMPORT_TYPES()

    m.def("has_flag", [](uint32_t flags, BSDFFlags f) { return has_flag(flags, f); });
    m.def("has_flag", [](UInt32   flags, BSDFFlags f) { return has_flag(flags, f); });

    auto bs = nb::class_<BSDFSample3f>(m, "BSDFSample3f", D(BSDFSample3))
        .def(nb::init<>())
        .def(nb::init<const Vector3f &>(), "wo"_a, D(BSDFSample3, BSDFSample3))
        .def(nb::init<const BSDFSample3f &>(), "bs"_a, "Copy constructor")
        .def_rw("wo", &BSDFSample3f::wo, D(BSDFSample3, wo))
        .def_rw("pdf", &BSDFSample3f::pdf, D(BSDFSample3, pdf))
        .def_rw("eta", &BSDFSample3f::eta, D(BSDFSample3, eta))
        .def_rw("sampled_type", &BSDFSample3f::sampled_type, D(BSDFSample3, sampled_type))
        .def_rw("sampled_component", &BSDFSample3f::sampled_component, D(BSDFSample3, sampled_component))
        .def_repr(BSDFSample3f);

    MI_PY_DRJIT_STRUCT(bs, BSDFSample3f, wo, pdf, eta, sampled_type, sampled_component);
}

/// Trampoline for derived types implemented in Python
MI_VARIANT class PyBSDF : public BSDF<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(BSDF)
    NB_TRAMPOLINE(BSDF, 12);

    PyBSDF(const Properties &props) : BSDF(props) { }

    std::pair<BSDFSample3f, Spectrum>
    sample(const BSDFContext &ctx, const SurfaceInteraction3f &si,
           Float sample1, const Point2f &sample2,
           Mask active) const override {
        NB_OVERRIDE_PURE(sample, ctx, si, sample1, sample2, active);
    }

    Spectrum eval(const BSDFContext &ctx,
                  const SurfaceInteraction3f &si,
                  const Vector3f &wo,
                  Mask active) const override {
        NB_OVERRIDE_PURE(eval, ctx, si, wo, active);
    }

    Float pdf(const BSDFContext &ctx,
              const SurfaceInteraction3f &si,
              const Vector3f &wo,
              Mask active) const override {
        NB_OVERRIDE_PURE(pdf, ctx, si, wo, active);
    }

    std::pair<Spectrum, Float> eval_pdf(const BSDFContext &ctx,
              const SurfaceInteraction3f &si,
              const Vector3f &wo,
              Mask active) const override {
        NB_OVERRIDE(eval_pdf, ctx, si, wo, active);
    }

    Spectrum eval_diffuse_reflectance(const SurfaceInteraction3f &si,
                                      Mask active) const override {
        NB_OVERRIDE(eval_diffuse_reflectance, si, active);
    }

    Spectrum eval_null_transmission(const SurfaceInteraction3f &si,
                                      Mask active) const override {
        NB_OVERRIDE(eval_null_transmission, si, active);
    }

    Mask has_attribute(const std::string &name, Mask active) const override {
        NB_OVERRIDE(has_attribute, name, active);
    }

    UnpolarizedSpectrum eval_attribute(const std::string &name, const SurfaceInteraction3f &si, Mask active) const override {
        NB_OVERRIDE(eval_attribute, name, si, active);
    }

    Float eval_attribute_1(const std::string &name, const SurfaceInteraction3f &si, Mask active) const override {
        NB_OVERRIDE(eval_attribute_1, name, si, active);
    }

    Color3f eval_attribute_3(const std::string &name, const SurfaceInteraction3f &si, Mask active) const override {
        NB_OVERRIDE(eval_attribute_3, name, si, active);
    }

    std::string to_string() const override {
        NB_OVERRIDE_PURE(to_string);
    }

    void traverse(TraversalCallback *cb) override {
        NB_OVERRIDE(traverse, cb);
    }

    void parameters_changed(const std::vector<std::string> &keys) override {
        NB_OVERRIDE(parameters_changed, keys);
    }

    using BSDF::m_flags;
    using BSDF::m_components;
};

template <typename Ptr, typename Cls> void bind_bsdf_generic(Cls &cls) {
    MI_PY_IMPORT_TYPES()

    cls.def("sample",
            [](Ptr bsdf, const BSDFContext &ctx, const SurfaceInteraction3f &si,
               Float sample1, const Point2f &sample2, Mask active) {
                return bsdf->sample(ctx, si, sample1, sample2, active);
            }, "ctx"_a, "si"_a, "sample1"_a, "sample2"_a,
            "active"_a = true, D(BSDF, sample))
        .def("eval",
             [](Ptr bsdf, const BSDFContext &ctx, const SurfaceInteraction3f &si,
                const Vector3f &wo,
                Mask active) { return bsdf->eval(ctx, si, wo, active);
             }, "ctx"_a, "si"_a, "wo"_a, "active"_a = true, D(BSDF, eval))
        .def("pdf",
             [](Ptr bsdf, const BSDFContext &ctx, const SurfaceInteraction3f &si,
                const Vector3f &wo,
                Mask active) { return bsdf->pdf(ctx, si, wo, active);
             }, "ctx"_a, "si"_a, "wo"_a, "active"_a = true, D(BSDF, pdf))
        .def("eval_pdf",
             [](Ptr bsdf, const BSDFContext &ctx, const SurfaceInteraction3f &si,
                const Vector3f &wo,
                Mask active) { return bsdf->eval_pdf(ctx, si, wo, active);
             }, "ctx"_a, "si"_a, "wo"_a, "active"_a = true, D(BSDF, eval_pdf))
        .def("eval_pdf_sample",
             [](Ptr bsdf, const BSDFContext &ctx, const SurfaceInteraction3f &si,
                const Vector3f &wo, Float sample1, const Point2f &sample2,
                Mask active) {
                    return bsdf->eval_pdf_sample(ctx, si, wo, sample1, sample2, active);
                }, "ctx"_a, "si"_a, "wo"_a, "sample1"_a, "sample2"_a, "active"_a = true,
                D(BSDF, eval_pdf))
        .def("eval_null_transmission",
             [](Ptr bsdf, const SurfaceInteraction3f &si, Mask active) {
                 return bsdf->eval_null_transmission(si, active);
             }, "si"_a, "active"_a = true, D(BSDF, eval_null_transmission))
        .def("eval_diffuse_reflectance",
             [](Ptr bsdf, const SurfaceInteraction3f &si, Mask active) {
                 return bsdf->eval_diffuse_reflectance(si, active);
             }, "si"_a, "active"_a = true, D(BSDF, eval_diffuse_reflectance))
             .def("has_attribute",
            [](Ptr bsdf, const std::string &name, const Mask &active) {
                return bsdf->has_attribute(name, active);
            },
            "name"_a, "active"_a = true, D(BSDF, has_attribute))
       .def("eval_attribute",
            [](Ptr bsdf, const std::string &name,
               const SurfaceInteraction3f &si, const Mask &active) {
                return bsdf->eval_attribute(name, si, active);
            },
            "name"_a, "si"_a, "active"_a = true, D(BSDF, eval_attribute))
       .def("eval_attribute_1",
            [](Ptr bsdf, const std::string &name,
               const SurfaceInteraction3f &si, const Mask &active) {
                return bsdf->eval_attribute_1(name, si, active);
            },
            "name"_a, "si"_a, "active"_a = true, D(BSDF, eval_attribute_1))
       .def("eval_attribute_3",
            [](Ptr bsdf, const std::string &name,
               const SurfaceInteraction3f &si, const Mask &active) {
                return bsdf->eval_attribute_3(name, si, active);
            },
            "name"_a, "si"_a, "active"_a = true, D(BSDF, eval_attribute_3))
        .def("flags", [](Ptr bsdf) { return bsdf->flags(); }, D(BSDF, flags))
        .def("needs_differentials",
             [](Ptr bsdf) { return bsdf->needs_differentials(); },
             D(BSDF, needs_differentials));
}

MI_PY_EXPORT(BSDF) {
    MI_PY_IMPORT_TYPES(BSDF, BSDFPtr)
    using PyBSDF = PyBSDF<Float, Spectrum>;
    using Properties = PropertiesV<Float>;

    auto bsdf = MI_PY_TRAMPOLINE_CLASS(PyBSDF, BSDF, Object)
        .def(nb::init<const Properties&>(), "props"_a)
        .def("flags", nb::overload_cast<size_t, Mask>(&BSDF::flags, nb::const_),
            "index"_a, "active"_a = true, D(BSDF, flags, 2))
        .def_method(BSDF, component_count, "active"_a = true)
        .def_method(BSDF, id)
        .def_field(PyBSDF, m_flags, D(BSDF, m_flags))
        .def_field(PyBSDF, m_components, D(BSDF, m_components))
        .def("__repr__", &BSDF::to_string, D(BSDF, to_string));

    bind_bsdf_generic<BSDF *>(bsdf);

    if constexpr (dr::is_array_v<BSDFPtr>) {
        dr::ArrayBinding b;
        auto bsdf_ptr = dr::bind_array_t<BSDFPtr>(b, m, "BSDFPtr");
        bind_bsdf_generic<BSDFPtr>(bsdf_ptr);
    }

    MI_PY_REGISTER_OBJECT("register_bsdf", BSDF)
}
