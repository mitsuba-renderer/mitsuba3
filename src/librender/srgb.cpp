#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/spectrum.h>
#include <mitsuba/render/srgb.h>
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
        tbb::spin_mutex::scoped_lock sl(model_mutex);
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

template <typename Value> Color<Value, 3> srgb_model_eval_rgb(const Vector<Value, 3> &coeff) {
    ref<ContinuousSpectrum> d65 =
        PluginManager::instance()->create_object<ContinuousSpectrum>(
            Properties("d65"));
    auto expanded = d65->expand();
    if (expanded.size() == 1)
        d65 = (ContinuousSpectrum *) expanded[0].get();

    const size_t n_samples = ((MTS_CIE_SAMPLES - 1) * 3 + 1);
    set_label(coeff, "coeff");

    Vector<Value, 3> accum = 0.f;
    Float h = (MTS_CIE_MAX - MTS_CIE_MIN) / (n_samples - 1);
    for (size_t i = 0; i < n_samples; ++i) {
        Float lambda = MTS_CIE_MIN + i * h;

        Float weight = 3.f / 8.f * h;
        if (i == 0 || i == n_samples - 1)
            ;
        else if ((i - 1) % 3 == 2)
            weight *= 2.f;
        else
            weight *= 3.f;

        Vector<Value, 3> weight_v = weight * d65->eval(Spectrumf(lambda))[0] * cie1931_xyz(lambda);
        Value lambda_v = Value(lambda);

        Value model_eval = srgb_model_eval(coeff, lambda_v);

        accum = fmadd(weight_v, model_eval, accum);

        set_label(lambda_v, ("lambda_" + std::to_string(i)).c_str());
        set_label(weight_v, ("weight_" + std::to_string(i)).c_str());
        set_label(accum, ("accum_" + std::to_string(i)).c_str());
        set_label(model_eval, ("model_eval_" + std::to_string(i)).c_str());
    }

    Matrix3f xyz_to_srgb(
         3.240479f, -1.537150f, -0.498535f,
        -0.969256f,  1.875991f,  0.041556f,
         0.055648f, -0.204043f,  1.057311f
    );

    return xyz_to_srgb * accum;
}

#if defined(MTS_ENABLE_AUTODIFF)

/// Differentiable variant of \ref rgb2spec_fetch
Vector3fD rgb2spec_fetch_d(RGB2Spec *model, const Color3fD &rgb_) {
    // TODO: merge with rgb2spec_fetch, with if constexpr where appropriate
    Color3fD rgb_clamp = max(min(rgb_, 1.f), 0.f);
    size_t res  = model->res;

    // Compute permutations, z is max
    MaskD r_max = (rgb_clamp.x() >= rgb_clamp.y()) & (rgb_clamp.x() >= rgb_clamp.z());
    MaskD g_max = (rgb_clamp.y() >= rgb_clamp.x()) & (rgb_clamp.y() >= rgb_clamp.z());
    Color3fD xyz(rgb_clamp);
    UInt32D i(2);

    masked(xyz.x(), r_max) = rgb_clamp.y();
    masked(xyz.y(), r_max) = rgb_clamp.z();
    masked(xyz.z(), r_max) = rgb_clamp.x();
    masked(i, r_max)       = 0;

    masked(xyz.x(), g_max) = rgb_clamp.z();
    masked(xyz.y(), g_max) = rgb_clamp.x();
    masked(xyz.z(), g_max) = rgb_clamp.y();
    masked(i, g_max)       = 1;

    FloatD z_d     = xyz.z();
    FloatD scale_d = FloatD(res - 1.f) / z_d;
    FloatD x_d     = xyz.x() * scale_d;
    FloatD y_d     = xyz.y() * scale_d;

    // Trilinearly interpolated lookup
    UInt32D xi = min(UInt32D(x_d), UInt32D(res - 2)), yi = min(UInt32D(y_d), UInt32D(res - 2));

    FloatD values = FloatC::copy(model->scale, model->res);
    FloatD data   = FloatC::copy(model->data, res * res * res * 9);

    UInt32D zi = math::find_interval(
        model->res,
        [&](UInt32D idx, MaskD active) { return gather<FloatD>(values, idx, active) <= z_d; },
        true);

    UInt32D offset = (((i * res + zi) * res + yi) * res + xi) * 3;
    size_t dx      = 3;
    size_t dy      = 3 * res;
    size_t dz      = 3 * res * res;

    FloatD scale_zi    = gather<FloatD>(values, zi);
    FloatD scale_zi_p1 = gather<FloatD>(values, zi + 1);

    // Differentiable weights
    FloatD x1 = x_d - xi, x0 = 1.f - x1, y1 = y_d - yi, y0 = 1.f - y1,
           z1 = (z_d - scale_zi) / (scale_zi_p1 - scale_zi), z0 = 1.f - z1;

    Vector3fD output(0.f);

    // Loop on the 3 coefficients
    for (int j = 0; j < 3; ++j) {
        // Need to do 8 gathers, and combine then with the weights
        FloatD v000 = gather<FloatD>(data, offset);
        FloatD v100 = gather<FloatD>(data, offset + dx);
        FloatD v010 = gather<FloatD>(data, offset + dy);
        FloatD v110 = gather<FloatD>(data, offset + dy + dx);
        FloatD v001 = gather<FloatD>(data, offset + dz);
        FloatD v101 = gather<FloatD>(data, offset + dx + dz);
        FloatD v011 = gather<FloatD>(data, offset + dy + dz);
        FloatD v111 = gather<FloatD>(data, offset + dy + dx + dz);

        FloatD coeff = ((v000 * x0 + v100 * x1) * y0 + (v010 * x0 + v110 * x1) * y1) * z0 +
                       ((v001 * x0 + v101 * x1) * y0 + (v011 * x0 + v111 * x1) * y1) * z1;

        output[j] = coeff;
        offset += 1;
    }

    return output;
}

Vector3fD srgb_model_fetch_d(const Color3fD &c) {
    // TODO: merge with rgb2spec_fetch, with if constexpr where appropriate
    if (unlikely(model == nullptr)) {
        tbb::spin_mutex::scoped_lock c(model_mutex);
        if (model == nullptr) {
            FileResolver *fr  = Thread::thread()->file_resolver();
            std::string fname = fr->resolve("data/srgb.coeff").string();
            Log(EInfo, "Loading spectral upsampling model \"data/srgb.coeff\" .. ");
            model = rgb2spec_load(fname.c_str());
            if (model == nullptr)
                Throw("Could not load sRGB-to-spectrum upsampling model ('data/srgb.coeff')");
            atexit([] { rgb2spec_free(model); });
        }
    }

    return rgb2spec_fetch_d(model, c);
}

template MTS_EXPORT_RENDER Color<FloatD, 3> srgb_model_eval_rgb(const Vector<FloatD, 3> &coeff);
#endif // end MTS_ENABLE_AUTODIFF

template MTS_EXPORT_RENDER Color<float, 3> srgb_model_eval_rgb(const Vector<float, 3> &coeff);

NAMESPACE_END(mitsuba)
