#include <mitsuba/python/python.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/microfacet.h>

MTS_PY_EXPORT_VARIANTS(MicrofacetDistribution) {
    using ScalarFloat = typename MicrofacetDistribution::ScalarFloat;
    using FloatP    = Packet<ScalarFloat>;
    using Vector3fX = Vector<DynamicArray<FloatP>, 3>;

    auto md = py::class_<MicrofacetDistribution>(m, "MicrofacetDistribution", D(MicrofacetDistribution))
        .def(py::init<MicrofacetType, const ScalarFloat &, bool>(),
            "type"_a, "alpha"_a, "sample_visible"_a = true)
        .def(py::init<MicrofacetType, const ScalarFloat &, const ScalarFloat &, bool>(),
             "type"_a, "alpha_u"_a, "alpha_v"_a, "sample_visible"_a = true)
        .def(py::init<const Properties &>())
        .mdef(MicrofacetDistribution, type)
        .mdef(MicrofacetDistribution, alpha)
        .mdef(MicrofacetDistribution, alpha_u)
        .mdef(MicrofacetDistribution, alpha_v)
        .mdef(MicrofacetDistribution, sample_visible)
        .mdef(MicrofacetDistribution, is_anisotropic)
        .mdef(MicrofacetDistribution, is_isotropic)
        .mdef(MicrofacetDistribution, scale_alpha, "value"_a)
        .mdef(MicrofacetDistribution, eval, "m"_a)
        .mdef(MicrofacetDistribution, pdf, "wi"_a, "m"_a)
        .mdef(MicrofacetDistribution, sample, "wi"_a, "sample"_a)
        .mdef(MicrofacetDistribution, G, "wi"_a, "wo"_a, "m"_a)
        .mdef(MicrofacetDistribution, smith_g1, "v"_a, "m"_a)
        .mdef(MicrofacetDistribution, sample_visible_11, "cos_theta_i"_a, "sample"_a)
        .def("eval_reflectance",
            [](const MicrofacetDistribution &d, const Vector3fX &wi_, float eta) {
                mitsuba::MicrofacetDistribution<FloatP> d2(d.type(), d.alpha_u(), d.alpha_v());
                return d2.eval_reflectance(wi_, eta);
            }, "wi"_a, "eta"_a)
        .def("__repr__",
            [](const MicrofacetDistribution &md) {
                std::ostringstream oss;
                oss << md;
                return oss.str();
            }
        )
        ;

    if constexpr (is_array_v<Float> && !is_dynamic_v<Float>) {
        md.def("eval", enoki::vectorize_wrapper(&MicrofacetDistribution::eval),
               "m"_a, D(MicrofacetDistribution, eval))
        .def("pdf", enoki::vectorize_wrapper(&MicrofacetDistribution::pdf),
             "wi"_a, "m"_a, D(MicrofacetDistribution, pdf))
        .def("smith_g1", enoki::vectorize_wrapper(&MicrofacetDistribution::smith_g1),
             "v"_a, "m"_a, D(MicrofacetDistribution, smith_g1))
        .def("sample", enoki::vectorize_wrapper(&MicrofacetDistribution::sample),
             "wi"_a, "sample"_a, D(MicrofacetDistribution, sample))
        .def("G", enoki::vectorize_wrapper(&MicrofacetDistribution::G),
             "wi"_a, "wo"_a, "m"_a, D(MicrofacetDistribution, G))
        .def("sample_visible_11",
             enoki::vectorize_wrapper(&MicrofacetDistribution::sample_visible_11),
             "cos_theta_i"_a, "sample"_a, D(MicrofacetDistribution, sample_visible_11))
        ;
    }
}

MTS_PY_EXPORT(MicrofacetType) {
    py::enum_<MicrofacetType>(m, "MicrofacetType", D(MicrofacetType), py::arithmetic())
        .value("Beckmann", MicrofacetType::Beckmann, D(MicrofacetType, Beckmann))
        .value("GGX",      MicrofacetType::GGX, D(MicrofacetType, GGX))
        .export_values();
}
