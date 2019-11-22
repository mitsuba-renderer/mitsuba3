#include <mitsuba/python/python.h>
#include <mitsuba/render/sampler.h>

MTS_PY_EXPORT(Sampler) {
    MTS_IMPORT_TYPES(Sampler)
    MTS_PY_CHECK_ALIAS(Sampler, m) {
        MTS_PY_CLASS(Sampler, Object)
            .def_method(Sampler, clone)
            .def_method(Sampler, seed, "seed"_a)
            .def_method(Sampler, next_1d, "active"_a = true)
            .def_method(Sampler, next_2d, "active"_a = true)
            .def_method(Sampler, sample_count);
    }
}
