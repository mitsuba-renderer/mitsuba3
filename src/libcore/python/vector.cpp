#include <mitsuba/core/vector.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(vector) {
    MTS_IMPORT_CORE_TYPES()
    m.def("coordinate_system",
          vectorize<Float>(&coordinate_system<Vector<Float, 3>>),
          "n"_a, D(coordinate_system));
}
