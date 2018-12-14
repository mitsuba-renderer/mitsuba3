#include <mitsuba/render/srgb.h>
#include <mitsuba/render/spectrum.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
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

    if (c == Vector3f(0.f))
        return Vector3f(0.f, 0.f, -math::Infinity);
    else if (c == Vector3f(1.f))
        return Vector3f(0.f, 0.f,  math::Infinity);

    float rgb[3] = { (float) c.r(), (float) c.g(), (float) c.b() };
    float out[3];
    rgb2spec_fetch(model, rgb, out);

    return Vector3f(out[0], out[1], out[2]);
}

Color3f srgb_model_eval_rgb(const Vector3f &coeff) {
    ref<ContinuousSpectrum> d65 =
        PluginManager::instance()->create_object<ContinuousSpectrum>(
            Properties("d65"));
    auto expanded = d65->expand();
    if (expanded.size() == 1)
        d65 = (ContinuousSpectrum *) expanded[0].get();

    Vector3f accum = zero<Vector3f>();
    for (size_t i = 0; i < 1000; ++i) {
        Float lambda = i * (1.f / 999.f) * (830.f - 360.f) + 360.f;
        accum += cie1931_xyz(lambda) * srgb_model_eval(coeff, lambda) *
                 d65->eval(Spectrumf(lambda))[0];
    }
    accum *= (830.f - 360.f) / 1000.f;

    Matrix3f xyz_to_srgb(
         3.240479, -1.537150, -0.498535,
        -0.969256,  1.875991,  0.041556,
         0.055648, -0.204043,  1.057311
    );

    return xyz_to_srgb * accum;
}

NAMESPACE_END(mitsuba)
