#include <mitsuba/python/python.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/medium.h>

MTS_PY_EXPORT_CLASS_VARIANTS(Interaction) {
    py::class_<Interaction>(m, "Interaction3f", D(Interaction))
        // Members
        .def_field(Interaction, t)
        .def_field(Interaction, time)
        .def_field(Interaction, wavelengths)
        .def_field(Interaction, p)

        // Methods
        .def(py::init<>(), D(Interaction, Interaction))
        .def_method(Interaction, spawn_ray)
        .def_method(Interaction, spawn_ray_to)
        .def_method(Interaction, is_valid)
        .def_repr(Interaction)
        ;
}

MTS_PY_EXPORT_CLASS_VARIANTS(SurfaceInteraction) {
    using Base = typename SurfaceInteraction::Base;
    using Vector3f = typename SurfaceInteraction::Vector3f;
    using RayDifferential3f = typename SurfaceInteraction::RayDifferential3f;
    using PositionSample3f = typename SurfaceInteraction::PositionSample3f;
    using Wavelength = typename SurfaceInteraction::Wavelength;

    py::class_<SurfaceInteraction, Base>(m, "SurfaceInteraction3f", D(SurfaceInteraction))
        // Members
        .def_field(SurfaceInteraction, shape)
        .def_field(SurfaceInteraction, uv)
        .def_field(SurfaceInteraction, n)
        .def_field(SurfaceInteraction, sh_frame)
        .def_field(SurfaceInteraction, dp_du)
        .def_field(SurfaceInteraction, dp_dv)
        .def_field(SurfaceInteraction, duv_dx)
        .def_field(SurfaceInteraction, duv_dy)
        .def_field(SurfaceInteraction, wi)
        .def_field(SurfaceInteraction, prim_index)
        .def_field(SurfaceInteraction, instance)
        // Methods
        .def(py::init<>(), D(SurfaceInteraction, SurfaceInteraction))
        .def(py::init<const PositionSample3f &, const Wavelength &>(),
             "ps"_a, "wavelengths"_a, D(SurfaceInteraction, SurfaceInteraction))
        .def_method(SurfaceInteraction, to_world)
        .def_method(SurfaceInteraction, to_local)
        .def_method(SurfaceInteraction, to_world_mueller)
        .def_method(SurfaceInteraction, to_local_mueller)
        .def_method(SurfaceInteraction, emitter, "scene"_a, "active"_a=true)
        .def_method(SurfaceInteraction, is_sensor)
        .def_method(SurfaceInteraction, is_medium_transition)
        .def("target_medium",
             py::overload_cast<const Vector3f &>(&SurfaceInteraction::target_medium, py::const_),
             "d"_a, D(SurfaceInteraction, target_medium))
        .def("target_medium",
             py::overload_cast<const Float &>(&SurfaceInteraction::target_medium, py::const_),
             "cos_theta"_a, D(SurfaceInteraction, target_medium, 2))
        .def("bsdf", py::overload_cast<const RayDifferential3f &>(&SurfaceInteraction::bsdf),
             "ray"_a, D(SurfaceInteraction, bsdf))
        .def("bsdf", py::overload_cast<>(&SurfaceInteraction::bsdf, py::const_),
             D(SurfaceInteraction, bsdf, 2))
        .def_method(SurfaceInteraction, normal_derivative)
        .def_method(SurfaceInteraction, compute_partials)
        .def_method(SurfaceInteraction, has_uv_partials)
        .def_repr(SurfaceInteraction)
        ;
}
