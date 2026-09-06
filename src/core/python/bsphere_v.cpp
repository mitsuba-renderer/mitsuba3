#include <mitsuba/core/bsphere.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/python/python.h>

#include <nanobind/stl/string.h>
#include <nanobind/stl/tuple.h>

template <typename BSphere, typename Ray> auto bind_bsphere(nb::module_ &m, const char *name) {
        using Point = typename BSphere::Point;
        using Float = typename BSphere::Float;
        using Mask  = typename BSphere::Mask;

        MI_PY_CHECK_ALIAS(BSphere, name) {
        auto bsphere = nb::class_<BSphere>(m, name, D(BoundingSphere))
            .def(nb::init<>(), D(BoundingSphere, BoundingSphere))
            .def(nb::init<Point, Float>(), D(BoundingSphere, BoundingSphere, 2))
            .def(nb::init<const BSphere &>())
            .def("empty", &BSphere::empty, D(BoundingSphere, empty))
            .def("contains",
                [](const BSphere &self, const Point &p, bool strict) {
                    return strict ? self.template contains<true>(p)
                                  : self.template contains<false>(p);
                }, D(BoundingSphere, contains), "p"_a, "strict"_a = false)
            .def("expand", &BSphere::expand, D(BoundingSphere, expand))
            .def("ray_intersect",
                [](const BSphere &self, const Ray &ray) {
                    return self.ray_intersect(ray);
                }, D(BoundingSphere, ray_intersect), "ray"_a)
            .def(nb::self == nb::self)
            .def(nb::self != nb::self)
            .def_rw("center", &BSphere::center)
            .def_rw("radius", &BSphere::radius)
            .def_repr(BSphere);

        MI_PY_DRJIT_STRUCT(bsphere, BSphere, center, radius);
        }
}

// Add a `ray_intersect` overload to an existing bounding sphere binding.
// This is used to allow intersecting a JIT array of rays against a scalar
// bounding sphere.
template <typename BSphere, typename Ray> void bind_bsphere_ray(const char *name) {
    nb::handle h = nb::type<BSphere>();
    if (!h)
        Throw("bind_bsphere_ray(): '%s' has not been registered yet!", name);
    nb::borrow<nb::class_<BSphere>>(h).def(
        "ray_intersect",
        [](const BSphere &self, const Ray &ray) { return self.ray_intersect(ray); },
        D(BoundingSphere, ray_intersect), "ray"_a);
}

MI_PY_EXPORT(BoundingSphere) {
    MI_PY_IMPORT_TYPES()
    using ScalarRay3f = Ray<ScalarPoint3f, scalar_spectrum_t<Spectrum>>;

    bind_bsphere<BoundingSphere3f, Ray3f>(m, "BoundingSphere3f");

    bind_bsphere<ScalarBoundingSphere3f, ScalarRay3f>(m, "ScalarBoundingSphere3f");

    // Also accept this variant's (possibly vectorized) ray type for ray_intersect
    if constexpr (!std::is_same_v<Ray3f, ScalarRay3f>)
        bind_bsphere_ray<ScalarBoundingSphere3f, Ray3f>("ScalarBoundingSphere3f");
}
