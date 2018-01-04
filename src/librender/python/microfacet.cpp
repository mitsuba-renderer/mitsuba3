#include <mitsuba/python/python.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/microfacet.h>

MTS_PY_EXPORT(MicrofacetDistribution) {
    using MicrofacetDistributionf = MicrofacetDistribution<Float>;
    auto md = py::class_<MicrofacetDistribution<Float>>(m, "MicrofacetDistributionf")
        .def(py::init<EType, const Float &, bool>(),
            "type"_a, "alpha"_a, "sample_visible"_a = true)
        .def(py::init<EType, const Float &, const Float &, bool>(),
             "type"_a, "alpha_u"_a, "alpha_v"_a, "sample_visible"_a = true)
        .def(py::init<const Properties &>())
        .def("type",    &MicrofacetDistributionf::type,    D(MicrofacetDistribution, type))
        .def("alpha",   &MicrofacetDistributionf::alpha,   D(MicrofacetDistribution, alpha))
        .def("alpha_u", &MicrofacetDistributionf::alpha_u, D(MicrofacetDistribution, alpha_u))
        .def("alpha_v", &MicrofacetDistributionf::alpha_v, D(MicrofacetDistribution, alpha_v))
        .def("exponent", &MicrofacetDistributionf::exponent,
             D(MicrofacetDistribution, exponent))
        .def("exponent_u", &MicrofacetDistributionf::exponent_u,
             D(MicrofacetDistribution, exponent_u))
        .def("exponent_v", &MicrofacetDistributionf::exponent_v,
             D(MicrofacetDistribution, exponent_v))
        .def("sample_visible", &MicrofacetDistributionf::sample_visible,
             D(MicrofacetDistribution, sample_visible))
        .def("is_anisotropic", &MicrofacetDistributionf::is_anisotropic,
             D(MicrofacetDistribution, is_anisotropic))
        .def("is_isotropic", &MicrofacetDistributionf::is_isotropic,
             D(MicrofacetDistribution, is_isotropic))
        .def("scale_alpha", &MicrofacetDistributionf::scale_alpha,
             D(MicrofacetDistribution, scale_alpha), "value"_a)
        .def("eval", &MicrofacetDistributionf::eval,
             D(MicrofacetDistribution, eval), "m"_a, "active"_a = true)
        .def("sample", &MicrofacetDistributionf::sample,
             D(MicrofacetDistribution, sample), "wi"_a, "sample"_a, "active"_a = true)
        .def("pdf", &MicrofacetDistributionf::pdf,
             D(MicrofacetDistribution, pdf), "wi"_a, "m"_a, "active"_a = true)
        .def("sample_all", &MicrofacetDistributionf::sample_all,
             D(MicrofacetDistribution, sample_all), "sample"_a, "active"_a = true)
        .def("pdf_all", &MicrofacetDistributionf::pdf_all,
             D(MicrofacetDistribution, pdf_all), "m"_a, "active"_a = true)
        .def("sample_visible_normals", &MicrofacetDistributionf::sample_visible_normals,
             D(MicrofacetDistribution, sample_visible_normals), "wi"_a, "sample"_a, "active"_a = true)
        .def("pdf_visible_normals", &MicrofacetDistributionf::pdf_visible_normals,
             D(MicrofacetDistribution, pdf_visible_normals), "wi"_a, "m"_a, "active"_a = true)
        .def("smith_g1", &MicrofacetDistributionf::smith_g1,
             D(MicrofacetDistribution, smith_g1), "v"_a, "m"_a, "active"_a = true)
        .def("G", &MicrofacetDistributionf::G, D(MicrofacetDistribution, G), "wi"_a, "wo"_a, "m"_a, "active"_a = true);

    using MicrofacetDistributionX = MicrofacetDistribution<FloatP>;
    auto mdx = py::class_<MicrofacetDistribution<FloatP>>(m, "MicrofacetDistributionX")
        .def(py::init([](EType type, Float alpha, bool sample_visible) {
            return new MicrofacetDistributionX(type, alpha, sample_visible);
        }))
        .def(py::init([](EType type, Float alpha_u, Float alpha_v, bool sample_visible) {
            return new MicrofacetDistributionX(type, alpha_u, alpha_v, sample_visible);
        }))
        .def("type",    &MicrofacetDistributionX::type,    D(MicrofacetDistribution, type))
        .def("alpha",   &MicrofacetDistributionX::alpha,   D(MicrofacetDistribution, alpha))
        .def("alpha_u", &MicrofacetDistributionX::alpha_u, D(MicrofacetDistribution, alpha_u))
        .def("alpha_v", &MicrofacetDistributionX::alpha_v, D(MicrofacetDistribution, alpha_v))
        .def("exponent", &MicrofacetDistributionX::exponent,
             D(MicrofacetDistribution, exponent))
        .def("exponent_u", &MicrofacetDistributionX::exponent_u,
             D(MicrofacetDistribution, exponent_u))
        .def("exponent_v", &MicrofacetDistributionX::exponent_v,
             D(MicrofacetDistribution, exponent_v))
        .def("sample_visible", &MicrofacetDistributionX::sample_visible,
             D(MicrofacetDistribution, sample_visible))
        .def("is_anisotropic", &MicrofacetDistributionX::is_anisotropic,
             D(MicrofacetDistribution, is_anisotropic))
        .def("is_isotropic", &MicrofacetDistributionX::is_isotropic,
             D(MicrofacetDistribution, is_isotropic))
        .def("scale_alpha", &MicrofacetDistributionX::scale_alpha,
             D(MicrofacetDistribution, scale_alpha), "value"_a)
        .def("eval", enoki::vectorize_wrapper(&MicrofacetDistributionX::eval),
             D(MicrofacetDistribution, eval), "m"_a, "active"_a = true)
        .def("sample", enoki::vectorize_wrapper(&MicrofacetDistributionX::sample),
             D(MicrofacetDistribution, sample), "wi"_a, "sample"_a, "active"_a = true)
        .def("pdf", enoki::vectorize_wrapper(&MicrofacetDistributionX::pdf),
             D(MicrofacetDistribution, pdf), "wi"_a, "m"_a, "active"_a = true)
        .def("sample_all", enoki::vectorize_wrapper(&MicrofacetDistributionX::sample_all),
             D(MicrofacetDistribution, sample_all), "sample"_a, "active"_a = true)
        .def("pdf_all", enoki::vectorize_wrapper(&MicrofacetDistributionX::pdf_all),
             D(MicrofacetDistribution, pdf_all), "m"_a, "active"_a = true)
        .def("sample_visible_normals", enoki::vectorize_wrapper(
                &MicrofacetDistributionX::sample_visible_normals
             ), D(MicrofacetDistribution, sample_visible_normals),
             "wi"_a, "sample"_a, "active"_a = true)
        .def("pdf_visible_normals", enoki::vectorize_wrapper(
                &MicrofacetDistributionX::pdf_visible_normals
             ), D(MicrofacetDistribution, pdf_visible_normals),
             "wi"_a, "m"_a, "active"_a = true)
        .def("smith_g1", enoki::vectorize_wrapper(&MicrofacetDistributionX::smith_g1),
             D(MicrofacetDistribution, smith_g1), "v"_a, "m"_a, "active"_a = true)
        .def("G", enoki::vectorize_wrapper(&MicrofacetDistributionX::G),
             D(MicrofacetDistribution, G), "wi"_a, "wo"_a, "m"_a, "active"_a = true);

    py::enum_<EType>(md, "EType", py::arithmetic(), "Supported distribution types")
        .value("EBeckmann", EType::EBeckmann, D(EType, EBeckmann))
        .value("EGGX",      EType::EGGX, D(EType, EGGX))
        .value("EPhong",    EType::EPhong, D(EType, EPhong))
        .export_values();
}
