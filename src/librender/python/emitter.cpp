#include <mitsuba/render/emitter.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT_VARIANTS(Emitter) {
    using Base = typename Emitter::Base;
    MTS_PY_CLASS(Emitter, Base)
        .mdef(Emitter, is_environment)
        ;
}
