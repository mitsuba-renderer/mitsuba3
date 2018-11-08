#include <mitsuba/render/srgb.h>
#include <mitsuba/core/fresolver.h>
#include <rgb2spec.h>
#include <tbb/tbb.h>

NAMESPACE_BEGIN(mitsuba)

template MTS_EXPORT_RENDER Spectrumf srgb_model_eval(const Vector3f &, const Spectrumf &);
template MTS_EXPORT_RENDER SpectrumfP srgb_model_eval(const Vector3fP &, const SpectrumfP &);

template MTS_EXPORT_RENDER Float srgb_model_mean(const Vector3f &);
template MTS_EXPORT_RENDER FloatP srgb_model_mean(const Vector3fP &);

static RGB2Spec *model = nullptr;
static tbb::spin_mutex model_mutex;

Vector3f srgb_model_fetch(const Color3f &c) {
    if (unlikely(model == nullptr)) {
        tbb::spin_mutex::scoped_lock c(model_mutex);
        if (model == nullptr) {
            FileResolver *fr = Thread::thread()->file_resolver();
            std::string fname = fr->resolve("data/srgb.coeff").string();
            Log(EInfo, "Loading spectral upsampling model \"data/srgb.coeff\" .. ");
            model = rgb2spec_load(fname.c_str());
            if (model == nullptr)
                Throw("Could not load sRGB-to-spectrum upsampling model ('data/srgb.coeff')");
            atexit([]{ rgb2spec_free(model); });
        }
    }

    float rgb[3] = { (float) c.r(), (float) c.g(), (float) c.b() };
    float out[3];
    rgb2spec_fetch(model, rgb, out);

    return Vector3f(out[0], out[1], out[2]);
}

NAMESPACE_END(mitsuba)
