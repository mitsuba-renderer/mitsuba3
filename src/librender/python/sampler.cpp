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
        .def("next_1d_p",
             [](Sampler &sampler, const MaskP &active) {
                 return sampler.next_1d_p(active);
             },
             D(Sampler, next_1d_p))
#if defined(MTS_ENABLE_AUTODIFF)
        .def("next_1d_d", &Sampler::next_1d_d, "active"_a)
#endif
        .def("next_2d", py::overload_cast<>(&Sampler::next_2d),
             D(Sampler, next_2d))
        .def("next_2d_p", [](Sampler &sampler) { return sampler.next_2d_p(); },
             D(Sampler, next_2d_p))
        .def("next_2d_p",
             [](Sampler &sampler, const MaskP &active) {
                 return sampler.next_2d_p(active);
             },
             D(Sampler, next_2d_p))
#if defined(MTS_ENABLE_AUTODIFF)
        .def("next_2d_d", &Sampler::next_2d_d, "active"_a)
#endif
        .mdef(Sampler, sample_count);
}
