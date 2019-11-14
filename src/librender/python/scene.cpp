#include <mitsuba/core/properties.h>
#include <mitsuba/python/python.h>
#include <mitsuba/render/integrator.h>
#include <mitsuba/render/mesh.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/sensor.h>

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
        .def("ray_intersect",
             vectorize<Float>(&Scene::ray_intersect),
             "ray"_a, "active"_a = true, D(Scene, ray_intersect))
        .def("ray_test",
             vectorize<Float>(&Scene::ray_test),
             "ray"_a, "active"_a = true)
#if !defined(MTS_USE_EMBREE)
        .def("ray_intersect_naive",
             vectorize<Float>(&Scene::ray_intersect_naive),
             "ray"_a, "active"_a = true)
#endif
        .def("sample_emitter_direction",
             vectorize<Float>(&Scene::sample_emitter_direction),
             "ref"_a, "sample"_a, "test_visibility"_a = true, "mask"_a = true)
        .def("pdf_emitter_direction",
             vectorize<Float>(&Scene::pdf_emitter_direction),
             "ref"_a, "ds"_a, "active"_a = true)
        // Accessors
        .def("bbox", &Scene::bbox, D(Scene, bbox))
        .def("sensor", py::overload_cast<>(&Scene::sensor), D(Scene, sensor))
        .def("sensors", py::overload_cast<>(&Scene::sensors), D(Scene, sensors))
        .def_method(Scene, set_current_sensor, "index"_a)
        .def("emitters", py::overload_cast<>(&Scene::emitters), D(Scene, emitters))
        .def_method(Scene, environment)
        .def("film", py::overload_cast<>(&Scene::film), D(Scene, film))
        .def("shapes", py::overload_cast<>(&Scene::shapes), D(Scene, shapes))
        .def("integrator",
             [](Scene &scene) {
                 Integrator *o = scene.integrator();
                 PY_CAST_VARIANTS(MonteCarloIntegrator);
                 PY_CAST_VARIANTS(SamplingIntegrator);
                 return py::cast(o);
             },
             D(Scene, integrator))
        .def("__repr__", &Scene::to_string);
}
