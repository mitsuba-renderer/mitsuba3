#include <mitsuba/core/properties.h>
#include <mitsuba/render/microfacet.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(MicrofacetDistribution) {
    MTS_PY_IMPORT_TYPES(MicrofacetDistribution)

    py::class_<MicrofacetDistribution>(m, "MicrofacetDistribution", D(MicrofacetDistribution))
        // TODO is this needed?
        .def(py::init([](MicrofacetType t, ScalarFloat alpha, bool sv) {
            return MicrofacetDistribution(t, alpha, sv);
        }), "type"_a, "alpha"_a, "sample_visible"_a = true)
        .def(py::init([](MicrofacetType t, ScalarFloat alpha_u, ScalarFloat alpha_v, bool sv) {
            return MicrofacetDistribution(t, alpha_u, alpha_v, sv);
        }), "type"_a, "alpha_u"_a, "alpha_v"_a, "sample_visible"_a = true)
        .def(py::init<MicrofacetType, const Float &, bool>(), "type"_a, "alpha"_a,
            "sample_visible"_a = true)
        .def(py::init<MicrofacetType, const Float &, const Float &, bool>(), "type"_a, "alpha_u"_a,
            "alpha_v"_a, "sample_visible"_a = true)
        .def(py::init<const Properties &>())
        .def_method(MicrofacetDistribution, type)
        .def_method(MicrofacetDistribution, alpha)
        .def_method(MicrofacetDistribution, alpha_u)
        .def_method(MicrofacetDistribution, alpha_v)
        .def_method(MicrofacetDistribution, sample_visible)
        .def_method(MicrofacetDistribution, is_anisotropic)
        .def_method(MicrofacetDistribution, is_isotropic)
        .def_method(MicrofacetDistribution, scale_alpha, "value"_a)
        .def("eval", vectorize(&MicrofacetDistribution::eval), "m"_a,
            D(MicrofacetDistribution, eval))
        .def("pdf", vectorize(&MicrofacetDistribution::pdf), "wi"_a, "m"_a,
            D(MicrofacetDistribution, pdf))
        .def("smith_g1", vectorize(&MicrofacetDistribution::smith_g1), "v"_a, "m"_a,
            D(MicrofacetDistribution, smith_g1))
        .def("sample", vectorize(&MicrofacetDistribution::sample), "wi"_a, "sample"_a,
            D(MicrofacetDistribution, sample))
        .def("G", vectorize(&MicrofacetDistribution::G), "wi"_a, "wo"_a, "m"_a,
            D(MicrofacetDistribution, G))
        .def("sample_visible_11", vectorize(&MicrofacetDistribution::sample_visible_11),
            "cos_theta_i"_a, "sample"_a, D(MicrofacetDistribution, sample_visible_11))
        .def_repr(MicrofacetDistribution);

    m.def("eval_reflectance",
        [](MicrofacetType type, float alpha_u, float alpha_v,
           const Vector<DynamicArray<Packet<float>>, 3> &wi_,
           float eta) {
            mitsuba::MicrofacetDistribution<Packet<float>, Spectrum> d(type, alpha_u, alpha_v);
            return eval_reflectance(d, wi_, eta);
        }, "type"_a, "alpha_u"_a, "alpha_v"_a, "wi"_a, "eta"_a);
}
