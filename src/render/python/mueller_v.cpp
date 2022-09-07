#include <mitsuba/render/mueller.h>
#include <mitsuba/python/python.h>
#include <drjit/sphere.h>

MI_PY_EXPORT(mueller) {
    MI_PY_IMPORT_TYPES()

    m.def("depolarizer", &mueller::depolarizer<Float>,
          "value"_a = 1.f, D(mueller, depolarizer));
    m.def("depolarizer", &mueller::depolarizer<UnpolarizedSpectrum>,
          "value"_a = 1.f, D(mueller, depolarizer));

    m.def("absorber", &mueller::absorber<Float>,
          "value"_a, D(mueller, absorber));
    m.def("absorber", &mueller::absorber<UnpolarizedSpectrum>,
          "value"_a, D(mueller, absorber));

    m.def("linear_polarizer", &mueller::linear_polarizer<Float>,
          "value"_a = 1.f, D(mueller, linear_polarizer));
    m.def("linear_polarizer", &mueller::linear_polarizer<UnpolarizedSpectrum>,
          "value"_a = 1.f, D(mueller, linear_polarizer));

    m.def("linear_retarder", &mueller::linear_retarder<Float>,
          "phase"_a, D(mueller, linear_retarder));
    m.def("linear_retarder", &mueller::linear_retarder<UnpolarizedSpectrum>,
          "phase"_a, D(mueller, linear_retarder));

    m.def("right_circular_polarizer", &mueller::right_circular_polarizer<Float>,
          D(mueller, right_circular_polarizer));
    m.def("right_circular_polarizer", &mueller::right_circular_polarizer<UnpolarizedSpectrum>,
          D(mueller, right_circular_polarizer));

    m.def("left_circular_polarizer", &mueller::left_circular_polarizer<Float>,
          D(mueller, left_circular_polarizer));
    m.def("left_circular_polarizer", &mueller::left_circular_polarizer<UnpolarizedSpectrum>,
          D(mueller, left_circular_polarizer));

    m.def("diattenuator", &mueller::diattenuator<Float>,
          "x"_a, "y"_a, D(mueller, diattenuator));
    m.def("diattenuator", &mueller::diattenuator<UnpolarizedSpectrum>,
          "x"_a, "y"_a, D(mueller, diattenuator));

    m.def("rotator", &mueller::rotator<Float>,
          "theta"_a, D(mueller, rotator));
    m.def("rotator", &mueller::rotator<UnpolarizedSpectrum>,
          "theta"_a, D(mueller, rotator));

    m.def("rotated_element", &mueller::rotated_element<Float>,
          "theta"_a, "M"_a, D(mueller, rotated_element));
    m.def("rotated_element", &mueller::rotated_element<UnpolarizedSpectrum>,
          "theta"_a, "M"_a, D(mueller, rotated_element));

    m.def("specular_reflection", &mueller::specular_reflection<Float, dr::Complex<Float>>,
          "cos_theta_i"_a, "eta"_a, D(mueller, specular_reflection));
    m.def("specular_reflection", &mueller::specular_reflection<UnpolarizedSpectrum, dr::Complex<UnpolarizedSpectrum>>,
          "cos_theta_i"_a, "eta"_a, D(mueller, specular_reflection));

    m.def("specular_transmission", &mueller::specular_transmission<Float>,
          "cos_theta_i"_a, "eta"_a, D(mueller, specular_transmission));
    m.def("specular_transmission", &mueller::specular_transmission<UnpolarizedSpectrum>,
          "cos_theta_i"_a, "eta"_a, D(mueller, specular_transmission));

    m.def("stokes_basis", &mueller::stokes_basis<Vector3f>,
          "w"_a, D(mueller, stokes_basis));

    m.def("rotate_stokes_basis", &mueller::rotate_stokes_basis<Vector3f>,
          "wi"_a, "basis_current"_a, "basis_target"_a, D(mueller, rotate_stokes_basis));
    m.def("rotate_stokes_basis_m", &mueller::rotate_stokes_basis<Vector3f, Float, MuellerMatrix<UnpolarizedSpectrum>>,
          "wi"_a, "basis_current"_a, "basis_target"_a, D(mueller, rotate_stokes_basis));

    m.def("rotate_mueller_basis", &mueller::rotate_mueller_basis<Vector3f>,
          "M"_a, "in_forward"_a, "in_basis_current"_a, "in_basis_target"_a,
          "out_forward"_a, "out_basis_current"_a,
          "out_basis_target"_a, D(mueller, rotate_mueller_basis));
    m.def("rotate_mueller_basis", &mueller::rotate_mueller_basis<Vector3f, Float, MuellerMatrix<UnpolarizedSpectrum>>,
          "M"_a, "in_forward"_a, "in_basis_current"_a, "in_basis_target"_a,
          "out_forward"_a, "out_basis_current"_a,
          "out_basis_target"_a, D(mueller, rotate_mueller_basis));

    m.def("rotate_mueller_basis_collinear",
          &mueller::rotate_mueller_basis_collinear<Vector3f>,
          "M"_a, "forward"_a, "basis_current"_a, "basis_target"_a,
          D(mueller, rotate_mueller_basis_collinear));
    m.def("rotate_mueller_basis_collinear",
          &mueller::rotate_mueller_basis_collinear<Vector3f, Float, MuellerMatrix<UnpolarizedSpectrum>>,
          "M"_a, "forward"_a, "basis_current"_a, "basis_target"_a,
          D(mueller, rotate_mueller_basis_collinear));

    m.def("unit_angle", [](const Vector3f &a, const Vector3f &b) { return dr::unit_angle(a, b); },
          "a"_a, "b"_a);
}
