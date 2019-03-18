#include <mitsuba/render/srgb.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(srgb) {
    m.def("srgb_model_fetch", &srgb_model_fetch, D(srgb_model_fetch));
    m.def("srgb_model_eval_rgb", &srgb_model_eval_rgb<float>, D(srgb_model_eval_rgb));

#if defined(MTS_ENABLE_AUTODIFF)
    m.def("srgb_model_eval_rgb", &srgb_model_eval_rgb<FloatD>, D(srgb_model_eval_rgb));
#endif

    m.def("srgb_model_eval", [](const Vector3f &coeff, const FloatX &s) {
        FloatX result(s.size());
        for (size_t i = 0; i < packets(s); ++i)
            packet(result, i) = srgb_model_eval(coeff, packet(s, i));
        return result;
    });

    m.def("srgb_model_mean", &srgb_model_mean<Vector3f>, D(srgb_model_mean));
    m.def("srgb_model_mean", vectorize_wrapper(&srgb_model_mean<Vector3fP>));
}
