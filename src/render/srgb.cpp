#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/render/srgb.h>
#include <rgb2spec.h>
#include <mutex>

NAMESPACE_BEGIN(mitsuba)

static RGB2Spec *model = nullptr;
static std::mutex model_mutex;

dr::Array<float, 3> srgb_model_fetch(const Color<float, 3> &c) {
    using Array3f = dr::Array<float, 3>;

    if (unlikely(model == nullptr)) {
        std::lock_guard<std::mutex> lock(model_mutex);
        if (model == nullptr) {
            FileResolver *fr = file_resolver();
            std::string fname = fr->resolve("data/srgb.coeff").string();
            Log(Info, "Loading spectral upsampling model \"data/srgb.coeff\" .. ");
            model = rgb2spec_load(fname.c_str());
            if (model == nullptr)
                Throw("Could not load sRGB-to-spectrum upsampling model ('data/srgb.coeff')");
            atexit([]{ rgb2spec_free(model); });
        }
    }

    float rgb[3] = { (float) c.r(), (float) c.g(), (float) c.b() };
    float out[3];
    rgb2spec_fetch(model, rgb, out);

    return Array3f(out[0], out[1], out[2]);
}

NAMESPACE_END(mitsuba)
