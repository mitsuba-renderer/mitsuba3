#include <mitsuba/core/properties.h>
#include <mitsuba/python/python.h>
#include <mitsuba/render/integrator.h>
#include <mitsuba/render/kdtree.h>
#include <mitsuba/render/mesh.h>
#include <mitsuba/render/sampler.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/sensor.h>

using Index = Shape::Index;

extern py::object py_cast(Object *o);

MTS_PY_EXPORT(ShapeKDTree) {
#if !defined(MTS_USE_EMBREE)
    MTS_PY_CLASS(ShapeKDTree, Object)
        .def(py::init<const Properties &>(), D(ShapeKDTree, ShapeKDTree))
        .def_method(ShapeKDTree, add_shape)
        .def_method(ShapeKDTree, primitive_count)
        .def_method(ShapeKDTree, shape_count)
        .def("shape", (Shape *(ShapeKDTree::*)(size_t)) &ShapeKDTree::shape, D(ShapeKDTree, shape))
        .def("__getitem__", [](ShapeKDTree &s, size_t i) -> py::object {
            if (i >= s.primitive_count())
                throw py::index_error();
            Shape *shape = s.shape(i);
            if (shape->class_()->derives_from(MTS_CLASS(Mesh)))
                return py::cast(static_cast<Mesh *>(s.shape(i)));
            else
                return py::cast(s.shape(i));
        })
        .def("__len__", &ShapeKDTree::primitive_count)
        .def("bbox", [] (ShapeKDTree &s) { return s.bbox(); })
        .def_method(ShapeKDTree, build);
#else
    ENOKI_MARK_USED(m);
#endif
}

MTS_PY_EXPORT(Scene) {
    MTS_PY_CLASS(Scene, Object)
        .def(py::init<const Properties>())

        // Full intersection
        .def("ray_intersect",
             py::overload_cast<const Ray3f &, bool>(&Scene::ray_intersect, py::const_),
             "ray"_a, "unused"_a = true, D(Scene, ray_intersect))
        .def("ray_intersect",
             enoki::vectorize_wrapper(
                py::overload_cast<const Ray3fP &, MaskP>(&Scene::ray_intersect, py::const_)
             ), "ray"_a, "active"_a = true)

        // Shadow rays
        .def("ray_test",
             py::overload_cast<const Ray3f &, bool>(&Scene::ray_test, py::const_),
             "ray"_a, "unused"_a = true, D(Scene, ray_test))
        .def("ray_test",
             enoki::vectorize_wrapper(
                py::overload_cast<const Ray3fP &, MaskP>(&Scene::ray_test, py::const_)
             ), "ray"_a, "active"_a = true)

        /// Emitter sampling
        .def("sample_emitter_direction",
             py::overload_cast<const Interaction3f &, const Point2f &, bool, bool>(
                &Scene::sample_emitter_direction, py::const_),
             "ref"_a, "sample"_a, "test_visibility"_a = true, "unused"_a = true,
             D(Scene, sample_emitter_direction))

        .def("sample_emitter_direction", enoki::vectorize_wrapper(
                py::overload_cast<const Interaction3fP &, const Point2fP &, bool, MaskP>(
                    &Scene::sample_emitter_direction, py::const_)),
             "ref"_a, "sample"_a, "test_visibility"_a = true, "mask"_a = true)

        .def("pdf_emitter_direction",
             py::overload_cast<const Interaction3f &, const DirectionSample3f &, bool>(
                &Scene::pdf_emitter_direction, py::const_),
             "ref"_a, "ds"_a, "unused"_a = true, D(Scene, pdf_emitter_direction))

        .def("pdf_emitter_direction", enoki::vectorize_wrapper(
                py::overload_cast<const Interaction3fP &, const DirectionSample3fP &, MaskP>(
                    &Scene::pdf_emitter_direction, py::const_)),
             "ref"_a, "ds"_a, "active"_a = true)

#if !defined(MTS_USE_EMBREE)
        // Full intersection (brute force, for testing)
        .def("ray_intersect_naive",
             py::overload_cast<const Ray3f &, bool>(&Scene::ray_intersect_naive, py::const_),
             "ray"_a, "unused"_a = true)  //, D(Scene, ray_intersect_naive))
        .def("ray_intersect_naive",
             enoki::vectorize_wrapper(
                py::overload_cast<const Ray3fP &, MaskP>(&Scene::ray_intersect_naive, py::const_)
             ),
             "ray"_a, "active"_a = true)
#endif

#if defined(MTS_ENABLE_AUTODIFF)
        .def("ray_intersect",
             py::overload_cast<const Ray3fD &, MaskD>(&Scene::ray_intersect, py::const_),
             "ray"_a, "active"_a = true)
        .def("ray_test",
             py::overload_cast<const Ray3fD &, MaskD>(&Scene::ray_test, py::const_),
             "ray"_a, "active"_a = true)

        .def("sample_emitter_direction",
                py::overload_cast<const Interaction3fD &, const Point2fD &, bool, MaskD>(
                    &Scene::sample_emitter_direction, py::const_),
             "ref"_a, "sample"_a, "test_visibility"_a = true, "mask"_a = true)

        .def("pdf_emitter_direction",
             py::overload_cast<const Interaction3fD &, const DirectionSample3fD &, MaskD>(
                &Scene::pdf_emitter_direction, py::const_),
             "ref"_a, "ds"_a, "active"_a = true, D(Scene, pdf_emitter_direction))
#endif

        // Accessors
#if !defined(MTS_USE_EMBREE)
        .def("kdtree",     py::overload_cast<>(&Scene::kdtree))  //,     D(Scene, kdtree))
#endif
        .def("bbox", &Scene::bbox, D(Scene, bbox))
        .def("sensor",     py::overload_cast<>(&Scene::sensor),     D(Scene, sensor))
        .def("sensors",     py::overload_cast<>(&Scene::sensors),     D(Scene, sensors))
        .def_method(Scene, set_current_sensor, "index"_a)
        .def("emitters",     py::overload_cast<>(&Scene::emitters),     D(Scene, emitters))
        .def_method(Scene, environment)
        .def("film",       py::overload_cast<>(&Scene::film),       D(Scene, film))
        .def("sampler",    py::overload_cast<>(&Scene::sampler),    D(Scene, sampler))
        .def("shapes",     py::overload_cast<>(&Scene::shapes),     D(Scene, shapes))
        .def("integrator", [](Scene &scene) {
#define PY_CAST(Name) {                                                        \
        Name *temp = dynamic_cast<Name *>(o);                                  \
        if (temp)                                                              \
            return py::cast(temp);                                             \
    }
            Integrator *o = scene.integrator();
            PY_CAST(PolarizedMonteCarloIntegrator);
            PY_CAST(MonteCarloIntegrator);
            PY_CAST(SamplingIntegrator);
            return py::cast(o);

#undef PY_CAST
        }, D(Scene, integrator))
        .def("__repr__", &Scene::to_string);
}
