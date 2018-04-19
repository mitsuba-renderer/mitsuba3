#include <mitsuba/render/reflection.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(reflection) {
    m.def("fresnel", &fresnel<float>, D(fresnel), "cos_theta_i"_a, "eta"_a);
    m.def("fresnel", vectorize_wrapper(&fresnel<FloatP>), "cos_theta_i"_a, "eta"_a);
    m.def("fresnel_polarized", &fresnel_polarized<float>, D(fresnel_polarized), "cos_theta_i"_a, "eta"_a);
    m.def("fresnel_polarized", vectorize_wrapper(&fresnel_polarized<FloatP>), "cos_theta_i"_a, "eta"_a);
    m.def("fresnel_complex", &fresnel_complex<float>, D(fresnel_complex), "cos_theta_i"_a, "eta"_a);
    m.def("fresnel_complex", vectorize_wrapper(&fresnel_complex<FloatP>), "cos_theta_i"_a, "eta"_a);
    m.def("fresnel_complex_polarized", &fresnel_complex_polarized<float>, D(fresnel_complex_polarized), "cos_theta_i"_a, "eta"_a);
    m.def("fresnel_complex_polarized", vectorize_wrapper(&fresnel_complex_polarized<FloatP>), "cos_theta_i"_a, "eta"_a);
}
