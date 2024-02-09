#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/rfilter.h>
#include <mitsuba/python/python.h>

template <typename ReconstructionFilter>
void bind_rfilter(nb::module_ &m, const char *name) {
    MI_PY_CHECK_ALIAS(ReconstructionFilter, name) {
        nb::class_<ReconstructionFilter, Object>(
            m, name, D(ReconstructionFilter))
            .def("border_size", &ReconstructionFilter::border_size,
                 D(ReconstructionFilter, border_size))
            .def("is_box_filter", &ReconstructionFilter::is_box_filter,
                 D(ReconstructionFilter, is_box_filter))
            .def("radius", &ReconstructionFilter::radius,
                 D(ReconstructionFilter, radius))
            .def("eval", &ReconstructionFilter::eval,
                 D(ReconstructionFilter, eval), "x"_a, "active"_a = true)
            .def("eval_discretized", &ReconstructionFilter::eval_discretized,
                 D(ReconstructionFilter, eval_discretized), "x"_a,
                 "active"_a = true);
    }
}

MI_PY_EXPORT(rfilter) {
    MI_PY_IMPORT_TYPES(ReconstructionFilter)
    using BitmapReconstructionFilter = Bitmap::ReconstructionFilter;

    bind_rfilter<ReconstructionFilter>(m, "ReconstructionFilter");
    bind_rfilter<BitmapReconstructionFilter>(m, "BitmapReconstructionFilter");
}
