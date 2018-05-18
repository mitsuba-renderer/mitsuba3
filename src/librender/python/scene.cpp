#include <mitsuba/core/properties.h>
#include <mitsuba/python/python.h>
#include <mitsuba/render/integrator.h>
#include <mitsuba/render/kdtree.h>
#include <mitsuba/render/mesh.h>
#include <mitsuba/render/sampler.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/sensor.h>

using Index = Shape::Index;

MTS_PY_EXPORT(ShapeKDTree) {
    MTS_PY_CLASS(ShapeKDTree, Object)
        .def(py::init<const Properties &>(), D(ShapeKDTree, ShapeKDTree))
        .mdef(ShapeKDTree, add_shape)
        .mdef(ShapeKDTree, primitive_count)
        .mdef(ShapeKDTree, shape_count)
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
        .mdef(ShapeKDTree, build);
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
             ),
             "ray"_a, "active"_a = true)

        // Shadow rays
        .def("ray_test",
             py::overload_cast<const Ray3f &, bool>(&Scene::ray_test, py::const_),
             "ray"_a, "unused"_a = true, D(Scene, ray_test))
        .def("ray_test",
             enoki::vectorize_wrapper(
                py::overload_cast<const Ray3fP &, MaskP>(&Scene::ray_test, py::const_)
             ),
             "ray"_a, "active"_a = true)

        // Full intersection (brute force, for testing)
        .def("ray_intersect_naive",
             py::overload_cast<const Ray3f &, bool>(&Scene::ray_intersect_naive, py::const_),
             "ray"_a, "unused"_a = true, D(Scene, ray_intersect_naive))
        .def("ray_intersect_naive",
             enoki::vectorize_wrapper(
                py::overload_cast<const Ray3fP &, MaskP>(&Scene::ray_intersect_naive, py::const_)
             ),
             "ray"_a, "active"_a = true)
#if 0

        // Sampling
        .def("sample_emitter_direct",
             py::overload_cast<DirectSample3f &, const Point2f &, bool>(
                &Scene::sample_emitter_direct, py::const_),
             "d_rec"_a, "sample"_a, "test_visibility"_a = true,
             D(Scene, sample_emitter_direct))
        .mdef(Scene, pdf_emitter_direct, "d_rec"_a)
        .mdef(Scene, sample_attenuated_emitter_direct, "d_rec"_a, "its"_a,
              "medium"_a, "interactions"_a, "sample"_a, "sampler"_a = nullptr)
#endif

        // Accessors
        .def("kdtree",     py::overload_cast<>(&Scene::kdtree),     D(Scene, kdtree))
        .def("sensor",     py::overload_cast<>(&Scene::sensor),     D(Scene, sensor))
        .def("film",       py::overload_cast<>(&Scene::film),       D(Scene, film))
        .def("sampler",    py::overload_cast<>(&Scene::sampler),    D(Scene, sampler))
        .def("integrator", py::overload_cast<>(&Scene::integrator), D(Scene, integrator));
}
