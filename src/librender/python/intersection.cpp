#include <mitsuba/render/intersection.h>
#include <mitsuba/python/python.h>

using namespace pybind11::literals;

template <typename Point3>
auto bind_intersection(py::module &m, const char *name) {
    using Type = Intersection<Point3>;

    return py::class_<Type>(m, name, D(Intersection))
        // Methods
        .def("to_world", &Type::to_world, D(Intersection, to_world))
        .def("to_local", &Type::to_local, D(Intersection, to_local))
        .def("is_valid", &Type::is_valid, D(Intersection, is_valid))
        .def("is_emitter", &Type::is_emitter, D(Intersection, is_emitter))
        .def("is_sensor", &Type::is_sensor, D(Intersection, is_sensor))
        .def("has_subsurface", &Type::has_subsurface, D(Intersection, has_subsurface))
        .def("is_medium_transition", &Type::is_medium_transition, D(Intersection, is_medium_transition))
        .def("target_medium",
             py::overload_cast<const typename Type::Vector3 &>(
                &Type::target_medium, py::const_),
             D(Intersection, target_medium))
        .def("target_medium",
             py::overload_cast<typename Type::Type>(&Type::target_medium, py::const_),
             D(Intersection, target_medium, 2))
        .def("bsdf",
             py::overload_cast<const typename Type::RayDifferential3 &>(&Type::bsdf),
             D(Intersection, bsdf))
        .def("bsdf",
             py::overload_cast<>(&Type::bsdf, py::const_),
             D(Intersection, bsdf))
        .def("Le", &Type::Le, D(Intersection, Le))
        .def("Lo_sub", &Type::Lo_sub, D(Intersection, Lo_sub),
             "scene"_a, "sampler"_a, "d"_a, "depth"_a=0)
        .def("compute_partials", &Type::compute_partials,
             D(Intersection, compute_partials),
             "ray"_a, "mask"_a=typename Type::Mask(true))
        .def("adjust_time", &Type::adjust_time, D(Intersection, adjust_time))
        .def("normal_derivative", &Type::normal_derivative,
             D(Intersection, normal_derivative),
             "dndu"_a, "dndv"_a, "shading_frame"_a=typename Type::Mask(true))

        // Members
        .def_readwrite("shape", &Type::shape, D(Intersection, shape))
        .def_readwrite("t", &Type::t, D(Intersection, time))
        .def_readwrite("p", &Type::p, D(Intersection, p))
        .def_readwrite("geo_frame", &Type::geo_frame, D(Intersection, geo_frame))
        .def_readwrite("sh_frame", &Type::sh_frame, D(Intersection, sh_frame))
        .def_readwrite("uv", &Type::uv, D(Intersection, uv))
        .def_readwrite("dpdu", &Type::dpdu, D(Intersection, dpdu))
        .def_readwrite("dpdv", &Type::dpdv, D(Intersection, dpdv))
        .def_readwrite("dudx", &Type::dudx, D(Intersection, dudx))
        .def_readwrite("dudy", &Type::dudy, D(Intersection, dudy))
        .def_readwrite("dvdx", &Type::dvdx, D(Intersection, dvdx))
        .def_readwrite("dvdy", &Type::dvdy, D(Intersection, dvdy))
        .def_readwrite("time", &Type::time, D(Intersection, time))
        .def_readwrite("color", &Type::color, D(Intersection, color))
        .def_readwrite("wi", &Type::wi, D(Intersection, wi))
        .def_readwrite("has_uv_partials", &Type::has_uv_partials,
                       D(Intersection, has_uv_partials))
        .def_readwrite("prim_index", &Type::prim_index, D(Intersection, prim_index))
        .def_readwrite("instance", &Type::instance, D(Intersection, instance))
        .def("__repr__", [](const Type &it) {
            std::ostringstream oss;
            oss << it;
            return oss.str();
        });
}

MTS_PY_EXPORT(Intersection) {
    bind_intersection<Point3f>(m, "Intersection3f")
        .def(py::init<>(), D(Intersection, Intersection));

    // bind_intersection<Point3fX>(m, "Intersection3fX")
    //     .def("__init__",
    //          [](Intersection3fX &ps, size_t n) {
    //              new (&ps) Intersection3fX();
    //              enoki::dynamic_resize(ps, n);
    //              ps.shape = zero<Intersection3fX::ShapePointer>(n);
    //              ps.instance = zero<Intersection3fX::ShapePointer>(n);
    //              // TODO: is there a `constant` equivalent to `zero`
    //              ps.t = math::MaxFloat;
    //          })
    //     .def("__getitem__",
    //          [](Intersection3fX &ps, size_t i) {
    //              if (i >= dynamic_size(ps))
    //                  throw py::index_error();
    //              return Intersection3f(enoki::slice(ps, i));
    //          })
    //     .def("__setitem__", [](Intersection3fX &ps, size_t i,
    //                            const Intersection3f &ps2) {
    //         if (i >= dynamic_size(ps))
    //             throw py::index_error();
    //         enoki::slice(ps, i) = ps2;
    //     });
}
