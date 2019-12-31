#include <mitsuba/core/rfilter.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(rfilter) {
    MTS_PY_IMPORT_TYPES(ReconstructionFilter)

    MTS_PY_CLASS(ReconstructionFilter, Object)
        .def_method(ReconstructionFilter, border_size)
        .def_method(ReconstructionFilter, radius)
        .def("eval",
            vectorize(&ReconstructionFilter::eval),
            D(ReconstructionFilter, eval), "x"_a, "active"_a = true)
        .def("eval_discretized",
            vectorize(&ReconstructionFilter::eval_discretized),
            D(ReconstructionFilter, eval_discretized), "x"_a, "active"_a = true)
        ;
}
