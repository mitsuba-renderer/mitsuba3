#include <mitsuba/python/python.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/microfacet.h>

MTS_PY_EXPORT_MODE_VARIANTS(MicrofacetDistribution) {
    MTS_IMPORT_TYPES()
    MTS_IMPORT_OBJECT_TYPES()
    using ScalarFloat = typename MicrofacetDistribution::ScalarFloat;
    using FloatP    = Packet<ScalarFloat>;
    using Vector3fX = Vector<DynamicArray<FloatP>, 3>;

    auto md = py::class_<MicrofacetDistribution>(m, "MicrofacetDistribution", D(MicrofacetDistribution))
        .def(py::init<MicrofacetType, const ScalarFloat &, bool>(),
            "type"_a, "alpha"_a, "sample_visible"_a = true)
        .def(py::init<MicrofacetType, const ScalarFloat &, const ScalarFloat &, bool>(),
             "type"_a, "alpha_u"_a, "alpha_v"_a, "sample_visible"_a = true)
        .def(py::init<const Properties &>())
        .def_method(MicrofacetDistribution, type)
        .def_method(MicrofacetDistribution, alpha)
        .def_method(MicrofacetDistribution, alpha_u)
        .def_method(MicrofacetDistribution, alpha_v)
        .def_method(MicrofacetDistribution, sample_visible)
        .def_method(MicrofacetDistribution, is_anisotropic)
        .def_method(MicrofacetDistribution, is_isotropic)
        .def_method(MicrofacetDistribution, scale_alpha, "value"_a)
        .def_method(MicrofacetDistribution, eval, "m"_a)
        .def_method(MicrofacetDistribution, pdf, "wi"_a, "m"_a)
        .def_method(MicrofacetDistribution, sample, "wi"_a, "sample"_a)
        .def_method(MicrofacetDistribution, G, "wi"_a, "wo"_a, "m"_a)
        .def_method(MicrofacetDistribution, smith_g1, "v"_a, "m"_a)
        .def_method(MicrofacetDistribution, sample_visible_11, "cos_theta_i"_a, "sample"_a)
        .def("eval_reflectance",
            [](const MicrofacetDistribution &d, const Vector3fX &wi_, float eta) {
                mitsuba::MicrofacetDistribution<FloatP> d2(d.type(), d.alpha_u(), d.alpha_v());
                return d2.eval_reflectance(wi_, eta);
            }, "wi"_a, "eta"_a)
        .def_repr(MicrofacetDistribution)
        ;

    // TODO vectorize wrapper bindings
    // if constexpr (is_array_v<Float> && !is_dynamic_v<Float>) {
    //     md.def("eval", enoki::vectorize_wrapper(&MicrofacetDistribution::eval),
    //            "m"_a, D(MicrofacetDistribution, eval))
    //     .def("pdf", enoki::vectorize_wrapper(&MicrofacetDistribution::pdf),
    //          "wi"_a, "m"_a, D(MicrofacetDistribution, pdf))
    //     .def("smith_g1", enoki::vectorize_wrapper(&MicrofacetDistribution::smith_g1),
    //          "v"_a, "m"_a, D(MicrofacetDistribution, smith_g1))
    //     .def("sample", enoki::vectorize_wrapper(&MicrofacetDistribution::sample),
    //          "wi"_a, "sample"_a, D(MicrofacetDistribution, sample))
    //     .def("G", enoki::vectorize_wrapper(&MicrofacetDistribution::G),
    //          "wi"_a, "wo"_a, "m"_a, D(MicrofacetDistribution, G))
    //     .def("sample_visible_11",
    //          enoki::vectorize_wrapper(&MicrofacetDistribution::sample_visible_11),
    //          "cos_theta_i"_a, "sample"_a, D(MicrofacetDistribution, sample_visible_11))
    //     ;
    // }
}

MTS_PY_EXPORT(MicrofacetType) {
    py::enum_<MicrofacetType>(m, "MicrofacetType", D(MicrofacetType), py::arithmetic())
        .value("Beckmann", MicrofacetType::Beckmann, D(MicrofacetType, Beckmann))
        .value("GGX",      MicrofacetType::GGX, D(MicrofacetType, GGX))
        .export_values();
}
