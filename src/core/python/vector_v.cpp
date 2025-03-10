#include <mitsuba/core/vector.h>
#include <mitsuba/core/fwd.h>
#include <mitsuba/python/python.h>

#include <nanobind/stl/pair.h>

MI_PY_EXPORT(vector) {
    MI_PY_IMPORT_TYPES()
    m.def("coordinate_system", &coordinate_system<Vector<Float, 3>>, "n"_a,
          D(coordinate_system));
    m.def("sph_to_dir", &sph_to_dir<Float>, "theta"_a, "phi"_a,
          D(sph_to_dir));
    m.def("dir_to_sph", &dir_to_sph<Float>, "v"_a,
          D(dir_to_sph));
}
