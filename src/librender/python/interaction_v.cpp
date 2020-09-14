#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/records.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(Interaction) {
    MTS_PY_IMPORT_TYPES()
    auto inter = py::class_<Interaction3f>(m, "Interaction3f", D(Interaction))
        // Members
        .def_field(Interaction3f, t,           D(Interaction, t))
        .def_field(Interaction3f, time,        D(Interaction, time))
        .def_field(Interaction3f, wavelengths, D(Interaction, wavelengths))
        .def_field(Interaction3f, p,           D(Interaction, p))
        // Methods
        .def(py::init<>(), D(Interaction, Interaction))
        .def("spawn_ray",    &Interaction3f::spawn_ray, "d"_a,    D(Interaction, spawn_ray))
        .def("spawn_ray_to", &Interaction3f::spawn_ray_to, "t"_a, D(Interaction, spawn_ray_to))
        .def("is_valid",     &Interaction3f::is_valid,     D(Interaction, is_valid))
        .def_repr(Interaction3f);
    bind_struct_support<Interaction3f>(inter);
}

MTS_PY_EXPORT(SurfaceInteraction) {
    MTS_PY_IMPORT_TYPES()
    auto inter =
        py::class_<SurfaceInteraction3f, Interaction3f>(m, "SurfaceInteraction3f",
                                                        D(SurfaceInteraction))
        // Members
        .def_field(SurfaceInteraction3f, shape,      D(SurfaceInteraction, shape))
        .def_field(SurfaceInteraction3f, uv,         D(SurfaceInteraction, uv))
        .def_field(SurfaceInteraction3f, n,          D(SurfaceInteraction, n))
        .def_field(SurfaceInteraction3f, sh_frame,   D(SurfaceInteraction, sh_frame))
        .def_field(SurfaceInteraction3f, dp_du,      D(SurfaceInteraction, dp_du))
        .def_field(SurfaceInteraction3f, dp_dv,      D(SurfaceInteraction, dp_dv))
        .def_field(SurfaceInteraction3f, dn_du,      D(SurfaceInteraction, dn_du))
        .def_field(SurfaceInteraction3f, dn_dv,      D(SurfaceInteraction, dn_dv))
        .def_field(SurfaceInteraction3f, duv_dx,     D(SurfaceInteraction, duv_dx))
        .def_field(SurfaceInteraction3f, duv_dy,     D(SurfaceInteraction, duv_dy))
        .def_field(SurfaceInteraction3f, wi,         D(SurfaceInteraction, wi))
        .def_field(SurfaceInteraction3f, prim_index, D(SurfaceInteraction, prim_index))
        .def_field(SurfaceInteraction3f, instance,   D(SurfaceInteraction, instance))

        // Methods
        .def(py::init<>(), D(SurfaceInteraction, SurfaceInteraction))
        .def(py::init<const PositionSample3f &, const Wavelength &>(), "ps"_a,
            "wavelengths"_a, D(SurfaceInteraction, SurfaceInteraction))
        .def("to_world", &SurfaceInteraction3f::to_world, "v"_a, D(SurfaceInteraction, to_world))
        .def("to_local", &SurfaceInteraction3f::to_local, "v"_a, D(SurfaceInteraction, to_local))
        .def("to_world_mueller", &SurfaceInteraction3f::to_world_mueller, "M_local"_a,
            "wi_local"_a, "wo_local"_a, D(SurfaceInteraction, to_world_mueller))
        .def("to_local_mueller", &SurfaceInteraction3f::to_local_mueller, "M_world"_a,
            "wi_world"_a, "wo_world"_a, D(SurfaceInteraction, to_local_mueller))
        .def("emitter", &SurfaceInteraction3f::emitter, D(SurfaceInteraction, emitter),
            "scene"_a, "active"_a = true)
        .def("is_sensor", &SurfaceInteraction3f::is_sensor, D(SurfaceInteraction, is_sensor))
        .def("is_medium_transition", &SurfaceInteraction3f::is_medium_transition,
            D(SurfaceInteraction, is_medium_transition))
        .def("target_medium",
            py::overload_cast<const Vector3f &>(&SurfaceInteraction3f::target_medium,
                                                py::const_),
            "d"_a, D(SurfaceInteraction, target_medium))
        .def("target_medium",
            py::overload_cast<const Float &>(&SurfaceInteraction3f::target_medium,
                                            py::const_),
            "cos_theta"_a, D(SurfaceInteraction, target_medium, 2))
        .def("bsdf",
            py::overload_cast<const RayDifferential3f &>(&SurfaceInteraction3f::bsdf),
            "ray"_a, D(SurfaceInteraction, bsdf))
        .def("bsdf", py::overload_cast<>(&SurfaceInteraction3f::bsdf, py::const_),
            D(SurfaceInteraction, bsdf, 2))
        .def("compute_uv_partials", &SurfaceInteraction3f::compute_uv_partials, "ray"_a,
            D(SurfaceInteraction, compute_uv_partials))
        .def("has_uv_partials", &SurfaceInteraction3f::has_uv_partials,
            D(SurfaceInteraction, has_uv_partials))
        .def("has_n_partials", &SurfaceInteraction3f::has_n_partials,
            D(SurfaceInteraction, has_n_partials))
        .def_repr(SurfaceInteraction3f);

    bind_struct_support<SurfaceInteraction3f>(inter);
}

