#include <mitsuba/core/vector.h>
#include <mitsuba/core/fwd.h>
#include <mitsuba/python/python.h>

#include <nanobind/stl/pair.h>

MI_PY_EXPORT(vector) {
    MI_PY_IMPORT_TYPES()
    m.def("coordinate_system", &coordinate_system<Vector<Float, 3>>, "n"_a,
          D(coordinate_system));
}
