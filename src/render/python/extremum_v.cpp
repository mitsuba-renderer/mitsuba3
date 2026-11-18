#include <mitsuba/core/properties.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/extremum.h>
#include <mitsuba/render/extremum_segment.h>
#include <mitsuba/python/python.h>
#include <nanobind/trampoline.h>
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

MI_PY_EXPORT(ExtremumState) {
    MI_PY_IMPORT_TYPES()

    auto ds = nb::class_<ExtremumState>(m, "ExtremumState",
                                           D(ExtremumState))
        .def(nb::init<>())
        .def(nb::init<const ExtremumState &>(), "other"_a, "Copy constructor")
        .def_field(ExtremumState, o,    D(ExtremumState, o))
        .def_field(ExtremumState, d,    D(ExtremumState, d))
        .def_field(ExtremumState, pi,   D(ExtremumState, pi))
        .def_field(ExtremumState, mint, D(ExtremumState, mint))
        .def_field(ExtremumState, maxt, D(ExtremumState, maxt));

    MI_PY_DRJIT_STRUCT(ds, ExtremumState, o, d, pi, mint, maxt);
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

    ExtremumState begin(const Ray3f &ray, Float mint, Float maxt,
                               Mask active) const override {
        NB_OVERRIDE_PURE(begin, ray, mint, maxt, active);
    }

    ExtremumSegment next(ExtremumState &state, const Ray3f &ray,
                              Mask active) const override {
        constexpr uint64_t nb_hash = nb::detail::str_hash("next");
        nb::detail::ticket nb_ticket(nb_trampoline, "next", nb_hash, true);
        auto [segment, next] =
            nb::cast<std::pair<ExtremumSegment, ExtremumState>>(
                nb_trampoline.base().attr(nb_ticket.key)(state, ray, active));
        dr::masked(state, active) = next;
        return segment;
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

MI_PY_EXPORT(Extremum) {
    MI_PY_IMPORT_TYPES(Extremum)
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
        .def("bbox", &Extremum::bbox, D(Extremum, bbox))
        .def("begin", &Extremum::begin,
             "ray"_a, "mint"_a, "maxt"_a, "active"_a = true,
             D(Extremum, begin))
        .def("next",
             [](const Extremum *ptr, ExtremumState state,
                const Ray3f &ray, Mask active) {
                 ExtremumSegment segment = ptr->next(state, ray, active);
                 return std::make_pair(segment, state);
             },
             "state"_a, "ray"_a, "active"_a = true,
             "Test utility: ``next`` on a copy of ``state``. Returns "
             "(segment, advanced state).");

    drjit::bind_traverse(extremum);
}
