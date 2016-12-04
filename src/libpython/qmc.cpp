#include <mitsuba/core/qmc.h>
#include "python.h"

using namespace qmc;

MTS_PY_EXPORT(qmc) {
    MTS_PY_IMPORT_MODULE(qmc, "mitsuba.core.qmc");

    qmc.def("radicalInverse", (Float(*)(size_t, uint64_t)) & radicalInverse,
            DM(qmc, radicalInverse));

    qmc.def("radicalInverse",
            (FloatPacket(*)(size_t, UInt64Packet)) & radicalInverse,
            DM(qmc, radicalInverse, 2));
}
