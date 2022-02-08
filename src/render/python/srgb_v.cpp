#include <mitsuba/render/srgb.h>
#include <mitsuba/python/python.h>

MI_PY_EXPORT(srgb) {
    MI_PY_IMPORT_TYPES()
    m.def("srgb_model_fetch", &srgb_model_fetch, D(srgb_model_fetch))
    // .def("srgb_model_eval_rgb", &srgb_model_eval_rgb, D(srgb_model_eval_rgb))
    .def("srgb_model_eval",
        &srgb_model_eval<unpolarized_spectrum_t<Spectrum>, dr::Array<Float, 3>>,
        D(srgb_model_eval))
    .def("srgb_model_mean",
        &srgb_model_mean<dr::Array<Float, 3>>,
        D(srgb_model_mean))
      ;
}
