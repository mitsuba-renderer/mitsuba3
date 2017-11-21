#include <mitsuba/python/python.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/medium.h>
#include <enoki/stl.h>

template <typename Point3>
auto bind_interaction(py::module &m, const char *name) {
    using Type = SurfaceInteraction<Point3>;
    using Vector3 = typename Type::Vector3;
    using RayDifferential3 = typename Type::RayDifferential3;
    using Value = typename Type::Value;
    using Mask = typename Type::Mask;

    return py::class_<Type>(m, name, D(SurfaceInteraction))
        .def(py::init<>(), D(SurfaceInteraction, SurfaceInteraction))
        .def("compute_partials", &Type::compute_partials, "ray"_a,
             D(SurfaceInteraction, compute_partials))
        .def("to_world", &Type::to_world, D(SurfaceInteraction, to_world))
        .def("to_local", &Type::to_local, D(SurfaceInteraction, to_local))
        .def("is_valid", &Type::is_valid, D(SurfaceInteraction, is_valid))
        .def("is_emitter", &Type::is_emitter, D(SurfaceInteraction, is_emitter))
        .def("is_sensor", &Type::is_sensor, D(SurfaceInteraction, is_sensor))
        .def("is_medium_transition", &Type::is_medium_transition,
             D(SurfaceInteraction, is_medium_transition))
        .def("target_medium",
             py::overload_cast<const Vector3 &>(&Type::target_medium, py::const_),
             "d"_a, D(SurfaceInteraction, target_medium))
        .def("target_medium",
             py::overload_cast<const Value &>(&Type::target_medium, py::const_),
             "cos_theta"_a, D(SurfaceInteraction, target_medium, 2))
        .def("bsdf", py::overload_cast<const RayDifferential3 &,
                                       const Mask &>(&Type::bsdf),
             "ray"_a, "active"_a, D(SurfaceInteraction, bsdf))
        .def("bsdf", py::overload_cast<>(&Type::bsdf, py::const_),
             D(SurfaceInteraction, bsdf, 2))

        // Members
        .def_readwrite("shape", &Type::shape, D(SurfaceInteraction, shape))
        .def_readwrite("t", &Type::t, D(SurfaceInteraction, t))
        .def_readwrite("time", &Type::time, D(SurfaceInteraction, time))
        .def_readwrite("p", &Type::p, D(SurfaceInteraction, p))
        .def_readwrite("n", &Type::n, D(SurfaceInteraction, n))
        .def_readwrite("uv", &Type::uv, D(SurfaceInteraction, uv))
        .def_readwrite("sh_frame", &Type::sh_frame, D(SurfaceInteraction, sh_frame))
        .def_readwrite("dp_du", &Type::dp_du, D(SurfaceInteraction, dp_du))
        .def_readwrite("dp_dv", &Type::dp_dv, D(SurfaceInteraction, dp_dv))
        .def_readwrite("duv_dx", &Type::duv_dx, D(SurfaceInteraction, duv_dx))
        .def_readwrite("duv_dy", &Type::duv_dy, D(SurfaceInteraction, duv_dy))
        .def_readwrite("time", &Type::time, D(SurfaceInteraction, time))
        .def_readwrite("wi", &Type::wi, D(SurfaceInteraction, wi))
        .def_readwrite("prim_index", &Type::prim_index, D(SurfaceInteraction, prim_index))
        .def_readwrite("instance", &Type::instance, D(SurfaceInteraction, instance))
        .def_readwrite("has_uv_partials", &Type::has_uv_partials, D(SurfaceInteraction, has_uv_partials))
        .def("__repr__", [](const Type &it) {
            std::ostringstream oss;
            oss << it;
            return oss.str();
        });
}

MTS_PY_EXPORT(SurfaceInteraction) {
    bind_interaction<Point3f>(m, "SurfaceInteraction3f")
        .def("adjust_time", &SurfaceInteraction3f::adjust_time,
             "time"_a, D(SurfaceInteraction, adjust_time))
        .def("normal_derivative", &SurfaceInteraction3f::normal_derivative,
             "shading_frame"_a = true, "active"_a = true,
             D(SurfaceInteraction, normal_derivative));

    auto si3fx = bind_interaction<Point3fX>(m, "SurfaceInteraction3fX");
    bind_slicing_operators<SurfaceInteraction3fX, SurfaceInteraction3f>(si3fx);
}
