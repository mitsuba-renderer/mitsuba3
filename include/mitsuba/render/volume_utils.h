#pragma once

#include <mitsuba/core/fwd.h>
#include <mitsuba/core/platform.h>
#include <mitsuba/core/vector.h>
#include <drjit/texture.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Describes a volume's parametrization, including its transform and
 * wrapping behaviour.
 */
template<typename Float>
struct VolumeParametrization {

    using AffineTransform4f = Transform<Point<Float, 4>, true>;

    /// The volume transform
    AffineTransform4f to_world;

    /// The wrapping behaviour when exiting the volume boundaries.
    dr::WrapMode wrap_mode;

    VolumeParametrization()
        : to_world(AffineTransform4f()), wrap_mode(dr::WrapMode::Clamp) {}

    VolumeParametrization(AffineTransform4f to_world,
                          dr::WrapMode wrap_mode = dr::WrapMode::Clamp)
        : to_world(to_world), wrap_mode(wrap_mode) {}
};

/**
 * \brief Applies the configured texture wrapping mode to an integer
 * position,
 */
template <typename T> T wrap(
    const T &pos, const T &res,
    const dr::divisor<dr::scalar_t<T>> *inv_res,
    dr::WrapMode wrap_mode) {
    using Scalar = dr::scalar_t<T>;

    constexpr size_t Dimension = dr::size_v<T>;

    static_assert(
        std::is_integral_v<Scalar> &&
        std::is_signed_v<Scalar>
    );

    if (wrap_mode == dr::WrapMode::Clamp) {
        return clip(pos, 0, res - 1);
    } else {
        T value_shift_neg = select(pos < 0, pos + 1, pos);

        T div;
        for (size_t i = 0; i < Dimension; ++i)
            div[i] = inv_res[i](value_shift_neg[i]);

        T mod = pos - div * res;
        mod[mod < 0] += T(res);

        if (wrap_mode == dr::WrapMode::Mirror)
            // Starting at 0, flip the texture every other repetition
            // (flip when: even number of repetitions in negative direction,
            // or odd number of repetitions in positive direction)
            mod = select(((div & 1) == 0) ^ (pos < 0), mod, res - 1 - mod);

        return mod;
    }
}

NAMESPACE_END(mitsuba)
