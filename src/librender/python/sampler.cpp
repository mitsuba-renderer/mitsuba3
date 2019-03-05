#include <mitsuba/python/python.h>
#include <mitsuba/render/sampler.h>

MTS_PY_EXPORT(Sampler) {
    MTS_PY_CLASS(Sampler, Object)
        .mdef(Sampler, clone)
        .mdef(Sampler, seed)
        .def("next_1d", py::overload_cast<>(&Sampler::next_1d),
             D(Sampler, next_1d))
        .def("next_1d_p", [](Sampler &sampler) { return sampler.next_1d_p(); },
             D(Sampler, next_1d_p))
#if defined(MTS_ENABLE_AUTODIFF)
        .def("next_1d_d", [](Sampler &sampler) { return sampler.next_1d_d(); })
#endif
        .def("next_2d", py::overload_cast<>(&Sampler::next_2d),
             D(Sampler, next_2d))
        .def("next_2d_p", [](Sampler &sampler) { return sampler.next_2d_p(); },
             D(Sampler, next_2d_p))
#if defined(MTS_ENABLE_AUTODIFF)
        .def("next_2d_d", [](Sampler &sampler) { return sampler.next_2d_d(); })
#endif
        .mdef(Sampler, sample_count);
}
