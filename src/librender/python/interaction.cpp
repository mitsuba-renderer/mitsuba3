#include <mitsuba/python/python.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/scene.h>
#include <enoki/stl.h>

MTS_PY_EXPORT_VARIANTS(Interaction) {
    py::class_<Interaction>(m, "Interaction3f", D(Interaction))
        // Members
        .rwdef(Interaction, t)
        .rwdef(Interaction, time)
        .rwdef(Interaction, wavelengths)
        .rwdef(Interaction, p)

        // Methods
        .def(py::init<>(), D(Interaction, Interaction))
        .mdef(Interaction, spawn_ray)
        .mdef(Interaction, spawn_ray_to)
        .mdef(Interaction, is_valid)
        .def("__repr__",
            [](const Interaction &it) {
                std::ostringstream oss;
                oss << it;
                return oss.str();
            })
        ;
}

MTS_PY_EXPORT_VARIANTS(SurfaceInteraction) {
    using Base = typename SurfaceInteraction::Base;
    using Vector3f = typename SurfaceInteraction::Vector3f;
    using RayDifferential3f = typename SurfaceInteraction::RayDifferential3f;
    using PositionSample3f = typename SurfaceInteraction::PositionSample3f;
    using Wavelength = typename SurfaceInteraction::Wavelength;

    py::class_<SurfaceInteraction, Base>(m, "SurfaceInteraction3f", D(SurfaceInteraction))
        // Members
        .rwdef(SurfaceInteraction, shape)
        .rwdef(SurfaceInteraction, uv)
        .rwdef(SurfaceInteraction, n)
        .rwdef(SurfaceInteraction, sh_frame)
        .rwdef(SurfaceInteraction, dp_du)
        .rwdef(SurfaceInteraction, dp_dv)
        .rwdef(SurfaceInteraction, duv_dx)
        .rwdef(SurfaceInteraction, duv_dy)
        .rwdef(SurfaceInteraction, wi)
        .rwdef(SurfaceInteraction, prim_index)
        .rwdef(SurfaceInteraction, instance)
        // Methods
        .def(py::init<>(), D(SurfaceInteraction, SurfaceInteraction))
        .def(py::init<const PositionSample3f &, const Wavelength &>(),
             "ps"_a, "wavelengths"_a, D(SurfaceInteraction, SurfaceInteraction))
        .mdef(SurfaceInteraction, to_world)
        .mdef(SurfaceInteraction, to_local)
        .mdef(SurfaceInteraction, to_world_mueller)
        .mdef(SurfaceInteraction, to_local_mueller)
        .mdef(SurfaceInteraction, emitter, "scene"_a, "active"_a=true)
        .mdef(SurfaceInteraction, is_sensor)
        .mdef(SurfaceInteraction, is_medium_transition)
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
        .mdef(SurfaceInteraction, normal_derivative)
        .mdef(SurfaceInteraction, compute_partials)
        .mdef(SurfaceInteraction, has_uv_partials)
        .def("__repr__",
            [](const SurfaceInteraction &it) {
                std::ostringstream oss;
                oss << it;
                return oss.str();
            })
        ;
}
