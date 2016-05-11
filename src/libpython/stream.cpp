#include <mitsuba/core/stream.h>
#include "python.h"

// TODO: export concrete Stream implementations
MTS_PY_EXPORT(Stream) {
    MTS_PY_CLASS(Stream, Object);
}
