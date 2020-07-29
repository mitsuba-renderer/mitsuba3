#include <mitsuba/render/srgb.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(srgb) {
    MTS_PY_IMPORT_TYPES()
    m.def("srgb_model_fetch", &srgb_model_fetch, D(srgb_model_fetch))
    // .def("srgb_model_eval_rgb", &srgb_model_eval_rgb, D(srgb_model_eval_rgb))
    .def("srgb_model_eval",
        &srgb_model_eval<depolarize_t<Spectrum>, ek::Array<Float, 3>>,
        D(srgb_model_eval))
    .def("srgb_model_mean",
        &srgb_model_mean<ek::Array<Float, 3>>,
        D(srgb_model_mean))
      ;
}
