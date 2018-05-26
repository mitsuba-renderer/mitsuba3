#pragma once

#include <mitsuba/mitsuba.h>
#include <mitsuba/core/transform.h>

NAMESPACE_BEGIN(mitsuba)

extern MTS_EXPORT_RENDER uint8_t srgb_coeffs[24576];

template <typename Vector4, typename Value>
MTS_INLINE Value srgb_model_eval(const Vector4 &coeff, const Value &wavelengths) {
    Value v = fmadd(fmadd(coeff.x(), wavelengths, coeff.y()), wavelengths, coeff.z());
    return rcp(1.f + exp(v)) * coeff.w();
}

template <typename Vector4, typename Value = value_t<Vector4>>
MTS_INLINE Value srgb_model_mean(const Vector4 &coeff) {
    using Vec = Array<Value, 16>;
    Vec wavelengths = linspace<Vec>(MTS_WAVELENGTH_MIN, MTS_WAVELENGTH_MAX);
    Vec v = fmadd(fmadd(coeff.x(), wavelengths, coeff.y()), wavelengths, coeff.z());
    return mean(rcp(1.f + exp(v))) * coeff.w();
}

template <typename Color3,
          typename Value    = typename Color3::Value,
          typename Int      = int_array_t<Value>,
          typename Vector2  = Vector<Value, 2>,
          typename Vector2i = Vector<Int, 2>,
          typename Vector3  = Vector<Value, 3>,
          typename Vector3h = float16_array_t<Vector3>,
          typename Vector4  = Vector<Value, 4>>
MTS_INLINE Vector4 srgb_model_fetch(Color3 c) {
    Value cmax = hmax(c);

    /* sRGB -> (X, Y, X+Y+Z) */
    const Matrix3f M(0.412453f,  0.35758f,   0.180423f,
                     0.212671f,  0.71516f,   0.072169f,
                     0.644458f,  1.191933f,  1.202819f);
    c = M * c;

    /* XYZ -> xy */
    Value inv_y = rcp(c.z());
    Vector2 xy(c.x() * inv_y, c.y() * inv_y);

    /* Subtract xy coordinates of blue primary */
    Vector2 bary = xy - Vector2f(0.15f, 0.06f);

    /* xy -> barycentric coordinates in sRGB gamut */
    Matrix2f M2(-1.20481884f,  2.18652251f,
                 2.40963985f, -0.66934387f);
    bary = M2 * bary;

    /* Triangle to square */
    Vector2 uv(bary.y() / (1.f - bary.x()),
               bary.x() * (2.f - bary.x()));

    /* Map to position in interpolant */
    const int res = 64;
    uv *= Float(res - 1);
    Vector2i ui(uv);
    ui = enoki::min(enoki::max(ui, 0), res - 2);
    Int offset = (ui.x() + ui.y() * res);

    /* Return bilinearly interpolated lookup */
    const half *data = (const half *) &srgb_coeffs[0];
    Vector3 v00 = gather<Vector3h>(data, offset);
    Vector3 v01 = gather<Vector3h>(data, offset + 1);
    Vector3 v10 = gather<Vector3h>(data, offset + (res));
    Vector3 v11 = gather<Vector3h>(data, offset + (res+1));
    Vector2 b1 = uv - Vector2(ui), b0  = 1.f - b1;

    Vector3 interp = (v00 * b0.x() + v01 * b1.x()) * b0.y() +
                     (v10 * b0.x() + v11 * b1.x()) * b1.y();

    return select(
        eq(cmax, 0.f),
        zero<Vector4>(),
        Vector4(-interp.x(), -interp.y(), -interp.z(), cmax)
    );
}

NAMESPACE_END(mitsuba)
