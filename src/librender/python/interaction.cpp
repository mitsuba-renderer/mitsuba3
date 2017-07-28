#include <mitsuba/render/interaction.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/python/python.h>

using namespace pybind11::literals;

template <typename Point3>
auto bind_interaction(py::module &m, const char *name) {
    using Type = SurfaceInteraction<Point3>;

    return py::class_<Type>(m, name, D(SurfaceInteraction))
        // Methods
        //.def("to_world", &Type::to_world, D(SurfaceInteraction, to_world))
        //.def("to_local", &Type::to_local, D(SurfaceInteraction, to_local))
        //.def("is_valid", &Type::is_valid, D(SurfaceInteraction, is_valid))
        //.def("is_emitter", &Type::is_emitter, D(SurfaceInteraction, is_emitter))
        //.def("is_sensor", &Type::is_sensor, D(SurfaceInteraction, is_sensor))
        //.def("has_subsurface", &Type::has_subsurface, D(SurfaceInteraction, has_subsurface))
        //.def("is_medium_transition", &Type::is_medium_transition, D(SurfaceInteraction, is_medium_transition))
        //.def("target_medium",
             //py::overload_cast<const typename Type::Vector3 &>(
                //&Type::target_medium, py::const_),
             //D(SurfaceInteraction, target_medium))
        //.def("target_medium",
             //py::overload_cast<typename Type::Type>(&Type::target_medium, py::const_),
             //D(SurfaceInteraction, target_medium, 2))
        //.def("bsdf",
             //py::overload_cast<const typename Type::RayDifferential3 &>(&Type::bsdf),
             //D(SurfaceInteraction, bsdf))
        //.def("bsdf",
             //py::overload_cast<>(&Type::bsdf, py::const_),
             //D(SurfaceInteraction, bsdf))
        //.def("Le", &Type::Le, D(SurfaceInteraction, Le))
        //.def("Lo_sub", &Type::Lo_sub, D(SurfaceInteraction, Lo_sub),
             //"scene"_a, "sampler"_a, "d"_a, "depth"_a=0)
        //.def("adjust_time", &Type::adjust_time, D(SurfaceInteraction, adjust_time))
        //.def("normal_derivative", &Type::normal_derivative,
             //D(SurfaceInteraction, normal_derivative),
             //"dndu"_a, "dndv"_a, "shading_frame"_a=typename Type::Mask(true))

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
        .def_readwrite("color", &Type::color, D(SurfaceInteraction, color))
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
        .def(py::init<>(), D(SurfaceInteraction, SurfaceInteraction))
        .def("compute_partials", &SurfaceInteraction3f::compute_partials,
             D(SurfaceInteraction, compute_partials), "ray"_a);

    auto si3fx = bind_interaction<Point3f>(m, "SurfaceInteraction3fX");
    bind_slicing_operators<SurfaceInteraction3fX, SurfaceInteraction3f>(si3fx);
}
