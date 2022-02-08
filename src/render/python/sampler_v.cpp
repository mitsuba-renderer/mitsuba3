#include <mitsuba/render/sampler.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(Sampler) {
    MTS_PY_IMPORT_TYPES(Sampler)
    MTS_PY_CLASS(Sampler, Object)
        .def_method(Sampler, fork)
        .def_method(Sampler, clone)
        .def_method(Sampler, sample_count)
        .def_method(Sampler, wavefront_size)
        .def_method(Sampler, set_samples_per_wavefront, "samples_per_wavefront"_a)
        .def_method(Sampler, set_sample_count, "spp"_a)
        .def_method(Sampler, advance)
        .def_method(Sampler, schedule_state)
        .def("loop_put", &Sampler::loop_put, "loop"_a)
        .def_method(Sampler, seed, "seed"_a, "wavefront_size"_a = (uint32_t) -1)
        .def_method(Sampler, next_1d, "active"_a = true)
        .def_method(Sampler, next_2d, "active"_a = true);
}
