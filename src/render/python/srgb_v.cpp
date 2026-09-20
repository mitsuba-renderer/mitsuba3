#include <mitsuba/render/srgb.h>
#include <mitsuba/python/python.h>

MI_PY_EXPORT(srgb) {
    MI_PY_IMPORT_TYPES()

    if constexpr (dr::is_jit_v<Float>)
        m.def("srgb_model_fetch", &SRGBModel<Float, Spectrum>::template fetch<Float>,
              "color"_a, D(SRGBModel, fetch));

    m.def("srgb_model_fetch", &SRGBModel<Float, Spectrum>::template fetch<float>,
          "color"_a, D(SRGBModel, fetch))
    .def("srgb_model_eval",
        &srgb_model_eval<unpolarized_spectrum_t<Spectrum>, dr::Array<Float, 3>>,
        D(srgb_model_eval))
    .def("srgb_model_mean",
        &srgb_model_mean<dr::Array<Float, 3>>,
        D(srgb_model_mean))
      ;
}
