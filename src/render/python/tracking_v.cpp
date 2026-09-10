#include <mitsuba/render/tracking.h>
#include <mitsuba/python/python.h>
#include <drjit/python.h>

MI_PY_EXPORT(TrackingState) {
    MI_PY_IMPORT_TYPES()
    using TrackingState = mitsuba::TrackingState<Float, Spectrum>;

    auto ts = nb::class_<TrackingState>(m, "TrackingState", D(TrackingState))
        .def(nb::init<>())
        .def(nb::init<const TrackingState &>(), "other"_a, "Copy constructor")
        .def_field(TrackingState, ray,                      D(TrackingState, ray))
        .def_field(TrackingState, rng,                       D(TrackingState, rng))
        .def_field(TrackingState, mei,                       D(TrackingState, mei))
        .def_field(TrackingState, target_ot,                 D(TrackingState, target_ot))
        .def_field(TrackingState, has_spectral_extinction,   D(TrackingState, has_spectral_extinction))
        .def_field(TrackingState, throughput,                D(TrackingState, throughput));

    MI_PY_DRJIT_STRUCT(ts, TrackingState, ray, rng, mei, target_ot,
                        has_spectral_extinction, throughput);
}
