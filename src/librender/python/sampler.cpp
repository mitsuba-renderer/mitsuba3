#include <mitsuba/python/python.h>
#include <mitsuba/render/sampler.h>

MTS_PY_EXPORT(Sampler) {
    MTS_IMPORT_TYPES(Sampler)
    MTS_PY_CHECK_ALIAS(Sampler, m) {
        MTS_PY_CLASS(Sampler, Object)
            .def_method(Sampler, clone)
            .def("seed", vectorize<Float>(&Sampler::seed), "seed_value"_a)
            .def("next_1d", vectorize<Float>(&Sampler::next_1d), "active"_a = true)
            .def("next_2d", vectorize<Float>(&Sampler::next_2d), "active"_a = true)
            .def_method(Sampler, sample_count);
    }
}
