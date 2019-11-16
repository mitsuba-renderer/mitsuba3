#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/spectrum.h>
#include <mitsuba/render/srgb.h>
#include <rgb2spec.h>
#include <tbb/tbb.h>

NAMESPACE_BEGIN(mitsuba)

static RGB2Spec *model = nullptr;
static tbb::spin_mutex model_mutex;

Array<float, 3> srgb_model_fetch(const Color<float, 3> &c) {
    using Array3f = Array<float, 3>;

    if (unlikely(model == nullptr)) {
        tbb::spin_mutex::scoped_lock sl(model_mutex);
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

    if (c == Array3f(0.f))
        return Array3f(0.f, 0.f, -math::Infinity<float>);
    else if (c == Array3f(1.f))
        return Array3f(0.f, 0.f,  math::Infinity<float>);

    float rgb[3] = { (float) c.r(), (float) c.g(), (float) c.b() };
    float out[3];
    rgb2spec_fetch(model, rgb, out);

    return Array3f(out[0], out[1], out[2]);
}

Color<float, 3> srgb_model_eval_rgb(const Array<float, 3> &coeff) {
    using Array3f = Array<float, 3>;
    using Spectrum = Spectrum<float, 4>;
    using Matrix3f = Matrix<float, 3>;
    using ContinuousSpectrum = ContinuousSpectrum<float, Spectrum>;
    using Wavelength = wavelength_t<Spectrum>;

    ref<ContinuousSpectrum> d65 =
        PluginManager::instance()->create_object<ContinuousSpectrum>(
            Properties("d65"));
    auto expanded = d65->expand();
    if (expanded.size() == 1)
        d65 = (ContinuousSpectrum *) expanded[0].get();

    const size_t n_samples = ((MTS_CIE_SAMPLES - 1) * 3 + 1);

    Array3f accum = 0.f;
    float h = (MTS_CIE_MAX - MTS_CIE_MIN) / (n_samples - 1);
    for (size_t i = 0; i < n_samples; ++i) {
        float lambda = MTS_CIE_MIN + i * h;

        float weight = 3.f / 8.f * h;
        if (i == 0 || i == n_samples - 1)
            ;
        else if ((i - 1) % 3 == 2)
            weight *= 2.f;
        else
            weight *= 3.f;

        Array3f weight_v = weight * d65->eval(lambda).x() * cie1931_xyz(lambda);

        float model_eval =
            srgb_model_eval<Spectrum>(coeff, Wavelength(lambda)).x();

        accum = fmadd(weight_v, model_eval, accum);
    }

    Matrix3f xyz_to_srgb(
         3.240479f, -1.537150f, -0.498535f,
        -0.969256f,  1.875991f,  0.041556f,
         0.055648f, -0.204043f,  1.057311f
    );

    return xyz_to_srgb * accum;
}

NAMESPACE_END(mitsuba)
