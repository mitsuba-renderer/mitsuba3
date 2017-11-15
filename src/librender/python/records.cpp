#include <mitsuba/render/records.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/python/python.h>

template <typename Point3>
auto bind_position_sample(py::module &m, const char *name) {
    using Type = PositionSample<Point3>;
    using SurfaceInteraction = typename Type::SurfaceInteraction;

    return py::class_<Type>(m, name, D(PositionSample))
        .def(py::init<>(), "Construct an unitialized position sample")
        .def(py::init<const Type &>(), "Copy constructor")
        .def(py::init<Float>(), D(PositionSample, PositionSample))
        .def(py::init<const SurfaceInteraction &, EMeasure>(),
             "intersection"_a, "measure"_a = ESolidAngle,
             D(PositionSample, PositionSample, 2))
        .def_readwrite("p", &Type::p, D(PositionSample, p))
        .def_readwrite("time", &Type::time, D(PositionSample, time))
        .def_readwrite("n", &Type::n, D(PositionSample, n))
        .def_readwrite("pdf", &Type::pdf, D(PositionSample, pdf))
        .def_readwrite("measure", &Type::measure, D(PositionSample, measure))
        .def_readwrite("uv", &Type::uv, D(PositionSample, uv))
        .def_readwrite("object", &Type::object, D(PositionSample, object))
        .def("__repr__", [](const Type &ps) {
            std::ostringstream oss;
            oss << ps;
            return oss.str();
        });
}

// -----------------------------------------------------------------------------

template <typename Vector3> auto bind_direction_sample(py::module &m, const char *name) {
    using Type = DirectionSample<Vector3>;
    using SurfaceInteraction = typename Type::SurfaceInteraction;

    return py::class_<Type>(m, name, D(DirectionSample))
        .def(py::init<>(), "Construct an unitialized direction sample")
        .def(py::init<const Type &>(), "Copy constructor")
        .def(py::init<const Vector3 &, EMeasure>(), "d"_a,
             "measure"_a = ESolidAngle, D(DirectionSample, DirectionSample))
        .def(py::init<const SurfaceInteraction &, EMeasure>(),
             "intersection"_a, "measure"_a = ESolidAngle,
             D(DirectionSample, DirectionSample, 2))
        .def_readwrite("d", &Type::d, D(DirectionSample, d))
        .def_readwrite("pdf", &Type::pdf, D(DirectionSample, pdf))
        .def_readwrite("measure", &Type::measure, D(DirectionSample, measure))
        .def("__repr__", [](const Type &record) {
            std::ostringstream oss;
            oss << record;
            return oss.str();
        });
}

// -----------------------------------------------------------------------------

template <typename Point3, typename Base>
auto bind_direct_sample(py::module &m, const char *name) {
    using Type = DirectSample<Point3>;

    return py::class_<Type, Base>(m, name, D(DirectSample))
        .def(py::init<>(), "Construct an unitialized direct sample")
        .def(py::init<const Type &>(), "Copy constructor")
        .def(py::init<const Point3f &, Float>(),
             D(DirectSample, DirectSample))
        .def(py::init<const SurfaceInteraction3f &>(),
             D(DirectSample, DirectSample, 2))
        //.def(py::init<const MediumInteraction3f &>(), D(DirectSample,
        // DirectSample, 3))
        .def(py::init<const Ray3f &, const SurfaceInteraction3f, EMeasure>(),
             "ray"_a, "intersection"_a, "measure"_a = ESolidAngle,
             D(DirectSample, DirectSample, 4))
        .def_readwrite("ref_p", &Type::ref_p, D(DirectSample, ref_p))
        .def_readwrite("ref_n", &Type::ref_n, D(DirectSample, ref_n))
        .def_readwrite("d", &Type::d, D(DirectSample, d))
        .def_readwrite("dist", &Type::dist, D(DirectSample, dist))
        .def("__repr__", [](const Type &record) {
            std::ostringstream oss;
            oss << record;
            return oss.str();
        });
}

// -----------------------------------------------------------------------------

template <typename Point3>
auto bind_radiance_record(py::module &m, const char *name) {
    using Type = RadianceRecord<Point3>;

    return py::class_<Type>(m, name, D(RadianceRecord))
        .def(py::init<>(), D(RadianceRecord, RadianceRecord))
        .def(py::init<const Scene *, Sampler *>(),
             D(RadianceRecord, RadianceRecord, 2), "scene"_a, "sampler"_a)
        .def(py::init<const Type &>(),
             D(RadianceRecord, RadianceRecord, 3),  "r_rec"_a)
        .def("new_query", &Type::new_query, "medium"_a,
             D(RadianceRecord, new_query))
        .def("recursive_query", &Type::recursive_query, "parent"_a,
             D(RadianceRecord, recursive_query))
        .def("next_sample_1d", &Type::next_sample_1d,
             D(RadianceRecord, next_sample_1d))
        .def("next_sample_2d", &Type::next_sample_2d,
             D(RadianceRecord, next_sample_2d))
        .def("__repr__", [](const Type &record) {
            std::ostringstream oss;
            oss << record;
            return oss.str();
        })
        .def_readwrite(  "scene",   &Type::scene,   D(RadianceRecord, scene))
        .def_readwrite("sampler", &Type::sampler, D(RadianceRecord, sampler))
        .def_readwrite( "medium",  &Type::medium,  D(RadianceRecord, medium))
        .def_readwrite(  "depth",   &Type::depth,   D(RadianceRecord, depth))
        .def_readwrite(    "its",     &Type::its,     D(RadianceRecord, its))
        .def_readwrite(  "alpha",   &Type::alpha,   D(RadianceRecord, alpha))
        .def_readwrite(   "dist",    &Type::dist,    D(RadianceRecord, dist))
        ;
}

// -----------------------------------------------------------------------------

MTS_PY_EXPORT(SamplingRecords) {
    bind_position_sample<Point3f>(m, "PositionSample3f");
    auto ps3fx = bind_position_sample<Point3fX>(m, "PositionSample3fX");
    bind_slicing_operators<PositionSample3fX, PositionSample3f>(ps3fx);

    bind_direction_sample<Vector3f>(m, "DirectionSample3f");
    auto ds3fx = bind_direction_sample<Vector3fX>(m, "DirectionSample3fX");
    bind_slicing_operators<DirectionSample3fX, DirectionSample3f>(ds3fx);

    bind_direct_sample<Point3f, PositionSample3f>(m, "DirectSample3f");
    auto dds3fx = bind_direct_sample<Point3fX, PositionSample3fX>(m, "DirectSample3fX");
    bind_slicing_operators<DirectSample3fX, DirectSample3f>(dds3fx);

    bind_radiance_record<Point3f>(m, "RadianceRecord3f")
        // Needs to be handled separately so that we can use vectorize_wrapper.
        .def("ray_intersect", &RadianceRecord3f::ray_intersect,
             "ray"_a, "active"_a, D(RadianceRecord, ray_intersect));
    auto rr3fx = bind_radiance_record<Point3fX>(m, "RadianceRecord3fX")
        .def("ray_intersect", enoki::vectorize_wrapper(&RadianceRecord3fP::ray_intersect),
             "ray"_a, "active"_a, D(RadianceRecord, ray_intersect));
    bind_slicing_operators<RadianceRecord3fX, RadianceRecord3f>(rr3fx);
}
