#include <mitsuba/python/python.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/microfacet.h>

MTS_PY_EXPORT_VARIANTS(MicrofacetDistribution) {
    MTS_IMPORT_TYPES()
    MTS_IMPORT_OBJECT_TYPES()
    using ScalarFloat = typename MicrofacetDistribution::ScalarFloat;
    using FloatP    = Packet<ScalarFloat>;
    using Vector3fX = Vector<DynamicArray<FloatP>, 3>;
    MTS_PY_CHECK_ALIAS(MicrofacetDistribution)

    py::class_<MicrofacetDistribution>(m, "MicrofacetDistribution", D(MicrofacetDistribution))
        .def(py::init<MicrofacetType, const Float &, bool>(),
            "type"_a, "alpha"_a, "sample_visible"_a = true)
        .def(py::init<MicrofacetType, const Float &, const Float &, bool>(),
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
        .def("eval", vectorize<Float>(&MicrofacetDistribution::eval),
            "m"_a, D(MicrofacetDistribution, eval))
        .def("pdf", vectorize<Float>(&MicrofacetDistribution::pdf),
            "wi"_a, "m"_a, D(MicrofacetDistribution, pdf))
        .def("smith_g1", vectorize<Float>(&MicrofacetDistribution::smith_g1),
            "v"_a, "m"_a, D(MicrofacetDistribution, smith_g1))
        .def("sample", vectorize<Float>(&MicrofacetDistribution::sample),
            "wi"_a, "sample"_a, D(MicrofacetDistribution, sample))
        .def("G", vectorize<Float>(&MicrofacetDistribution::G),
            "wi"_a, "wo"_a, "m"_a, D(MicrofacetDistribution, G))
        .def("sample_visible_11",
            vectorize<Float>(&MicrofacetDistribution::sample_visible_11),
            "cos_theta_i"_a, "sample"_a, D(MicrofacetDistribution, sample_visible_11))
        .def("eval_reflectance",
            [](const MicrofacetDistribution &d, const Vector3fX &wi_, float eta) {
                mitsuba::MicrofacetDistribution<FloatP> d2(d.type(), d.alpha_u(), d.alpha_v());
                return eval_reflectance(d2, wi_, eta);
            }, "wi"_a, "eta"_a)
        .def_repr(MicrofacetDistribution)
        ;

    m.attr("MicrofacetType") = py::module::import("mitsuba.render.MicrofacetType");
}

MTS_PY_EXPORT(MicrofacetType) {
    py::enum_<MicrofacetType>(m, "MicrofacetType", D(MicrofacetType), py::arithmetic())
        .value("Beckmann", MicrofacetType::Beckmann, D(MicrofacetType, Beckmann))
        .value("GGX",      MicrofacetType::GGX, D(MicrofacetType, GGX))
        .export_values();
}
