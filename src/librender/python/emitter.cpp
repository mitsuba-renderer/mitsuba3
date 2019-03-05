#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/records.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(Emitter) {
    auto emitter = MTS_PY_CLASS(Emitter, Endpoint)
        ;
#if defined(MTS_ENABLE_AUTODIFF)
    using EmitterD = enoki::DiffArray<enoki::CUDAArray<const Emitter *>>;

    bind_array<EmitterD>(m, "EmitterD", py::module::import("enoki").attr("UInt64D"));
#endif
}
