#include <mitsuba/core/animated_transform.h>
#include <mitsuba/python/python.h>
#include <nanobind/stl/string.h>

template <typename AnimatedTransform, typename Float>
void bind_animated_transform(nb::module_ &m, const char *name) {
    MI_IMPORT_CORE_TYPES()

    using ScalarTransform = typename AnimatedTransform::ScalarTransform;
    using Transform = typename AnimatedTransform::Transform;

    auto cls = nb::class_<AnimatedTransform, Object>(m, name, D(AnimatedTransform))
        .def(nb::init<>(), "Create an empty animated transformation")
        .def(nb::init<const Transform &>(), "Initialize from a constant transformation")
        .def(nb::init<const ScalarTransform &>(), "Initialize from a constant scalar transformation")
        .def("add_keyframe", &AnimatedTransform::add_keyframe, "time"_a, "trafo"_a, D(AnimatedTransform, add_keyframe))
        .def("eval", &AnimatedTransform::eval, "time"_a, D(AnimatedTransform, eval))
        .def("eval_scalar", &AnimatedTransform::eval_scalar, "time"_a, D(AnimatedTransform, eval_scalar))
        .def("is_animated", &AnimatedTransform::is_animated, D(AnimatedTransform, is_animated))
        .def(nb::self == nb::self)
        .def(nb::self != nb::self)
        .def("__repr__", &AnimatedTransform::to_string);

    nb::implicitly_convertible<Transform, AnimatedTransform>();
    nb::implicitly_convertible<ScalarTransform, AnimatedTransform>();
}

MI_PY_EXPORT(AnimatedTransform) {
    MI_PY_IMPORT_TYPES()
    
    bind_animated_transform<AnimatedTransform<Float>, Float>(m, "AnimatedTransform4f");
}
