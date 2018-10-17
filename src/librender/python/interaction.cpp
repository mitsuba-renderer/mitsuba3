#include <mitsuba/python/python.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/scene.h>
#include <enoki/stl.h>

template <typename Point3>
auto bind_interaction(py::module &m, const char *name) {
    using Type = Interaction<Point3>;

    return py::class_<Type>(m, name, D(Interaction))
        // Members
        .def_readwrite("t", &Type::t, D(Interaction, t))
        .def_readwrite("time", &Type::time, D(Interaction, time))
        .def_readwrite("wavelengths", &Type::wavelengths, D(Interaction, wavelengths))
        .def_readwrite("p", &Type::p, D(Interaction, p))

        // Methods
        .def(py::init<>(), D(Interaction, Interaction))
        .def("spawn_ray", &Type::spawn_ray, D(Interaction, spawn_ray))
        .def("spawn_ray_to", &Type::spawn_ray_to, D(Interaction, spawn_ray_to))
        .def("is_valid", &Type::is_valid, D(Interaction, is_valid));
}

MTS_PY_EXPORT(Interaction) {
    bind_interaction<Point3f>(m, "Interaction3f");
    auto i3fx = bind_interaction<Point3fX>(m, "Interaction3fX");
    bind_slicing_operators<Interaction3fX, Interaction3f>(i3fx);
}

template <typename Point3>
auto bind_surface_interaction(py::module &m, const char *name) {
    using Type             = SurfaceInteraction<Point3>;
    using Base             = typename Type::Base;
    using Vector3          = typename Type::Vector3;
    using RayDifferential3 = typename Type::RayDifferential3;
    using Value            = typename Type::Value;
    using PositionSample3  = mitsuba::PositionSample<Point<Value, 3>>;
    using Spectrum         = mitsuba::Spectrum<Value>;

    return py::class_<Type, Base>(m, name, D(SurfaceInteraction))
        // Members
        .def_readwrite("shape", &Type::shape, D(SurfaceInteraction, shape))
        .def_readwrite("uv", &Type::uv, D(SurfaceInteraction, uv))
        .def_readwrite("n", &Type::n, D(SurfaceInteraction, n))
        .def_readwrite("sh_frame", &Type::sh_frame, D(SurfaceInteraction, sh_frame))
        .def_readwrite("dp_du", &Type::dp_du, D(SurfaceInteraction, dp_du))
        .def_readwrite("dp_dv", &Type::dp_dv, D(SurfaceInteraction, dp_dv))
        .def_readwrite("duv_dx", &Type::duv_dx, D(SurfaceInteraction, duv_dx))
        .def_readwrite("duv_dy", &Type::duv_dy, D(SurfaceInteraction, duv_dy))
        .def_readwrite("wi", &Type::wi, D(SurfaceInteraction, wi))
        .def_readwrite("prim_index", &Type::prim_index, D(SurfaceInteraction, prim_index))
        .def_readwrite("instance", &Type::instance, D(SurfaceInteraction, instance))
        .def_readwrite("has_uv_partials", &Type::has_uv_partials, D(SurfaceInteraction, has_uv_partials))
        // Methods
        .def(py::init<>(), D(SurfaceInteraction, SurfaceInteraction))
        .def(py::init<const PositionSample3 &, const Spectrum &>(),
             "ps"_a, "wavelengths"_a, D(SurfaceInteraction, SurfaceInteraction))
        .def("compute_partials", &Type::compute_partials, "ray"_a,
             D(SurfaceInteraction, compute_partials))
        .def("to_world", &Type::to_world, D(SurfaceInteraction, to_world))
        .def("to_local", &Type::to_local, D(SurfaceInteraction, to_local))
        .def("emitter", &Type::template emitter<Scene>, D(SurfaceInteraction, emitter))
        .def("is_sensor", &Type::is_sensor, D(SurfaceInteraction, is_sensor))
        .def("is_medium_transition", &Type::is_medium_transition,
             D(SurfaceInteraction, is_medium_transition))
        .def("target_medium",
             py::overload_cast<const Vector3 &>(
                    &Type::target_medium, py::const_),
             "d"_a, D(SurfaceInteraction, target_medium))
        .def("target_medium",
             py::overload_cast<const Value &>(
                    &Type::target_medium, py::const_),
             "cos_theta"_a, D(SurfaceInteraction, target_medium, 2))
        .def("bsdf", py::overload_cast<const RayDifferential3 &>(&Type::bsdf),
             "ray"_a, D(SurfaceInteraction, bsdf))
        .def("bsdf", py::overload_cast<>(&Type::bsdf, py::const_),
             D(SurfaceInteraction, bsdf, 2))
        .def("__repr__", [](const Type &it) {
            std::ostringstream oss;
            oss << it;
            return oss.str();
        });
}

MTS_PY_EXPORT(SurfaceInteraction) {
    bind_surface_interaction<Point3f>(m, "SurfaceInteraction3f")
        .def("normal_derivative", &SurfaceInteraction3f::normal_derivative,
             "shading_frame"_a = true, "active"_a = true,
             D(SurfaceInteraction, normal_derivative));

    auto si3fx = bind_surface_interaction<Point3fX>(m, "SurfaceInteraction3fX");
    bind_slicing_operators<SurfaceInteraction3fX, SurfaceInteraction3f>(si3fx);
}
