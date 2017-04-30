#include <mitsuba/core/vector.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(vector) {
    m.def("coordinate_system", &coordinate_system<Vector3f>,
          "n"_a, D(coordinate_system));

    m.def("coordinate_system",
          vectorize_wrapper(&coordinate_system<Vector3fP>),
          "n"_a);
}
