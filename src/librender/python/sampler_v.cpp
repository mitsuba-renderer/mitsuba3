#include <mitsuba/render/sampler.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(Sampler) {
    MTS_PY_IMPORT_TYPES(Sampler)
    MTS_PY_CLASS(Sampler, Object)
        .def_method(Sampler, clone)
        .def_method(Sampler, sample_count)
        .def_method(Sampler, wavefront_size)
        .def_method(Sampler, set_samples_per_wavefront, "samples_per_wavefront"_a)
        .def_method(Sampler, advance)
        .def("seed", vectorize(&Sampler::seed),
             "seed_offset"_a, "wavefront_size"_a = 1, D(Sampler, seed))
        .def("next_1d", vectorize(&Sampler::next_1d),
             "active"_a = true, D(Sampler, next_1d))
        .def("next_2d", vectorize(&Sampler::next_2d),
             "active"_a = true, D(Sampler, next_2d));
}
