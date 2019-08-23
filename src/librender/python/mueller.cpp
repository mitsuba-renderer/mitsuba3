#include <mitsuba/python/python.h>
#include <mitsuba/render/mueller.h>

MTS_PY_EXPORT(mueller) {
    MTS_PY_IMPORT_MODULE(mueller, "mitsuba.render.mueller");

    mueller.def("depolarizer", &mueller::depolarizer<Float>,
                D(mueller, depolarizer), "value"_a = 1.f);

    mueller.def("absorber", &mueller::absorber<Float>,
                D(mueller, absorber), "value"_a);

    mueller.def("linear_polarizer", &mueller::linear_polarizer<Float>,
                D(mueller, linear_polarizer), "value"_a = 1.f);

    mueller.def("linear_retarder", &mueller::linear_retarder<Float>,
                D(mueller, linear_retarder), "phase"_a);

    mueller.def("diattenuator", &mueller::diattenuator<Float>,
                D(mueller, diattenuator), "x"_a, "y"_a);

    mueller.def("rotator", &mueller::rotator<Float>, D(mueller, rotator),
                "theta"_a);

    mueller.def("rotated_element", &mueller::rotated_element<Float>,
                D(mueller, rotated_element), "theta"_a, "M"_a);

    mueller.def("reverse", &mueller::reverse<Float>,
                D(mueller, reverse), "M"_a);

    mueller.def("specular_reflection",
                &mueller::specular_reflection<Float, Complex<Float>>,
                D(mueller, specular_reflection), "cos_theta_i"_a, "eta"_a);

    mueller.def("specular_transmission", &mueller::specular_transmission<Float>,
                D(mueller, specular_transmission), "cos_theta_i"_a, "eta"_a);

    mueller.def("stokes_basis", &mueller::stokes_basis<Vector3f>,
                D(mueller, stokes_basis), "w"_a);

    mueller.def("rotate_stokes_basis", &mueller::rotate_stokes_basis<Vector3f>,
                D(mueller, rotate_stokes_basis), "wi"_a, "basis_current"_a, "basis_target"_a);

    mueller.def("rotate_mueller_basis",
                &mueller::rotate_mueller_basis<Vector3f>,
                D(mueller, rotate_mueller_basis), "M"_a,
                "in_forward"_a, "in_basis_current"_a, "in_basis_target"_a,
                "out_forward"_a, "out_basis_current"_a, "out_basis_target"_a);

    mueller.def("rotate_mueller_basis_collinear",
                &mueller::rotate_mueller_basis_collinear<Vector3f>,
                D(mueller, rotate_mueller_basis_collinear), "M"_a,
        "forward"_a, "basis_current"_a, "basis_target"_a);

    mueller.def("unit_angle", [](const Vector3f &a, const Vector3f &b) {
                return unit_angle(a, b);
    }, "a"_a, "b"_a);
}
