#include <mitsuba/core/animated_transform.h>
#include <mitsuba/python/python.h>

#include <drjit/python.h>
#include <nanobind/stl/unordered_map.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

template <typename AnimatedTransform, typename Float>
void bind_animated_transform(nb::module_ &m, const char *name) {
    MI_PY_CHECK_ALIAS(AnimatedTransform, name) {
        MI_IMPORT_CORE_TYPES()
        auto animated_transform =
            nb::class_<AnimatedTransform, Object>(m, name, D(AnimatedTransform))
                .def(nb::init<const std::vector<
                         std::pair<ScalarFloat, ScalarAffineTransform4f>> &>(),
                     "Initialize from a list of keyframes")
                .def(
                    "__init__",
                    [](AnimatedTransform *t,
                       const std::unordered_map<ScalarFloat, ScalarAffineTransform4f> &keyframes) {
                        std::vector<std::pair<ScalarFloat, ScalarAffineTransform4f>> kf;
                        kf.reserve(keyframes.size());
                        for (const auto &[time, trafo] : keyframes)
                            kf.push_back({ time, trafo });
                        new (t) AnimatedTransform(kf);
                    },
                    "Initialize from a dictionary of keyframes")
                .def_method(AnimatedTransform, eval, "time"_a)
                .def_method(AnimatedTransform, eval_scalar, "time"_a);

        drjit::bind_traverse(animated_transform);
    }
}

MI_PY_EXPORT(AnimatedTransform) {
    MI_PY_IMPORT_TYPES()

    bind_animated_transform<AnimatedTransform<Float, Spectrum>, Float>(
        m, "AnimatedTransform4f");
}
