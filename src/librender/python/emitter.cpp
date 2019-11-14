#include <mitsuba/python/python.h>
#include <mitsuba/render/emitter.h>

MTS_PY_EXPORT(Emitter) {
    MTS_IMPORT_TYPES(Emitter, Endpoint)
    MTS_PY_CHECK_ALIAS(Emitter, m) {
        MTS_PY_CLASS(Emitter, Endpoint)
            .def_method(Emitter, is_environment);
    }
}
