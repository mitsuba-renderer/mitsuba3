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
            FileResolver *fr = Thread::thread()->file_resolver();
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

#if 0
Color<float, 3> srgb_model_eval_rgb(const dr::Array<float, 3> &coeff) {
    using Array3f = dr::Array<float, 3>;
    using Spectrum = Spectrum<float, 4>;
    using Matrix3f = Matrix<float, 3>;
    using Texture = Texture<float, Spectrum>;
    using Wavelength = wavelength_t<Spectrum>;
    using SurfaceInteraction3f = SurfaceInteraction<float, Spectrum>;

    ref<Texture> d65 =
        PluginManager::instance()->create_object<Texture>(
            Properties("d65"));
    auto expanded = d65->expand();
    if (expanded.size() == 1)
        d65 = (Texture *) expanded[0].get();

    const size_t n_samples = ((MI_CIE_SAMPLES - 1) * 3 + 1);

    SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();
    Array3f accum = 0.f;
    float h = (MI_CIE_MAX - MI_CIE_MIN) / (n_samples - 1);
    for (size_t i = 0; i < n_samples; ++i) {
        float lambda = MI_CIE_MIN + i * h;

        float weight = 3.f / 8.f * h;
        if (i == 0 || i == n_samples - 1)
            ;
        else if ((i - 1) % 3 == 2)
            weight *= 2.f;
        else
            weight *= 3.f;

        si.wavelengths = lambda;
        Array3f weight_v = weight * d65->eval(si).x() * cie1931_xyz(lambda);

        float model_eval =
            srgb_model_eval<Spectrum>(coeff, Wavelength(lambda)).x();

        accum = dr::fmadd(weight_v, model_eval, accum);
    }

    Matrix3f xyz_to_srgb(
         3.240479f, -1.537150f, -0.498535f,
        -0.969256f,  1.875991f,  0.041556f,
         0.055648f, -0.204043f,  1.057311f
    );

    return xyz_to_srgb * accum;
}
#endif

NAMESPACE_END(mitsuba)
