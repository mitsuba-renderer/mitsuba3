#include <mitsuba/render/srgb.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(srgb) {
    MTS_PY_IMPORT_TYPES()
    m.def("srgb_model_fetch", &srgb_model_fetch, D(srgb_model_fetch))
    // .def("srgb_model_eval_rgb", &srgb_model_eval_rgb, D(srgb_model_eval_rgb))
    .def("srgb_model_eval",
        vectorize(&srgb_model_eval<depolarize_t<Spectrum>, Array<Float, 3>>),
        D(srgb_model_eval))
    .def("srgb_model_mean",
        vectorize(&srgb_model_mean<Array<Float, 3>>),
        D(srgb_model_mean))
      ;
}
