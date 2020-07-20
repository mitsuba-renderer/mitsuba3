#include <mitsuba/core/properties.h>
#include <mitsuba/render/integrator.h>
#include <mitsuba/render/mesh.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/kdtree.h>
#include <mitsuba/render/sensor.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(ShapeKDTree) {
    MTS_PY_IMPORT_TYPES(ShapeKDTree, Shape, Mesh)
#if !defined(MTS_ENABLE_EMBREE)
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
            if (shape->is_mesh())
                return py::cast(static_cast<Mesh *>(s.shape(i)));
            else
                return py::cast(s.shape(i));
        })
        .def("__len__", &ShapeKDTree::primitive_count)
        .def("bbox", [] (ShapeKDTree &s) { return s.bbox(); })
        .def_method(ShapeKDTree, build)
        .def_method(ShapeKDTree, build);
#else
    ENOKI_MARK_USED(m);
#endif
}

MTS_PY_EXPORT(Scene) {
    MTS_PY_IMPORT_TYPES(Scene, Integrator, SamplingIntegrator, MonteCarloIntegrator, Sensor)
    MTS_PY_CLASS(Scene, Object)
        .def(py::init<const Properties>())
        .def("ray_intersect_preliminary",
             vectorize(&Scene::ray_intersect_preliminary),
             "ray"_a, "active"_a = true, D(Scene, ray_intersect_preliminary))
        .def("ray_intersect",
             vectorize(py::overload_cast<const Ray3f &, Mask>(&Scene::ray_intersect, py::const_)),
             "ray"_a, "active"_a = true, D(Scene, ray_intersect))
        .def("ray_intersect",
             vectorize(py::overload_cast<const Ray3f &, HitComputeFlags, Mask>(&Scene::ray_intersect, py::const_)),
             "ray"_a, "flags"_a, "active"_a = true, D(Scene, ray_intersect))
        .def("ray_test",
            vectorize(&Scene::ray_test),
            "ray"_a, "active"_a = true)
#if !defined(MTS_ENABLE_EMBREE)
        .def("ray_intersect_naive",
            vectorize(&Scene::ray_intersect_naive),
            "ray"_a, "active"_a = true)
#endif
        .def("sample_emitter_direction",
            vectorize(&Scene::sample_emitter_direction),
            "ref"_a, "sample"_a, "test_visibility"_a = true, "mask"_a = true)
        .def("pdf_emitter_direction",
            vectorize(&Scene::pdf_emitter_direction),
            "ref"_a, "ds"_a, "active"_a = true)
        // Accessors
        .def_method(Scene, bbox)
        .def("sensors", py::overload_cast<>(&Scene::sensors), D(Scene, sensors))
        .def("emitters", py::overload_cast<>(&Scene::emitters), D(Scene, emitters))
        .def_method(Scene, environment)
        .def("shapes", [](const Scene &scene) {
            py::list result;
            for (const Shape *s : scene.shapes()) {
                const Mesh *m = dynamic_cast<const Mesh *>(s);
                if (m)
                    result.append(py::cast(m));
                else
                    result.append(py::cast(s));
            }
            return result;
        })
        .def("integrator",
            [](Scene &scene) {
                Integrator *o = scene.integrator();
                if (auto tmp = dynamic_cast<MonteCarloIntegrator *>(o); tmp)
                    return py::cast(tmp);
                if (auto tmp = dynamic_cast<SamplingIntegrator *>(o); tmp)
                    return py::cast(tmp);
                return py::cast(o);
            },
            D(Scene, integrator))
        .def_method(Scene, shapes_grad_enabled)
        .def("__repr__", &Scene::to_string);
}