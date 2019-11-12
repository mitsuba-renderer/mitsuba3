#include <mitsuba/python/python.h>
#include <mitsuba/render/emitter.h>

MTS_PY_EXPORT_VARIANTS(Emitter) {
    MTS_IMPORT_TYPES()
    MTS_IMPORT_OBJECT_TYPES()
    MTS_PY_CHECK_ALIAS(Emitter, m) {
        MTS_PY_CLASS(Emitter, typename Emitter::Base)
            .def_method(Emitter, is_environment);
    }
}
