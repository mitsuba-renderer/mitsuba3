#include "python.h"
#include <mitsuba/core/spectrum.h>
#include <mitsuba/render/mesh.h>
#include <mitsuba/render/scene.h>

#define PY_CAST(Name) {                                                        \
        Name *temp = dynamic_cast<Name *>(o);                                  \
        if (temp)                                                              \
            return py::cast(temp);                                             \
    }

py::object py_cast(Object *o) {
    PY_CAST(Scene);
    PY_CAST(Mesh);
    PY_CAST(Shape);
    PY_CAST(ContinuousSpectrum);
    return py::none();
}

