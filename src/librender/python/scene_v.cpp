#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/integrator.h>
#include <mitsuba/render/mesh.h>
#include <mitsuba/render/scatteringintegrator.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/sensor.h>

#include <mitsuba/python/python.h>

#if !defined(MTS_ENABLE_EMBREE)
#  include <mitsuba/render/kdtree.h>
#endif

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
        .def("render",
            [&](Scene *scene, uint32_t seed, uint32_t sensor_idx, uint32_t spp) {
                py::gil_scoped_release release;

#if MTS_HANDLE_SIGINT
                // Install new signal handler
                sigint_handler = [scene]() {
                    scene->integrator()->cancel();
                };

                sigint_handler_prev = signal(SIGINT, [](int) {
                    Log(Warn, "Received interrupt signal, winding down..");
                    if (sigint_handler) {
                        sigint_handler();
                        sigint_handler = std::function<void()>();
                        signal(SIGINT, sigint_handler_prev);
                        raise(SIGINT);
                    }
                });
#endif

                if (spp > 0)
                    scene->sensors()[sensor_idx]->sampler()->set_sample_count(spp);
                ref<Bitmap> bitmap = scene->render(seed, sensor_idx);

#if MTS_HANDLE_SIGINT
                // Restore previous signal handler
                signal(SIGINT, sigint_handler_prev);
#endif

                return bitmap;
            },
            D(Scene, render), "seed"_a = 0, "sensor_index"_a = 0, "spp"_a = 0)
        .def("ray_intersect_preliminary",
             py::overload_cast<const Ray3f &, Mask>(&Scene::ray_intersect_preliminary, py::const_),
             "ray"_a, "active"_a = true, D(Scene, ray_intersect_preliminary))
        .def("ray_intersect_preliminary",
             py::overload_cast<const Ray3f &, uint32_t, Mask>(&Scene::ray_intersect_preliminary, py::const_),
             "ray"_a, "hit_flags"_a, "active"_a = true, D(Scene, ray_intersect_preliminary))
        .def("ray_intersect",
             py::overload_cast<const Ray3f &, Mask>(&Scene::ray_intersect, py::const_),
             "ray"_a, "active"_a = true, D(Scene, ray_intersect))
        .def("ray_intersect",
             py::overload_cast<const Ray3f &, uint32_t, Mask>(&Scene::ray_intersect, py::const_),
             "ray"_a, "hit_flags"_a, "active"_a = true, D(Scene, ray_intersect))
        .def("ray_test",
             py::overload_cast<const Ray3f &, Mask>(&Scene::ray_test, py::const_),
             "ray"_a, "active"_a = true, D(Scene, ray_test))
        .def("ray_test",
             py::overload_cast<const Ray3f &, uint32_t, Mask>(&Scene::ray_test, py::const_),
             "ray"_a, "hit_flags"_a, "active"_a = true, D(Scene, ray_test))
#if !defined(MTS_ENABLE_EMBREE)
        .def("ray_intersect_naive",
            &Scene::ray_intersect_naive,
            "ray"_a, "active"_a = true)
#endif
        .def("sample_emitter",
            &Scene::sample_emitter, "sample"_a, "active"_a = true)
        .def("sample_emitter_direction",
            &Scene::sample_emitter_direction,
            "ref"_a, "sample"_a, "test_visibility"_a = true, "active"_a = true)
        .def("pdf_emitter",
            &Scene::pdf_emitter, "index"_a, "active"_a = true)
        .def("pdf_emitter_direction",
            &Scene::pdf_emitter_direction, "ref"_a, "ds"_a, "active"_a = true)
        // Accessors
        .def_method(Scene, bbox)
        .def("sensors", py::overload_cast<>(&Scene::sensors), D(Scene, sensors))
        .def("emitters", py::overload_cast<>(&Scene::emitters), D(Scene, emitters))
        .def("emitters_ek", &Scene::emitters_ek, D(Scene, emitters_ek))
        .def("shapes_ek", &Scene::shapes_ek, D(Scene, shapes_ek))
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
                if (auto tmp = dynamic_cast<ScatteringIntegrator *>(o); tmp)
                    return py::cast(tmp);
                return py::cast(o);
            },
            D(Scene, integrator))
        .def_method(Scene, shapes_grad_enabled)
        .def("__repr__", &Scene::to_string);
}
