#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/records.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/python/python.h>
#include <nanobind/stl/string.h>

MI_PY_EXPORT(Interaction) {
    MI_PY_IMPORT_TYPES()

    auto it = nb::class_<Interaction3f>(m, "Interaction3f", D(Interaction))
        // Members
        .def_field(Interaction3f, t,           D(Interaction, t))
        .def_field(Interaction3f, time,        D(Interaction, time))
        .def_field(Interaction3f, wavelengths, D(Interaction, wavelengths))
        .def_field(Interaction3f, p,           D(Interaction, p))
        .def_field(Interaction3f, n,           D(Interaction, n))
        // Methods
        .def(nb::init<>(), D(Interaction, Interaction))
        .def(nb::init<const Interaction3f &>(), "Copy constructor")
        .def(nb::init<Float, Float, Wavelength, Point3f, Normal3f>(),
             "t"_a, "time"_a, "wavelengths"_a, "p"_a, "n"_a = 0,
             D(Interaction, Interaction, 2))
        .def("zero_",        &Interaction3f::zero_, "size"_a = 1)
        .def("spawn_ray",    &Interaction3f::spawn_ray, "d"_a,    D(Interaction, spawn_ray))
        .def("spawn_ray_to", &Interaction3f::spawn_ray_to, "t"_a, D(Interaction, spawn_ray_to))
        .def("is_valid",     &Interaction3f::is_valid,     D(Interaction, is_valid))
        .def("zero_",        &Interaction3f::zero_, D(Interaction, zero))
        .def_repr(Interaction3f);

    MI_PY_DRJIT_STRUCT(it, Interaction3f, t, time, wavelengths, p, n)
}

MI_PY_EXPORT(SurfaceInteraction) {
    MI_PY_IMPORT_TYPES()
    auto si =
        nb::class_<SurfaceInteraction3f, Interaction3f>(m, "SurfaceInteraction3f",
                                                        D(SurfaceInteraction))
        // Members
        .def_field(SurfaceInteraction3f, shape,         "shape"_a.none(), D(SurfaceInteraction, shape))
        .def_field(SurfaceInteraction3f, uv,            D(SurfaceInteraction, uv))
        .def_field(SurfaceInteraction3f, sh_frame,      D(SurfaceInteraction, sh_frame))
        .def_field(SurfaceInteraction3f, dp_du,         D(SurfaceInteraction, dp_du))
        .def_field(SurfaceInteraction3f, dp_dv,         D(SurfaceInteraction, dp_dv))
        .def_field(SurfaceInteraction3f, dn_du,         D(SurfaceInteraction, dn_du))
        .def_field(SurfaceInteraction3f, dn_dv,         D(SurfaceInteraction, dn_dv))
        .def_field(SurfaceInteraction3f, duv_dx,        D(SurfaceInteraction, duv_dx))
        .def_field(SurfaceInteraction3f, duv_dy,        D(SurfaceInteraction, duv_dy))
        .def_field(SurfaceInteraction3f, wi,            D(SurfaceInteraction, wi))
        .def_field(SurfaceInteraction3f, prim_index,    D(SurfaceInteraction, prim_index))
        .def_field(SurfaceInteraction3f, instance,      "instance"_a.none(), D(SurfaceInteraction, instance))

        // Methods
        .def(nb::init<>(), D(SurfaceInteraction, SurfaceInteraction))
        .def(nb::init<const SurfaceInteraction3f &>(), "Copy constructor")
        .def(nb::init<const PositionSample3f &, const Wavelength &>(), "ps"_a,
            "wavelengths"_a, D(SurfaceInteraction, SurfaceInteraction))
        .def("initialize_sh_frame", &SurfaceInteraction3f::initialize_sh_frame,
            D(SurfaceInteraction, initialize_sh_frame))
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
            nb::overload_cast<const Vector3f &>(&SurfaceInteraction3f::target_medium,
                                                nb::const_),
            "d"_a, D(SurfaceInteraction, target_medium))
        .def("target_medium",
            nb::overload_cast<const Float &>(&SurfaceInteraction3f::target_medium,
                                            nb::const_),
            "cos_theta"_a, D(SurfaceInteraction, target_medium, 2))
        .def("bsdf",
            nb::overload_cast<const RayDifferential3f &>(&SurfaceInteraction3f::bsdf),
            "ray"_a, D(SurfaceInteraction, bsdf))
        .def("bsdf", nb::overload_cast<>(&SurfaceInteraction3f::bsdf, nb::const_),
            D(SurfaceInteraction, bsdf, 2))
        .def("compute_uv_partials", &SurfaceInteraction3f::compute_uv_partials, "ray"_a,
            D(SurfaceInteraction, compute_uv_partials))
        .def("has_uv_partials", &SurfaceInteraction3f::has_uv_partials,
            D(SurfaceInteraction, has_uv_partials))
        .def("has_n_partials", &SurfaceInteraction3f::has_n_partials,
            D(SurfaceInteraction, has_n_partials))
        .def_repr(SurfaceInteraction3f);

    MI_PY_DRJIT_STRUCT(si, SurfaceInteraction3f, t, time, wavelengths, p, n,
                       shape, uv, sh_frame, dp_du, dp_dv, dn_du, dn_dv, duv_dx,
                       duv_dy, wi, prim_index, instance)
}

