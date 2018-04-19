#include <mitsuba/python/python.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/microfacet.h>

MTS_PY_EXPORT(MicrofacetDistribution) {
    using MicrofacetDistribution = mitsuba::MicrofacetDistribution<Float>;
    using MicrofacetDistributionP = mitsuba::MicrofacetDistribution<FloatP>;

    auto md = py::class_<MicrofacetDistribution>(m, "MicrofacetDistribution")
        .def(py::init<EType, const Float &, bool>(),
            "type"_a, "alpha"_a, "sample_visible"_a = true)
        .def(py::init<EType, const Float &, const Float &, bool>(),
             "type"_a, "alpha_u"_a, "alpha_v"_a, "sample_visible"_a = true)
        .def(py::init<const Properties &>())
        .def("type",    &MicrofacetDistribution::type,    D(MicrofacetDistribution, type))
        .def("alpha",   &MicrofacetDistribution::alpha,   D(MicrofacetDistribution, alpha))
        .def("alpha_u", &MicrofacetDistribution::alpha_u, D(MicrofacetDistribution, alpha_u))
        .def("alpha_v", &MicrofacetDistribution::alpha_v, D(MicrofacetDistribution, alpha_v))
        .def("sample_visible", &MicrofacetDistribution::sample_visible,
             D(MicrofacetDistribution, sample_visible))
        .def("is_anisotropic", &MicrofacetDistribution::is_anisotropic,
             D(MicrofacetDistribution, is_anisotropic))
        .def("is_isotropic", &MicrofacetDistribution::is_isotropic,
             D(MicrofacetDistribution, is_isotropic))
        .def("scale_alpha", &MicrofacetDistribution::scale_alpha,
             D(MicrofacetDistribution, scale_alpha), "value"_a)
        .def("eval", &MicrofacetDistribution::eval,
             D(MicrofacetDistribution, eval), "m"_a)
        .def("eval", enoki::vectorize_wrapper(
            [](const MicrofacetDistribution &d, const Vector3fP &m) {
                return MicrofacetDistributionP(d.type(), d.alpha_u(), d.alpha_v(),
                                               d.sample_visible()).eval(m);
            }), "m"_a)
        .def("pdf", &MicrofacetDistribution::pdf,
             D(MicrofacetDistribution, pdf), "wi"_a, "m"_a)
        .def("pdf", enoki::vectorize_wrapper(
            [](const MicrofacetDistribution &d, const Vector3fP &wi, const Vector3fP &m) {
                return MicrofacetDistributionP(d.type(), d.alpha_u(), d.alpha_v(),
                                               d.sample_visible()).pdf(wi, m);
            }), "wi"_a, "m"_a)
        .def("sample", &MicrofacetDistribution::sample,
             D(MicrofacetDistribution, sample), "wi"_a, "sample"_a)
        .def("sample", enoki::vectorize_wrapper(
            [](const MicrofacetDistribution &d, const Vector3fP &wi,
               const Point2fP &sample) {
                return MicrofacetDistributionP(d.type(), d.alpha_u(), d.alpha_v(),
                                               d.sample_visible()).sample(wi, sample);
            }), "wi"_a, "sample"_a)
        .def("smith_g1", &MicrofacetDistribution::smith_g1,
             D(MicrofacetDistribution, smith_g1), "v"_a, "m"_a)
        .def("smith_g1", enoki::vectorize_wrapper(
            [](const MicrofacetDistribution &d, const Vector3fP &v, const Vector3fP &m) {
                MicrofacetDistributionP mdp(d.type(), d.alpha_u(), d.alpha_v(),
                                            d.sample_visible());
                return mdp.smith_g1(v, m);
            }), "v"_a, "m"_a)
        .def("G", &MicrofacetDistribution::G, D(MicrofacetDistribution, G),
             "wi"_a, "wo"_a, "m"_a)
        .def("G", enoki::vectorize_wrapper(
            [](const MicrofacetDistribution &d, const Vector3fP &wi,
               const Vector3fP &wo, const Vector3fP &m) {
                return MicrofacetDistributionP(d.type(), d.alpha_u(), d.alpha_v(),
                                               d.sample_visible()).G(wi, wo, m);
            }), "wi"_a, "wo"_a, "m"_a)
        .def("sample_visible_11", &MicrofacetDistribution::sample_visible_11,
             D(MicrofacetDistribution, sample_visible_11),
             "cos_theta_i"_a, "sample"_a)
        .def("sample_visible_11", enoki::vectorize_wrapper(
            [](const MicrofacetDistribution &d, FloatP tan_theta_i,
               const Point2fP &sample) {
                MicrofacetDistributionP mdp(d.type(), d.alpha_u(), d.alpha_v(),
                                            d.sample_visible());
                return mdp.sample_visible_11(tan_theta_i, sample);
            }), "cos_theta_i"_a, "sample"_a)
        .def("__repr__", [](const MicrofacetDistribution &md) {
                std::ostringstream oss;
                oss << md;
                return oss.str();
            }
        );

    py::enum_<EType>(md, "EType", py::arithmetic(), "Supported distribution types")
        .value("EBeckmann", EType::EBeckmann, D(EType, EBeckmann))
        .value("EGGX",      EType::EGGX, D(EType, EGGX))
        .export_values();
}
