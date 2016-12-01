#include <mitsuba/core/qmc.h>
#include "python.h"

MTS_PY_EXPORT(qmc) {
    m.def("radicalInverse", &radicalInverse);
    m.def("radicalInverseVectorized", &radicalInverseVectorized);
}
