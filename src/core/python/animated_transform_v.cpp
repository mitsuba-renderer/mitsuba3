#include <mitsuba/core/animated_transform.h>
#include <mitsuba/python/python.h>
#include <nanobind/stl/map.h>
#include <nanobind/stl/string.h>
#include <drjit/python.h>

template <typename AnimatedTransform, typename Float>
void bind_animated_transform(nb::module_ &m, const char *name) {
    MI_PY_CHECK_ALIAS(AnimatedTransform, name) {
        MI_IMPORT_CORE_TYPES()
        using ScalarTransform = typename AnimatedTransform::ScalarTransform;
        using Transform       = typename AnimatedTransform::Transform;
        auto animated_transform =
            nb::class_<AnimatedTransform, Object>(m, name, D(AnimatedTransform))
                .def(nb::init<>(), "Create an empty animated transformation")
                .def(nb::init<const ScalarTransform &>(),
                     "Initialize from a constant scalar transformation")
                .def(nb::init<const std::map<ScalarFloat, ScalarTransform> &>(),
                     "Initialize from a map of keyframes")
                .def_method(AnimatedTransform, eval, "time"_a)
                .def_method(AnimatedTransform, eval_scalar, "time"_a)
                .def_method(AnimatedTransform, is_animated)
                .def_method(AnimatedTransform, get_time_bounds)
                .def_method(AnimatedTransform, get_translation_bounds)
                .def_method(AnimatedTransform, get_spatial_bounds, "bbox"_a)
                .def_method(AnimatedTransform, has_scale)
                .def(nb::self == nb::self)
                .def(nb::self != nb::self);

        drjit::bind_traverse(animated_transform);
        nb::implicitly_convertible<ScalarTransform, AnimatedTransform>();
    }
}

MI_PY_EXPORT(AnimatedTransform) {
    MI_PY_IMPORT_TYPES()

    bind_animated_transform<AnimatedTransform<Float, Spectrum>, Float>(
        m, "AnimatedTransform4f");
}
