#include <mitsuba/render/microflake.h>
#include <mitsuba/python/python.h>

#include <nanobind/stl/string.h>
#include <nanobind/stl/list.h>

MI_PY_EXPORT(MicroflakeDistribution) {
    MI_PY_IMPORT_TYPES()

    using SGGXParams = SGGXPhaseFunctionParams<Float>;

    MI_PY_CHECK_ALIAS(SGGXParams, "SGGXPhaseFunctionParams") {
        auto sggx_phase = nb::class_<SGGXParams>(m, "SGGXPhaseFunctionParams")
            .def(nb::init<const dr::Array<Float, 3>&, const dr::Array<Float, 3>&>(),
                D(SGGXPhaseFunctionParams, SGGXPhaseFunctionParams))
            .def(nb::init<const SGGXParams &>(), "Copy constructor")
            .def("__init__", [](SGGXParams *t, const nb::list &l) {
                if (l.size() != 6)
                    nb::raise("Expected list of size 6!");
                SGGXParams s {
                    {nb::cast<Float>(l[0]), nb::cast<Float>(l[1]), nb::cast<Float>(l[2])},
                    {nb::cast<Float>(l[3]), nb::cast<Float>(l[4]), nb::cast<Float>(l[5])}
                };
                new (t) SGGXParams(s);
            })
            .def_field(SGGXParams, diag)
            .def_field(SGGXParams, off_diag)
            .def_repr(SGGXParams);
        MI_PY_DRJIT_STRUCT(sggx_phase, SGGXParams, diag, off_diag)

        nb::implicitly_convertible<nb::list, SGGXParams>();
    }

    m.def("sggx_sample", [](const Frame3f& sh_frame,
                            const Point2f& sample,
                            const SGGXParams& s) {
        return sggx_sample<Float>(sh_frame, sample, s);
    }, "sh_frame"_a, "sample"_a, "s"_a, D(sggx_sample));

    m.def("sggx_sample", [](const Vector3f& sh_frame,
                            const Point2f& sample,
                            const SGGXParams& s) {
        return sggx_sample<Float>(sh_frame, sample, s);
    }, "sh_frame"_a, "sample"_a, "s"_a, D(sggx_sample));

    m.def("sggx_pdf", [](const Vector<Float, 3> &wm,
                         const SGGXParams &s) {
        return sggx_pdf<Float>(wm, s);
    }, "wm"_a, "s"_a, D(sggx_pdf));

    m.def("sggx_projected_area", [](const Vector<Float, 3> &wi,
                                    const SGGXParams &s) {
        return sggx_projected_area<Float>(wi, s);
    }, "wi"_a, "s"_a, D(sggx_projected_area));
}
