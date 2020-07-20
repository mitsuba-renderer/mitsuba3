#include <mitsuba/core/vector.h>
#include <mitsuba/core/fwd.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(vector) {
    MTS_PY_IMPORT_TYPES()
    m.def("coordinate_system",
          vectorize(&coordinate_system<Vector<Float, 3>>),
          "n"_a, D(coordinate_system));
}
