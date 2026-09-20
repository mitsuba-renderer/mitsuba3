#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/render/srgb.h>
#include <rgb2spec.h>
#include <atomic>
#include <mutex>

NAMESPACE_BEGIN(mitsuba)

/// Serializes the initialization and release of the per-variant models
static std::mutex model_mutex;

/// Model state of one variant
template <typename Float> struct SRGBModelState {
    using Float32 = dr::float32_array_t<dr::detached_t<Float>>;

    RGB2Spec *model = nullptr;

    /// Device-resident copies of the coefficient table and the warped
    /// positions along its last axis (JIT variants only)
    Float32 data, scale;
};

template <typename Float, typename Spectrum>
static std::atomic<SRGBModelState<Float> *> srgb_model_state { nullptr };

/// Return the model of a variant, loading it on first use
template <typename Float, typename Spectrum>
static SRGBModelState<Float> *srgb_model() {
    using State = SRGBModelState<Float>;
    std::atomic<State *> &s_state = srgb_model_state<Float, Spectrum>;

    State *s = s_state.load(std::memory_order_acquire);
    if (likely(s))
        return s;

    std::lock_guard<std::mutex> lock(model_mutex);
    s = s_state.load(std::memory_order_relaxed);
    if (!s) {
        std::string fname = file_resolver()->resolve("data/srgb.coeff").string();
        Log(Info, "Loading spectral upsampling model \"data/srgb.coeff\" .. ");

        RGB2Spec *model = rgb2spec_load(fname.c_str());
        if (!model)
            Throw("Could not load sRGB-to-spectrum upsampling model ('data/srgb.coeff')");

        s = new State();
        s->model = model;

        if constexpr (dr::is_jit_v<Float>) {
            using Float32 = typename State::Float32;
            size_t res = model->res;
            s->data  = dr::load<Float32>(model->data, res * res * res * 9);
            s->scale = dr::load<Float32>(model->scale, res);
        }

        s_state.store(s, std::memory_order_release);
    }

    return s;
}

MI_VARIANT void SRGBModel<Float, Spectrum>::static_shutdown() {
    std::lock_guard<std::mutex> lock(model_mutex);
    SRGBModelState<Float> *s = srgb_model_state<Float, Spectrum>.exchange(nullptr);
    if (s) {
        rgb2spec_free(s->model);
        delete s;
    }
}

MI_VARIANT dr::Array<float, 3>
SRGBModel<Float, Spectrum>::fetch_scalar(const Color<float, 3> &c) {
    float rgb[3] = { c.r(), c.g(), c.b() }, out[3];
    rgb2spec_fetch(srgb_model<Float, Spectrum>()->model, rgb, out);
    return { out[0], out[1], out[2] };
}

MI_VARIANT dr::Array<Float, 3>
SRGBModel<Float, Spectrum>::fetch_jit(const Color<Float, 3> &color) {
    if constexpr (!dr::is_jit_v<Float>) {
        return fetch_scalar(color);
    } else {
        using Array3f = dr::Array<Float, 3>;
        using UInt32  = dr::uint32_array_t<Float>;
        using Mask    = dr::mask_t<Float>;
        using State   = SRGBModelState<Float>;
        using Float32 = typename State::Float32;

        const State *s = srgb_model<Float, Spectrum>();
        uint32_t res = s->model->res;
        float res_f = (float) res;

        Color<Float, 3> c = dr::clip(color, 0.f, 1.f);

        // The table is indexed by the two smaller components divided by the
        // largest one, which also selects one of the three sub-tables. Ties
        // resolve towards the last component.
        Mask g_ge_r = c.g() >= c.r();
        Float rg = dr::select(g_ge_r, c.g(), c.r());
        Mask b_largest = c.b() >= rg;

        UInt32 i = dr::select(b_largest, 2u, dr::select(g_ge_r, 1u, 0u));
        Float z = dr::select(b_largest, c.b(), rg),
              x = dr::select(b_largest, c.r(), dr::select(g_ge_r, c.b(), c.g())),
              y = dr::select(b_largest, c.g(), dr::select(g_ge_r, c.r(), c.b()));

        // Guard the division; monochromatic colors take the closed form below
        Float scale = (res_f - 1.f) / dr::maximum(z, 1e-9f);
        x *= scale;
        y *= scale;

        // The last axis of the table is nonuniformly spaced. Its 'res' node
        // positions are stored in 's->scale'. A binary search finds the
        // interval that contains 'z', mirroring the host implementation.
        auto scale_at = [&](const UInt32 &idx) {
            return Float(dr::gather<Float32>(s->scale, dr::detach<false>(idx)));
        };
        UInt32 zi = dr::binary_search<UInt32>(1, res - 1, [&](const UInt32 &idx) {
            return scale_at(idx) <= z;
        }) - 1;
        Float z0 = scale_at(zi), z1 = scale_at(zi + 1);

        // Texel coordinates and interpolation weights
        UInt32 xi = dr::minimum(UInt32(x), res - 2),
               yi = dr::minimum(UInt32(y), res - 2);

        Float wx = x - Float(xi),
              wy = y - Float(yi),
              wz = (z - z0) / (z1 - z0);

        // Trilinear interpolation of the three coefficients stored per texel
        UInt32 index = ((i * res + zi) * res + yi) * res + xi,
               dy = res, dz = res * res;

        auto fetch = [&](const UInt32 &idx) {
            return Array3f(dr::gather<dr::Array<Float32, 3>>(s->data, dr::detach<false>(idx)));
        };
        auto lerp_x = [&](const UInt32 &idx) {
            return dr::lerp(fetch(idx), fetch(idx + 1), wx);
        };

        Array3f result = dr::lerp(
            dr::lerp(lerp_x(index), lerp_x(index + dy), wy),
            dr::lerp(lerp_x(index + dz), lerp_x(index + dz + dy), wy), wz);

        // Monochromatic colors have a closed form solution. Black and white map
        // to -/+ 8192, which ``srgb_model_eval()`` turns into 0 and 1.
        Float v = c.r();
        Mask mono = (c.g() == v) && (c.b() == v);
        Float mono_c2 = dr::select(
            v <= 0.f, -8192.f,
            dr::select(v >= 1.f, 8192.f, (v - .5f) * dr::rsqrt(v * (1.f - v))));

        // Propagate derivatives to the full table-based lookup, otherwise the
        // closed form solution would disconnect the other two channels.
        Array3f mono_coeff(0.f, 0.f, mono_c2);
        if constexpr (dr::is_diff_v<Float>)
            mono_coeff = dr::replace_grad(mono_coeff, result);

        return dr::select(mono, mono_coeff, result);
    }
}

MI_INSTANTIATE_STRUCT(SRGBModel)
NAMESPACE_END(mitsuba)
