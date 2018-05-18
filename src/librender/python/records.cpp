#include <mitsuba/render/records.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/python/python.h>

template <typename Point3>
auto bind_position_sample(py::module &m, const char *name) {
    using Type = PositionSample<Point3>;
    using SurfaceInteraction = typename Type::SurfaceInteraction;

    return py::class_<Type>(m, name, D(PositionSample))
        .def(py::init<>(), "Construct an unitialized position sample")
        .def(py::init<const Type &>(), "Copy constructor", "other"_a)
        .def(py::init<const SurfaceInteraction &>(), "si"_a,
             D(PositionSample, PositionSample))
        .def_readwrite("p", &Type::p, D(PositionSample, p))
        .def_readwrite("n", &Type::n, D(PositionSample, n))
        .def_readwrite("uv", &Type::uv, D(PositionSample, uv))
        .def_readwrite("time", &Type::time, D(PositionSample, time))
        .def_readwrite("pdf", &Type::pdf, D(PositionSample, pdf))
        .def_readwrite("delta", &Type::delta, D(PositionSample, delta))
        .def_readwrite("object", &Type::object, D(PositionSample, object))
        .def("__repr__", [](const Type &ps) {
            std::ostringstream oss;
            oss << ps;
            return oss.str();
        });
}

// -----------------------------------------------------------------------------

template <typename Point3, typename Base>
auto bind_direction_sample(py::module &m, const char *name) {
    using Type = DirectionSample<Point3>;
    using SurfaceInteraction = typename Type::SurfaceInteraction;
    using Interaction = typename Type::Interaction;

    return py::class_<Type, Base>(m, name, D(DirectionSample))
        .def(py::init<>(), "Construct an unitialized direct sample")
        .def(py::init<const Type &>(), "Copy constructor", "other"_a)
        .def(py::init<const SurfaceInteraction &, const Interaction &>(), "si"_a,
            "ref"_a, D(PositionSample, PositionSample))
        .def_readwrite("d", &Type::d, D(DirectionSample, d))
        .def_readwrite("dist", &Type::dist, D(DirectionSample, dist))
        .def("__repr__", [](const Type &record) {
            std::ostringstream oss;
            oss << record;
            return oss.str();
        });
}

// -----------------------------------------------------------------------------

template <typename Point3>
auto bind_radiance_record(py::module &m, const char *name) {
    using Type = RadianceSample<Point3>;

    return py::class_<Type>(m, name, D(RadianceSample))
        .def(py::init<>(), D(RadianceSample, RadianceSample))
        .def(py::init<const Scene *, Sampler *>(),
             D(RadianceSample, RadianceSample, 2), "scene"_a, "sampler"_a)
        // .def(py::init<const Type &>(), D(RadianceSample, RadianceSample, 3),
        //   "other"_a)
        .def("__repr__",
             [](const Type &record) {
                 std::ostringstream oss;
                 oss << record;
                 return oss.str();
             })
        .def_readwrite("scene", &Type::scene, D(RadianceSample, scene))
        .def_readwrite("sampler", &Type::sampler, D(RadianceSample, sampler))
        .def_readwrite("si", &Type::si, D(RadianceSample, si))
        .def_readwrite("alpha", &Type::alpha, D(RadianceSample, alpha));
}

// -----------------------------------------------------------------------------

MTS_PY_EXPORT(SamplingRecords) {
    bind_position_sample<Point3f>(m, "PositionSample3f");
    auto ps3fx = bind_position_sample<Point3fX>(m, "PositionSample3fX");
    bind_slicing_operators<PositionSample3fX, PositionSample3f>(ps3fx);

    bind_direction_sample<Point3f, PositionSample3f>(m, "DirectionSample3f");
    auto dds3fx = bind_direction_sample<Point3fX, PositionSample3fX>(m, "DirectionSample3fX");
    bind_slicing_operators<DirectionSample3fX, DirectionSample3f>(dds3fx);

    bind_radiance_record<Point3f>(m, "RadianceSample3f")
        // Needs to be handled separately so that we can use vectorize_wrapper.
        .def("ray_intersect", &RadianceSample3f::ray_intersect,
             "ray"_a, "active"_a, D(RadianceSample, ray_intersect))
        .def("next_1d", &RadianceSample3f::next_1d, D(RadianceSample, next_1d))
        .def("next_2d", &RadianceSample3f::next_2d, D(RadianceSample, next_2d))
        ;

    auto rs3fx = bind_radiance_record<Point3fX>(m, "RadianceSample3fX")
        .def("ray_intersect", enoki::vectorize_wrapper(
                &RadianceSample3fP::ray_intersect
             ), "ray"_a, "active"_a, D(RadianceSample, ray_intersect))
        .def("next_1d", enoki::vectorize_wrapper(
                &RadianceSample3fP::next_1d
             ), D(RadianceSample, next_1d))
        .def("next_2d", enoki::vectorize_wrapper(
                &RadianceSample3fP::next_2d
             ), D(RadianceSample, next_2d))
        ;

    bind_slicing_operators<RadianceSample3fX, RadianceSample3f>(rs3fx);
}
