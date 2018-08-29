#include <mitsuba/python/python.h>
#include <mitsuba/render/mueller.h>

MTS_PY_EXPORT(mueller) {
    MTS_PY_IMPORT_MODULE(mueller, "mitsuba.render.mueller");

    mueller.def("depolarizer", &mueller::depolarizer<Float>,
                D(mueller, depolarizer), "value"_a = 1.f);

    mueller.def("linear_polarizer", &mueller::linear_polarizer<Float>,
                D(mueller, linear_polarizer), "value"_a = 1.f);

    mueller.def("diattenuator", &mueller::diattenuator<Float>,
                D(mueller, diattenuator), "x"_a, "y"_a);

    mueller.def("rotator", &mueller::rotator<Float>, D(mueller, rotator),
                "theta"_a);

    mueller.def("rotated_element", &mueller::rotated_element<Float>,
                D(mueller, rotated_element), "theta"_a, "M"_a);

    mueller.def("specular_reflection",
                &mueller::specular_reflection<Float, Complex<Float>>,
                D(mueller, specular_reflection), "cos_theta_i"_a, "eta"_a);

    mueller.def("specular_transmission", &mueller::specular_transmission<Float>,
                D(mueller, specular_transmission), "cos_theta_i"_a, "eta"_a);
}