MI_PY_EXPORT(MediumInteraction) {
    MI_PY_IMPORT_TYPES()
    auto mi =
        nb::class_<MediumInteraction3f, Interaction3f>(m, "MediumInteraction3f",
                                                        D(MediumInteraction))
        // Members
        .def_field(MediumInteraction3f, medium,     D(MediumInteraction, medium))
        .def_field(MediumInteraction3f, sh_frame,   D(MediumInteraction, sh_frame))
        .def_field(MediumInteraction3f, wi,         D(MediumInteraction, wi))
        .def_field(MediumInteraction3f, sigma_s,    D(MediumInteraction, sigma_s))
        .def_field(MediumInteraction3f, sigma_n,    D(MediumInteraction, sigma_n))
        .def_field(MediumInteraction3f, sigma_t,    D(MediumInteraction, sigma_t))
        .def_field(MediumInteraction3f, combined_extinction, D(MediumInteraction, combined_extinction))
        .def_field(MediumInteraction3f, mint, D(MediumInteraction, mint))

        // Methods
        .def(nb::init<>(), D(MediumInteraction, MediumInteraction))
        .def(nb::init<const MediumInteraction3f &>(), "Copy constructor")
        .def("to_world", &MediumInteraction3f::to_world, "v"_a, D(MediumInteraction, to_world))
        .def("to_local", &MediumInteraction3f::to_local, "v"_a, D(MediumInteraction, to_local))
        .def_repr(MediumInteraction3f);

    MI_PY_DRJIT_STRUCT(mi, MediumInteraction3f, t, time, wavelengths, p, n,
                       medium, sh_frame, wi, sigma_s, sigma_n, sigma_t,
                       combined_extinction, mint)
}

MI_PY_EXPORT(PreliminaryIntersection) {
    MI_PY_IMPORT_TYPES()

    m.def("has_flag", [](uint32_t f0, RayFlags f1) { return has_flag(f0, f1); });
    m.def("has_flag", [](UInt32   f0, RayFlags f1) { return has_flag(f0, f1); });

    auto pi =
        nb::class_<PreliminaryIntersection3f>(m, "PreliminaryIntersection3f",
                                              D(PreliminaryIntersection))
        // Members
        .def_field(PreliminaryIntersection3f, t,           D(PreliminaryIntersection, t))
        .def_field(PreliminaryIntersection3f, prim_uv,     D(PreliminaryIntersection, prim_uv))
        .def_field(PreliminaryIntersection3f, prim_index,  D(PreliminaryIntersection, prim_index))
        .def_field(PreliminaryIntersection3f, shape_index, D(PreliminaryIntersection, shape_index))
        .def_field(PreliminaryIntersection3f, shape,       D(PreliminaryIntersection, shape))
        .def_field(PreliminaryIntersection3f, instance,    D(PreliminaryIntersection, instance))

        // Methods
        .def(nb::init<>(), D(PreliminaryIntersection, PreliminaryIntersection))
        .def(nb::init<const PreliminaryIntersection3f &>(), "Copy constructor")
        .def("is_valid", &PreliminaryIntersection3f::is_valid, D(PreliminaryIntersection, is_valid))
        .def("compute_surface_interaction",
           // GCC 13.2.0 miscompiles the bindings below unless its wrapped in a lambda
           [](PreliminaryIntersection3f& pi, const Ray3f &&ray, uint32_t ray_flags, Mask active) {
               return pi.compute_surface_interaction(
                       std::forward<const Ray3f&>(ray), ray_flags, active);
           },
           D(PreliminaryIntersection, compute_surface_interaction),
           "ray"_a, "ray_flags"_a = +RayFlags::All, "active"_a = true)
        .def("zero_", &PreliminaryIntersection3f::zero_, D(PreliminaryIntersection, zero))
        .def_repr(PreliminaryIntersection3f);

    MI_PY_DRJIT_STRUCT(pi, PreliminaryIntersection3f, t, prim_uv, prim_index,
                        shape_index, shape, instance);
}
