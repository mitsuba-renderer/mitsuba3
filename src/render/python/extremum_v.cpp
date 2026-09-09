#include <mitsuba/core/properties.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/extremum.h>
#include <mitsuba/render/extremum_segment.h>
#include <mitsuba/render/tracking.h>
#include <mitsuba/python/python.h>
#include <nanobind/trampoline.h>
#include <nanobind/stl/function.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/tuple.h>
#include <nanobind/stl/vector.h>
#include <drjit/python.h>


MI_PY_EXPORT(ExtremumSegment) {
    MI_PY_IMPORT_TYPES()

    auto es = nb::class_<ExtremumSegment>(m, "ExtremumSegment", D(ExtremumSegment))
        .def(nb::init<>())
        .def(nb::init<const ExtremumSegment &>(), "other"_a, "Copy constructor")
        .def(nb::init<Float, Float, Float, Float>(),
                 D(ExtremumSegment, ExtremumSegment, 2),
                 "mint"_a, "maxt"_a, "minorant"_a, "majorant"_a)
        .def(nb::init<Float, Float, Vector2f>(),
                 D(ExtremumSegment, ExtremumSegment, 2),
                 "mint"_a, "maxt"_a, "value"_a)
        .def("valid",        &ExtremumSegment::valid,     D(ExtremumSegment, valid))
        .def("reset",        &ExtremumSegment::reset,     D(ExtremumSegment, reset))
        .def("zero_",        &ExtremumSegment::zero_,     "size"_a = 1)
        .def("zero_",        &ExtremumSegment::zero_,     D(ExtremumSegment, zero))
        .def("minorant",     &ExtremumSegment::minorant,  D(ExtremumSegment, minorant))
        .def("majorant",     &ExtremumSegment::majorant,  D(ExtremumSegment, majorant))
        .def_field(ExtremumSegment, mint,   D(ExtremumSegment, mint))
        .def_field(ExtremumSegment, maxt,   D(ExtremumSegment, maxt))
        .def_field(ExtremumSegment, value,  D(ExtremumSegment, value))
        .def_repr(ExtremumSegment);

    MI_PY_DRJIT_STRUCT(es, ExtremumSegment, mint, maxt, value);
}

/// Trampoline for derived types implemented in Python
MI_VARIANT class PyExtremum : public Extremum<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Extremum, Volume)
    NB_TRAMPOLINE(Extremum);

    PyExtremum(const Properties &props) : Extremum(props) {}

    void build(const Volume * volume) override {
        NB_OVERRIDE_PURE(build, volume);
    }

    std::string to_string() const override {
        NB_OVERRIDE(to_string);
    }

    void traverse(TraversalCallback *cb) override {
        NB_OVERRIDE(traverse, cb);
    }

    void parameters_changed(const std::vector<std::string> &keys) override {
        NB_OVERRIDE(parameters_changed, keys);
    }
};

template <typename Ptr, typename Cls> void bind_extremum_generic(Cls &cls) {
    MI_PY_IMPORT_TYPES(Extremum, Medium)
    using TrackingStateType    = TrackingState<Float, Spectrum>;
    using TrackingFunctionType = TrackingFunction<Float, Spectrum>;

    cls.def("traverse_extremum",
            [](Ptr ptr, const Ray3f &ray, Float mint, Float maxt,
               UInt32 channel, TrackingStateType state,
               const TrackingFunctionType &func, Mask active) {
                return ptr->traverse_extremum(ray, mint, maxt, channel,
                                               state, func, active);
            },
            "ray"_a, "mint"_a, "maxt"_a, "channel"_a, "state"_a, "func"_a,
            "active"_a = true,
            D(Extremum, traverse_extremum));


    // Test utility: deterministic delta tracking driven by a fixed target
    // optical thickness.
    cls.def("sample_test",
            [](Ptr ptr, const Ray3f &ray, Float mint, Float maxt,
               Float target_ot, UInt32 channel, Mask active) {
                using TrackingStateType = TrackingState<Float, Spectrum>;

                TrackingStateType state = dr::zeros<TrackingStateType>();
                state.ray       = ray;
                state.target_ot = target_ot;
                state.mei       = dr::zeros<MediumInteraction3f>();

                state = ptr->traverse_extremum(
                    ray, mint, maxt, channel, state,
                    [](const ExtremumSegment &segment, TrackingStateType *state,
                       const UInt32 &, Mask active) {
                        Float mint = dr::select(
                            state->mei.is_valid(),
                            dr::maximum(segment.mint, state->mei.t),
                            segment.mint);
                        Float segment_ot =
                            (segment.maxt - mint) * segment.majorant();
                        Mask sampled = (state->target_ot < segment_ot) && active;

                        Float maxt = dr::select(
                            sampled,
                            mint + state->target_ot /
                                       dr::maximum(segment.majorant(),
                                                   dr::Epsilon<Float>),
                            segment.maxt);

                        dr::masked(state->mei.t, sampled)  = maxt;
                        dr::masked(state->mei.t, !sampled) = dr::Infinity<Float>;
                        dr::masked(state->target_ot, !sampled && active) -=
                            segment_ot;

                        return std::pair<Mask, Mask>(/*advance=*/!sampled,
                                                     active && !sampled);
                    },
                    active);

                return std::make_tuple(state.mei.t, state.target_ot);
            },
            "ray"_a, "mint"_a, "maxt"_a, "target_ot"_a, "channel"_a = 0u,
            "active"_a = true,
            "Deterministic delta-tracking test utility. Traverses the extremum "
            "structure's segments, until an interaction is sampled based on "
            "`target_ot`. Returns (distance, leftover_ot); `distance` is "
            "infinite if `target_ot` is not reached before `maxt`.");
}


MI_PY_EXPORT(Extremum) {
    MI_PY_IMPORT_TYPES(Extremum, ExtremumPtr)
    using PyExtremum = PyExtremum<Float, Spectrum>;
    using Properties = mitsuba::Properties;

    auto extremum = MI_PY_TRAMPOLINE_CLASS(PyExtremum, Extremum, Object)
        .def(nb::init<const Properties &>(), "props"_a)
        .def("__repr__", &Extremum::to_string)
        .def("set_bbox", &Extremum::set_bbox,
             "bbox"_a, D(Extremum, set_bbox))
        .def("set_scale", &Extremum::set_scale,
             "scale"_a, D(Extremum, set_scale))
        .def("update_extremum", &Extremum::update_extremum,
             "bbox"_a, "volume"_a, "scale"_a = nb::none(),
             D(Extremum, update_extremum))
        .def("build", &Extremum::build,
             "volume"_a, D(Extremum, build))
        .def("bbox", &Extremum::bbox, D(Extremum, bbox));

    drjit::bind_traverse(extremum);

    bind_extremum_generic<Extremum *>(extremum);

    if constexpr (dr::is_array_v<ExtremumPtr>) {
        dr::ArrayBinding b;
        auto extremum_ptr = dr::bind_array_t<ExtremumPtr>(b, m, "ExtremumPtr");
        bind_extremum_generic<ExtremumPtr>(extremum_ptr);
        extremum_ptr.freeze();
    }
}
