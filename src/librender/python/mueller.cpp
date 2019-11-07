#include <mitsuba/python/python.h>
#include <mitsuba/render/mueller.h>

MTS_PY_EXPORT_VARIANTS(mueller) {
    MTS_IMPORT_TYPES()

    MTS_PY_IMPORT_MODULE(mueller, "mitsuba.render.mueller");

    mueller.def("depolarizer", &mueller::depolarizer<Float>,
        "value"_a = 1.f, D(mueller, depolarizer))
    .def("absorber", &mueller::absorber<Float>,
        "value"_a, D(mueller, absorber))
    .def("linear_polarizer", &mueller::linear_polarizer<Float>,
        "value"_a = 1.f, D(mueller, linear_polarizer))
    .def("linear_retarder", &mueller::linear_retarder<Float>,
        "phase"_a, D(mueller, linear_retarder))
    .def("diattenuator", &mueller::diattenuator<Float>,
        "x"_a, "y"_a, D(mueller, diattenuator))
    .def("rotator", &mueller::rotator<Float>, "theta"_a, D(mueller, rotator))
    .def("rotated_element", &mueller::rotated_element<Float>,
        "theta"_a, "M"_a, D(mueller, rotated_element))
    .def("reverse", &mueller::reverse<Float>, "M"_a, D(mueller, reverse))
    .def("specular_reflection",
        &mueller::specular_reflection<Float, Complex<Float>>,
        "cos_theta_i"_a, "eta"_a, D(mueller, specular_reflection))
    .def("specular_transmission", &mueller::specular_transmission<Float>,
        "cos_theta_i"_a, "eta"_a, D(mueller, specular_transmission))
    .def("stokes_basis", &mueller::stokes_basis<Vector3f>,
        "w"_a, D(mueller, stokes_basis))
    .def("rotate_stokes_basis", &mueller::rotate_stokes_basis<Vector3f>,
        "wi"_a, "basis_current"_a, "basis_target"_a, D(mueller, rotate_stokes_basis))
    .def("rotate_mueller_basis", &mueller::rotate_mueller_basis<Vector3f>,
        "M"_a, "in_forward"_a, "in_basis_current"_a, "in_basis_target"_a,
        "out_forward"_a, "out_basis_current"_a, "out_basis_target"_a,
        D(mueller, rotate_mueller_basis))
    .def("rotate_mueller_basis_collinear",
        &mueller::rotate_mueller_basis_collinear<Vector3f>,
        "M"_a, "forward"_a, "basis_current"_a, "basis_target"_a,
        D(mueller, rotate_mueller_basis_collinear))
    .def("unit_angle", [](const Vector3f &a, const Vector3f &b) {
                return unit_angle(a, b);
    }, "a"_a, "b"_a)
    ;
}