MTS_PY_EXPORT(MediumInteraction) {
    MTS_PY_IMPORT_TYPES()
    auto inter =
        py::class_<MediumInteraction3f, Interaction3f>(m, "MediumInteraction3f",
                                                        D(MediumInteraction))
        // Members
        .def_field(MediumInteraction3f, medium,     D(MediumInteraction, medium))
        .def_field(MediumInteraction3f, sh_frame,   D(MediumInteraction, sh_frame))
        .def_field(MediumInteraction3f, wi,         D(MediumInteraction, wi))

        // Methods
        .def(py::init<>(), D(MediumInteraction, MediumInteraction))
        .def("to_world", &MediumInteraction3f::to_world, "v"_a, D(MediumInteraction, to_world))
        .def("to_local", &MediumInteraction3f::to_local, "v"_a, D(MediumInteraction, to_local))
        .def_repr(MediumInteraction3f);

    bind_struct_support<MediumInteraction3f>(inter);
}

MTS_PY_EXPORT(PreliminaryIntersection) {
    MTS_PY_IMPORT_TYPES()

    m.def("has_flag", [](HitComputeFlags f0, HitComputeFlags f1) { return has_flag(f0, f1); });

    auto pi =
        py::class_<PreliminaryIntersection3f>(m, "PreliminaryIntersection3f",
                                              D(PreliminaryIntersection))
        // Members
        .def_field(PreliminaryIntersection3f, t,           D(PreliminaryIntersection, t))
        .def_field(PreliminaryIntersection3f, prim_uv,     D(PreliminaryIntersection, prim_uv))
        .def_field(PreliminaryIntersection3f, prim_index,  D(PreliminaryIntersection, prim_index))
        .def_field(PreliminaryIntersection3f, shape_index, D(PreliminaryIntersection, shape_index))
        .def_field(PreliminaryIntersection3f, shape,       D(PreliminaryIntersection, shape))
        .def_field(PreliminaryIntersection3f, instance,    D(PreliminaryIntersection, instance))

        // Methods
        .def(py::init<>(), D(PreliminaryIntersection, PreliminaryIntersection))
        .def("is_valid", &PreliminaryIntersection3f::is_valid, D(PreliminaryIntersection, is_valid))
        .def("compute_surface_interaction",
           &PreliminaryIntersection3f::compute_surface_interaction,
           D(PreliminaryIntersection, compute_surface_interaction),
           "ray"_a, "flags"_a = HitComputeFlags::All, "active"_a = true)
        .def_repr(PreliminaryIntersection3f);

    bind_struct_support<PreliminaryIntersection3f>(pi);
}