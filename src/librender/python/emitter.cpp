#include <mitsuba/python/python.h>
#include <mitsuba/render/emitter.h>

MTS_PY_EXPORT_MODE_VARIANTS(Emitter) {
    MTS_IMPORT_TYPES()
    MTS_IMPORT_OBJECT_TYPES()
    using Base = typename Emitter::Base;
    MTS_PY_CLASS(Emitter, Base)
        .def_method(Emitter, is_environment)
        ;
}
