#include <mitsuba/core/properties.h>
#include <mitsuba/python/python.h>
#include <mitsuba/render/integrator.h>
#include <mitsuba/render/kdtree.h>
#include <mitsuba/render/mesh.h>
#include <mitsuba/render/sampler.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/sensor.h>

using Index = Shape::Index;

template <typename Func>
auto provide_cache_scalar(Func f) {
    return [f](const ShapeKDTree &kdtree, const Ray3f &ray,
               const Float &mint, const Float &maxt) {
        MTS_MAKE_KD_CACHE(cache);
        return (kdtree.*f)(ray, mint, maxt, (void *)cache, true);
    };
}
template <typename Func>
auto provide_cache(Func f) {
    return [f](const ShapeKDTree &kdtree, const Ray3fX &ray, const FloatX &mint,
               const FloatX &maxt, const mask_t<FloatX> &active_) {
        MTS_MAKE_KD_CACHE(cache);
        auto n = slices(ray);
        mask_t<FloatX> mask;
        FloatX time;
        set_slices(mask, n);
        set_slices(time, n);
        // Expand if a single value is provided
        auto active = active_;
        if (slices(active) == 1) {
            set_slices(active, n);
            active = active_.coeff(0);
        }

        vectorize([&kdtree, &cache, f](auto &&m, auto &&t, auto &&r,
                                       auto &&mi, auto &&ma, auto&&a) {
            std::tie(m, t) = (kdtree.*f)(r, mi, ma, (void *)cache, a);
        }, mask, time, ray, mint, maxt, active);
        return std::make_pair(mask, time);
    };
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
