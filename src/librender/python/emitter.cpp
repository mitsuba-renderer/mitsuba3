#include <mitsuba/render/emitter.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT_VARIANTS(Emitter) {
    MTS_PY_CLASS(Emitter, Endpoint)
        .mdef(BSDF, is_environment)
        ;
}
