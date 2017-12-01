#include <mitsuba/core/properties.h>
#include <mitsuba/python/python.h>
#include <mitsuba/render/integrator.h>
#include <mitsuba/render/kdtree.h>
#include <mitsuba/render/mesh.h>
#include <mitsuba/render/sampler.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/sensor.h>

using Index = Shape::Index;

/// Creates the intersection cache used by the KD-tree, so that the Python
/// user doesn't need to handle it (since it is an internal data structure).
template <typename Func>
auto provide_cache_scalar(Func f) {
    return [f](const ShapeKDTree &kdtree, const Ray3f &ray,
               const Float &mint, const Float &maxt) {
        MTS_MAKE_KD_CACHE(cache);
        return (kdtree.*f)(ray, mint, maxt, (void *) cache, true);
    };
}
template <typename Func>
auto provide_cache(Func f) {
    auto provide_cache_p = [f](const ShapeKDTree *kdtree, const Ray3fP &ray,
                               const FloatP &mint, const FloatP &maxt,
                               const mask_t<FloatP> &active) {
        MTS_MAKE_KD_CACHE(cache);
        return ((*kdtree).*f)(ray, mint, maxt, (void *) cache, active);
    };
    return enoki::vectorize_wrapper(provide_cache_p);
}

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

        // These methods are exposed to Python mostly for testing purposes.
        // For fully correct intersections, use Scene::ray_intersect.
        .def("ray_intersect_dummy_scalar",
             provide_cache_scalar(&ShapeKDTree::ray_intersect_dummy<false, Ray3f>),
             "ray"_a, "mint"_a, "maxt"_a,
             D(ShapeKDTree, ray_intersect_dummy))
        .def("ray_intersect_dummy_packet",
             provide_cache(&ShapeKDTree::ray_intersect_dummy<false, Ray3fP>),
             "ray"_a, "mint"_a, "maxt"_a, "active"_a = true,
             D(ShapeKDTree, ray_intersect_dummy))

        .def("ray_intersect_havran_scalar",
             provide_cache_scalar(&ShapeKDTree::ray_intersect_havran<false>),
             "ray"_a, "mint"_a, "maxt"_a,
             D(ShapeKDTree, ray_intersect_havran))

        .def("ray_intersect_pbrt_scalar",
             provide_cache_scalar(&ShapeKDTree::ray_intersect_pbrt<false, Ray3f>),
             "ray"_a, "mint"_a, "maxt"_a,
             D(ShapeKDTree, ray_intersect_pbrt))
        .def("ray_intersect_pbrt_packet",
             provide_cache(&ShapeKDTree::ray_intersect_pbrt<false, Ray3fP>),
             "ray"_a, "mint"_a, "maxt"_a, "active"_a = true,
             D(ShapeKDTree, ray_intersect_pbrt))
        ;
}

MTS_PY_EXPORT(Scene) {
    MTS_PY_CLASS(Scene, Object)
        .def("eval_environment", &Scene::eval_environment<RayDifferential3f>,
             D(Scene, eval_environment), "ray"_a)
        .def("eval_environment", enoki::vectorize_wrapper(
                &Scene::eval_environment<RayDifferential3fP>),
             D(Scene, eval_environment), "ray"_a)

        // Full intersection
        .def("ray_intersect",
             py::overload_cast<const Ray3f &, const Float &, const Float &,
                              SurfaceInteraction3f &, const bool &>(
                &Scene::ray_intersect<Ray3f>, py::const_),
             "ray"_a, "mint"_a, "maxt"_a, "its"_a, "active"_a = true,
             D(Scene, ray_intersect))
        // TODO: why can't we use vectorize_wrapper here? (compilation error)
        .def("ray_intersect", //enoki::vectorize_wrapper(
             py::overload_cast<const Ray3fP &, const FloatP &, const FloatP &,
                               SurfaceInteraction3fP &, const mask_t<FloatP> &>(
                &Scene::ray_intersect<Ray3fP>, py::const_), //),
             "ray"_a, "mint"_a, "maxt"_a, "its"_a, "active"_a,
             D(Scene, ray_intersect))

        // Shadow rays
        .def("ray_intersect",
             py::overload_cast<const Ray3f &, const Float &, const Float &,
                              const bool &>(
                &Scene::ray_intersect<Ray3f>, py::const_),
             "ray"_a, "mint"_a, "maxt"_a, "active"_a = true,
             D(Scene, ray_intersect, 2))
        .def("ray_intersect", enoki::vectorize_wrapper(
             py::overload_cast<const Ray3fP &, const FloatP &, const FloatP &,
                              const mask_t<FloatP> &> (
                &Scene::ray_intersect<Ray3fP>, py::const_)),
             "ray"_a, "mint"_a, "maxt"_a, "activa"_a = true,
             D(Scene, ray_intersect, 2))

        // Sampling
        .def("sample_emitter_direct",
             py::overload_cast<DirectSample3f &, const Point2f &, bool>(
                &Scene::sample_emitter_direct, py::const_),
             "d_rec"_a, "sample"_a, "test_visibility"_a = true,
             D(Scene, sample_emitter_direct))
        // TODO: bind this (vectorize_wrapper probably fails because of the
        // remaining scalar parameter `bool`).
        // .def("sample_emitter_direct", enoki::vectorize_wrapper(
        //      py::overload_cast<DirectSample3fP &, const Point2fP &, bool,
        //                        const mask_t<FloatP> &>(
        //         &Scene::sample_emitter_direct, py::const_)),
        //      "d_rec"_a, "sample"_a, "test_visibility"_a, "active"_a,
        //      D(Scene, sample_emitter_direct))
        .mdef(Scene, pdf_emitter_direct, "d_rec"_a)
        .mdef(Scene, sample_attenuated_emitter_direct, "d_rec"_a, "its"_a,
              "medium"_a, "interactions"_a, "sample"_a, "sampler"_a = nullptr)

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
        ;
}
