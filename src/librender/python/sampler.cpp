#include <mitsuba/python/python.h>
#include <mitsuba/render/sampler.h>

MTS_PY_EXPORT_CLASS_VARIANTS(Sampler) {
    MTS_PY_CLASS(Sampler, Object)
        .mdef(Sampler, clone)
        .mdef(Sampler, seed, "seed"_a, "size"_a)
        .mdef(Sampler, next_1d, "active"_a)
        .mdef(Sampler, next_2d, "active"_a)
        .mdef(Sampler, sample_count);
}
