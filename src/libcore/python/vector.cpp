#include <mitsuba/core/vector.h>
#include <mitsuba/python/python.h>
#include <mitsuba/core/fwd.h>

MTS_PY_EXPORT(vector) {
    MTS_IMPORT_CORE_TYPES()
    m.def("coordinate_system",
          vectorize<Float>(&coordinate_system<Vector<Float, 3>>),
          "n"_a, D(coordinate_system));

    if constexpr (is_cuda_array_v<Float>) {
        py::module::import("enoki");
        pybind11_type_alias<Array<Float, 2>, Vector2f>();
        pybind11_type_alias<Array<Float, 2>, Point2f>();

        pybind11_type_alias<Array<Float, 3>, Vector3f>();
        pybind11_type_alias<Array<Float, 3>, Point3f>();
        pybind11_type_alias<Array<Float, 3>, Normal3f>();

        pybind11_type_alias<Array<Float, 4>, Vector4f>();
        pybind11_type_alias<Array<Float, 4>, Point4f>();
    }
}
