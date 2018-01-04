#include <mitsuba/render/sampler.h>
#include <mitsuba/python/python.h>


MTS_PY_EXPORT(Sampler) {
    MTS_PY_CLASS(Sampler, Object)
        .mdef(Sampler, clone)
        .mdef(Sampler, seed)
        .def("next_1d", py::overload_cast<bool>(&Sampler::next_1d),
             "unused"_a = true, D(Sampler, next_1d))
        .def("next_1d_p", py::overload_cast<MaskP>(&Sampler::next_1d_p),
             "active"_a = MaskP(true), D(Sampler, next_1d_p))
        .def("next_2d", py::overload_cast<bool>(&Sampler::next_2d),
             "unused"_a = true, D(Sampler, next_2d))
        .def("next_2d_p", py::overload_cast<MaskP>(&Sampler::next_2d_p),
             "active"_a = MaskP(true), D(Sampler, next_2d_p))
        .mdef(Sampler, sample_count);
}
