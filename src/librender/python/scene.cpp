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
        .mdef(ShapeKDTree, build)
        .def("ray_intersect_havran",       &ShapeKDTree::ray_intersect_havran<false>)
        .def("ray_intersect_dummy_scalar", &ShapeKDTree::ray_intersect_dummy<false, Ray3f>)
        .def("ray_intersect_dummy_packet", vectorize_wrapper(&ShapeKDTree::ray_intersect_dummy<false, Ray3fP>))
        .def("ray_intersect_pbrt_scalar",  &ShapeKDTree::ray_intersect_pbrt<false, Ray3f>)
        .def("ray_intersect_pbrt_packet",  vectorize_wrapper(&ShapeKDTree::ray_intersect_pbrt<false, Ray3fP>));
}

MTS_PY_EXPORT(Scene) {
    MTS_PY_CLASS(Scene, Object)
        .mdef(Scene, sample_emitter_direct, "d_rec"_a, "sample"_a,
              "test_visibility"_a = true)
        .mdef(Scene, pdf_emitter_direct, "d_rec"_a)
        .mdef(Scene, sample_attenuated_emitter_direct, "d_rec"_a, "its"_a,
              "medium"_a, "interactions"_a, "sample"_a, "sampler"_a = nullptr)
        .def("eval_environment", &Scene::eval_environment<RayDifferential3f>,
             D(Scene, eval_environment), "ray"_a)
        .def("eval_environment", enoki::vectorize_wrapper(
                &Scene::eval_environment<RayDifferential3fP>),
             D(Scene, eval_environment), "ray"_a)

        .def("ray_intersect",
             py::overload_cast<const Ray3f &, const Float &, const Float &,
                              SurfaceInteraction3f &, const bool &>(
                &Scene::ray_intersect<Ray3f>, py::const_),
             "ray"_a, "mint"_a, "maxt"_a, "its"_a, "active"_a,
             D(Scene, ray_intersect))
        //.def("ray_intersect", enoki::vectorize_wrapper(
        //     py::overload_cast<const Ray3fP &, const FloatP &, const FloatP &,
        //                       SurfaceInteraction3fP &, const mask_t<FloatP> &> (
        //        &Scene::ray_intersect<Ray3fP>, py::const_),
        //    "ray"_a, "mint"_a, "maxt"_a, "its"_a, "active"_a,
        //     D(Scene, ray_intersect))
        .def("ray_intersect",
            py::overload_cast<const Ray3f &, const Float &, const Float &>(
                &Scene::ray_intersect<Ray3f>, py::const_),
             "ray"_a, "mint"_a, "maxt"_a, D(Scene, ray_intersect, 2))
        .def("ray_intersect", enoki::vectorize_wrapper(
            py::overload_cast<const Ray3fP &, const FloatP &, const FloatP &> (
                &Scene::ray_intersect<Ray3fP>, py::const_)),
             "ray"_a, "mint"_a, "maxt"_a, D(Scene, ray_intersect, 2))

        // Sampling

        // Accessors
        .def("kdtree",  py::overload_cast<>(&Scene::kdtree),  D(Scene, kdtree))
        .def("sensor",  py::overload_cast<>(&Scene::sensor),  D(Scene, sensor))
        .def("film",    py::overload_cast<>(&Scene::film),    D(Scene, film))
        .def("sampler", py::overload_cast<>(&Scene::sampler), D(Scene, sampler))
        .def("integrator", py::overload_cast<>(&Scene::integrator),
             D(Scene, integrator))
        .def("environment_emitter",
             py::overload_cast<>(&Scene::environment_emitter),
             D(Scene, environment_emitter))
        .def("__repr__", &Scene::to_string)
        ;
}
