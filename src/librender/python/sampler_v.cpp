#include <mitsuba/python/python.h>
#include <mitsuba/render/sampler.h>

MTS_PY_EXPORT(Sampler) {
    MTS_PY_IMPORT_TYPES(Sampler)
    MTS_PY_CLASS(Sampler, Object)
        .def_method(Sampler, clone)
        .def_method(Sampler, sample_count)
        .def_method(Sampler, wavefront_size)
        .def("seed", vectorize(&Sampler::seed),
             "seed_value"_a, D(Sampler, seed))
        .def("next_1d", vectorize(&Sampler::next_1d),
             "active"_a = true, D(Sampler, next_1d))
        .def("next_2d", vectorize(&Sampler::next_2d),
             "active"_a = true, D(Sampler, next_2d));
}
