#include <mitsuba/python/python.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/render/fwd.h>

#if defined(MTS_ENABLE_AUTODIFF)
#  include <enoki/cuda.h>
#  include <enoki/autodiff.h>
#endif

MTS_PY_EXPORT(autodiff) {
    ENOKI_MARK_USED(m);

#if defined(MTS_ENABLE_AUTODIFF)
    py::module::import("enoki");
    pybind11_type_alias<Array<FloatD, 2>, Vector2fD>();
    pybind11_type_alias<Array<FloatD, 2>, Point2fD>();

    pybind11_type_alias<Array<FloatD, 3>, Vector3fD>();
    pybind11_type_alias<Array<FloatD, 3>, Point3fD>();
    pybind11_type_alias<Array<FloatD, 3>, Normal3fD>();

    pybind11_type_alias<Array<FloatD, 4>, Vector4fD>();
    pybind11_type_alias<Array<FloatD, 4>, Point4fD>();

    pybind11_type_alias<Array<FloatD, 3>, Color3fD>();
    pybind11_type_alias<Array<FloatD, 4>, SpectrumfD>();
#endif
}
