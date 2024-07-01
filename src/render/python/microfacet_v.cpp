#include <mitsuba/core/properties.h>
#include <mitsuba/render/microfacet.h>
#include <mitsuba/python/python.h>
#include <drjit/dynamic.h>

#include <nanobind/stl/pair.h>
#include <nanobind/stl/list.h>
#include <nanobind/stl/string.h>
#include <nanobind/ndarray.h>

MI_PY_EXPORT(MicrofacetDistribution) {
    MI_PY_IMPORT_TYPES(MicrofacetDistribution)

    nb::class_<MicrofacetDistribution>(m, "MicrofacetDistribution", D(MicrofacetDistribution))
        .def("__init__", [](MicrofacetDistribution* alloc, MicrofacetType t, ScalarFloat alpha, bool sv) {
            new (alloc) MicrofacetDistribution(t, alpha, sv);
        }, "type"_a, "alpha"_a, "sample_visible"_a = true)
        .def("__init__", [](MicrofacetDistribution* alloc, MicrofacetType t, ScalarFloat alpha_u, ScalarFloat alpha_v, bool sv) {
            new (alloc) MicrofacetDistribution(t, alpha_u, alpha_v, sv);
        }, "type"_a, "alpha_u"_a, "alpha_v"_a, "sample_visible"_a = true)
        .def(nb::init<MicrofacetType, const Float &, bool>(), "type"_a, "alpha"_a,
            "sample_visible"_a = true)
        .def(nb::init<MicrofacetType, const Float &, const Float &, bool>(), "type"_a, "alpha_u"_a,
            "alpha_v"_a, "sample_visible"_a = true)
        .def(nb::init<const Properties &>())
        .def_method(MicrofacetDistribution, type)
        .def_method(MicrofacetDistribution, alpha)
        .def_method(MicrofacetDistribution, alpha_u)
        .def_method(MicrofacetDistribution, alpha_v)
        .def_method(MicrofacetDistribution, sample_visible)
        .def_method(MicrofacetDistribution, is_anisotropic)
        .def_method(MicrofacetDistribution, is_isotropic)
        .def_method(MicrofacetDistribution, scale_alpha, "value"_a)
        .def("eval", &MicrofacetDistribution::eval, "m"_a,
            D(MicrofacetDistribution, eval))
        .def("pdf", &MicrofacetDistribution::pdf, "wi"_a, "m"_a,
            D(MicrofacetDistribution, pdf))
        .def("smith_g1", &MicrofacetDistribution::smith_g1, "v"_a, "m"_a,
            D(MicrofacetDistribution, smith_g1))
        .def("sample", &MicrofacetDistribution::sample, "wi"_a, "sample"_a,
            D(MicrofacetDistribution, sample))
        .def("G", &MicrofacetDistribution::G, "wi"_a, "wo"_a, "m"_a,
            D(MicrofacetDistribution, G))
        .def("sample_visible_11", &MicrofacetDistribution::sample_visible_11,
            "cos_theta_i"_a, "sample"_a, D(MicrofacetDistribution, sample_visible_11))
        .def_repr(MicrofacetDistribution);

    m.def("eval_reflectance",
        [](MicrofacetType type, float alpha_u, float alpha_v,
           const Vector<DynamicBuffer<Float>, 3> &wi,
           float eta) {
            mitsuba::MicrofacetDistribution<dr::Packet<float>, Spectrum> d(type, alpha_u, alpha_v);
            return eval_reflectance(d, wi, eta);
        }, "type"_a, "alpha_u"_a, "alpha_v"_a, "wi"_a, "eta"_a);
}
