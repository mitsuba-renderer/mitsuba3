/*
    spline.h -- Spline evaluation and sampling routines
    Copyright (c) 2015 Wenzel Jakob <wenzel@inf.ethz.ch>
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#pragma once

#include <mitsuba/core/fwd.h>
#include <mitsuba/core/frame.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/vector.h>

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(spline)

/*
// Equivalent to what's implemented in term of gathers below

#define GET_SPLINE_UNIFORM(idx)
    Scalar f0 = values[idx],
           f1 = values[idx+1],
           d0, d1;
    if (idx > 0)
        d0 = (Scalar) 0.5 * (f1 - values[idx-1]);
    else
        d0 = f1 - f0;

    if (idx + 2 < size)
        d1 = (Scalar) 0.5 * (values[idx+2] - f0);
    else
        d1 = f1 - f0;

#define GET_SPLINE_NONUNIFORM(idx)
    Scalar f0    = values[idx],
           f1    = values[idx+1],
           x0    = nodes[idx],
           x1    = nodes[idx+1],
           width = x1 - x0,
           d0, d1;
    if (idx > 0)
        d0 = width * (f1 - values[idx-1]) / (x1 - nodes[idx-1]);
    else
        d0 = f1 - f0;

    if (idx + 2 < size)
        d1 = width * (values[idx+2] - f0) / (nodes[idx+2] - x0);
    else
        d1 = f1 - f0;
*/

#define GET_SPLINE_UNIFORM(idx)                                                \
    auto mask_low_idx = mask_t<Scalar>(idx > 0);                               \
    auto mask_up_idx  = mask_t<Scalar>(idx + 2 < size);                        \
    Scalar f_1        = gather<Scalar>(values, idx - 1, mask_low_idx);         \
    Scalar f0         = gather<Scalar>(values, idx);                           \
    Scalar f1         = gather<Scalar>(values, idx + 1);                       \
    Scalar f2         = gather<Scalar>(values, idx + 2, mask_up_idx);          \
    Scalar d0         = select(mask_low_idx, 0.5f * (f1 - f_1), f1 - f0);      \
    Scalar d1         = select(mask_up_idx, 0.5f * (f2 - f0), f1 - f0);

#define GET_SPLINE_NONUNIFORM(idx)                                             \
    auto mask_low_idx = mask_t<Scalar>(idx > 0);                               \
    auto mask_up_idx  = mask_t<Scalar>(idx + 2 < size);                        \
    Scalar f_1        = gather<Scalar>(values, idx - 1, mask_low_idx);         \
    Scalar f0         = gather<Scalar>(values, idx);                           \
    Scalar f1         = gather<Scalar>(values, idx + 1);                       \
    Scalar f2         = gather<Scalar>(values, idx + 2, mask_up_idx);          \
    Scalar x_1        = gather<Scalar>(nodes, idx - 1, mask_low_idx);          \
    Scalar x0         = gather<Scalar>(nodes, idx);                            \
    Scalar x1         = gather<Scalar>(nodes, idx + 1);                        \
    Scalar x2         = gather<Scalar>(nodes, idx + 2, mask_up_idx);           \
    Scalar width      = x1 - x0;                                               \
    Scalar d0         = select(mask_low_idx, width * (f1 - f_1) /              \
                                                     (x1 - x_1), f1 - f0);     \
    Scalar d1         = select(mask_up_idx,  width * (f2 - f0) /               \
                                                     (x2 - x0), f1 - f0);

// =======================================================================
//! @{ \name Functions for evaluating and sampling cubic Catmull-Rom splines
// =======================================================================


/**
 * \brief Compute the definite integral and derivative of a cubic spline that
 * is parameterized by the function values and derivatives at the endpoints
 * of the interval <tt>[0, 1]</tt>.
 *
 * \param f0
 *      The function value at the left position
 * \param f1
 *      The function value at the right position
 * \param d0
 *      The function derivative at the left position
 * \param d1
 *      The function derivative at the right position
 * \param t
 *      The parameter variable
 * \return
 *      The interpolated function value at \c t
 */
template <typename Scalar>
Scalar eval_spline(Scalar f0, Scalar f1, Scalar d0, Scalar d1, Scalar t) {
    Scalar t2 = t*t, t3 = t2*t;
    return ( 2*t3 - 3*t2 + 1) * f0 +
           (-2*t3 + 3*t2)     * f1 +
           (   t3 - 2*t2 + t) * d0 +
           (   t3 - t2)       * d1;
}

/**
 * \brief Compute the value and derivative of a cubic spline that is
 * parameterized by the function values and derivatives of the
 * interval <tt>[0, 1]</tt>.
 *
 * \param f0
 *      The function value at the left position
 * \param f1
 *      The function value at the right position
 * \param d0
 *      The function derivative at the left position
 * \param d1
 *      The function derivative at the right position
 * \param t
 *      The parameter variable
 * \return
 *      The interpolated function value and
 *      its derivative at \c t
 */
template <typename Scalar>
std::pair<Scalar, Scalar> eval_spline_d(Scalar f0, Scalar f1, Scalar d0,
                                        Scalar d1, Scalar t) {
    Scalar t2 = t*t, t3 = t2*t;
    return std::make_pair(
        /* Function value */
        ( 2*t3 - 3*t2 + 1) * f0 +
        (-2*t3 + 3*t2)     * f1 +
        (   t3 - 2*t2 + t) * d0 +
        (   t3 - t2)       * d1,

        /* Derivative */
        ( 6*t2 - 6*t)      * f0 +
        (-6*t2 + 6*t)      * f1 +
        ( 3*t2 - 4*t + 1)  * d0 +
        ( 3*t2 - 2*t)      * d1
    );
}

/**
 * \brief Compute the definite integral and value of a cubic spline
 * that is parameterized by the function values and derivatives of
 * the interval <tt>[0, 1]</tt>.
 *
 * \param f0
 *      The function value at the left position
 * \param f1
 *      The function value at the right position
 * \param d0
 *      The function derivative at the left position
 * \param d1
 *      The function derivative at the right position
 * \return
 *      The definite integral and the interpolated
 *      function value at \c t
 */
template <typename Scalar>
std::pair<Scalar, Scalar> eval_spline_i(Scalar f0, Scalar f1, Scalar d0,
                                        Scalar d1, Scalar t) {
    Scalar t2 = t*t, t3 = t2*t, t4 = t2*t2;
    const Scalar H = Scalar(0.5f);
    const Scalar T = Scalar(1.f / 3.f);
    const Scalar Q = Scalar(0.25f);

    return std::make_pair(
        /* Definite integral */
        ( H*t4 - t3 + t)         * f0 +
        (-H*t4 + t3)             * f1 +
        ( Q*t4 - 2*T*t3 + H*t2)  * d0 +
        ( Q*t4 - T*t3)           * d1,

        /* Function value */
        ( 2*t3 - 3*t2 + 1)       * f0 +
        (-2*t3 + 3*t2)           * f1 +
        (   t3 - 2*t2 + t)       * d0 +
        (   t3 - t2)             * d1
    );
}

/**
 * \brief Evaluate a cubic spline interpolant of a \a uniformly sampled 1D function
 *
 * The implementation relies on Catmull-Rom splines, i.e. it uses finite
 * differences to approximate the derivatives at the endpoints of each spline
 * segment.
 *
 * \tparam Extrapolate
 *      Extrapolate values when \c x is out of range? (default: \c false)
 * \param min
 *      Position of the first node
 * \param max
 *      Position of the last node
 * \param values
 *      Array containing \c size regularly spaced evaluations in the range [\c
 *      min, \c max] of the approximated function.
 * \param size
 *      Denotes the size of the \c values array
 * \param x
 *      Evaluation point
 * \remark
 *      The Python API lacks the \c size parameter, which is inferred
 *      automatically from the size of the input array.
 * \remark
 *      The Python API provides a vectorized version which evaluates
 *      the function for many arguments \c x.
 * \return
 *      The interpolated value or zero when <tt>Extrapolate=false</tt>
 *      and \c x lies outside of [\c min, \c max]
 */
template <bool Extrapolate = false, typename Scalar, typename Float>
Scalar eval_1d(Float min, Float max, const Float *values,
               uint32_t size, Scalar x) {
    using Mask = mask_t<Scalar>;
    using Index = uint32_array_t<Scalar>;

    /* Give up when given an out-of-range or NaN argument */
    Mask mask_valid = (x >= min & x <= max);

    if (unlikely(!Extrapolate && none(mask_valid)))
        return zero<Scalar>();

    /* Transform 'x' so that nodes lie at integer positions */
    Scalar t = (x - min) * (Float(size - 1) / (max - min));

    /* Find the index of the left node in the queried subinterval */
    Index idx = enoki::max(Index(0), enoki::min(Index(t), Index(size - 2)));

    GET_SPLINE_UNIFORM(idx);

    /* Compute the relative position within the interval */
    t -= idx;

    if (!Extrapolate)
        return select(mask_valid, eval_spline(f0, f1, d0, d1, t), zero<Scalar>());
    else
        return eval_spline(f0, f1, d0, d1, t);
}

/**
 * \brief Evaluate a cubic spline interpolant of a \a non-uniformly sampled 1D function
 *
 * The implementation relies on Catmull-Rom splines, i.e. it uses finite
 * differences to approximate the derivatives at the endpoints of each spline
 * segment.
 *
 * \tparam Extrapolate
 *      Extrapolate values when \c x is out of range? (default: \c false)
 * \param nodes
 *      Array containing \c size non-uniformly spaced values denoting positions
 *      the where the function to be interpolated was evaluated. They must be
 *      provided in \a increasing order.
 * \param values
 *      Array containing function evaluations matched to the entries of \c
 *      nodes.
 * \param size
 *      Denotes the size of the \c nodes and \c values array
 * \param x
 *      Evaluation point
 * \remark
 *      The Python API lacks the \c size parameter, which is inferred
 *      automatically from the size of the input array
 * \remark
 *      The Python API provides a vectorized version which evaluates
 *      the function for many arguments \c x.
 * \return
 *      The interpolated value or zero when <tt>Extrapolate=false</tt>
 *      and \c x lies outside of \a [\c min, \c max]
 */
template <bool Extrapolate = false, typename Scalar, typename Float>
Scalar eval_1d(const Float *nodes, const Float *values,
               uint32_t size, Scalar x) {
    using Mask = mask_t<Scalar>;
    using Index = uint32_array_t<Scalar>;

    /* Give up when given an out-of-range or NaN argument */
    Mask mask_valid = (x >= nodes[0] & x <= nodes[size-1]);

    if (unlikely(!Extrapolate && none(mask_valid)))
        return zero<Scalar>();

    /* Find the index of the left node in the queried subinterval */
    Index idx = math::find_interval(size,
        [&](Index idx, Mask active) {
            return gather<Scalar>(nodes, idx, active) <= x;
        },
        mask_valid
    );

    GET_SPLINE_NONUNIFORM(idx);

    /* Compute the relative position within the interval */
    Scalar t = (x - x0) / width;

    if (!Extrapolate)
        return select(mask_valid, eval_spline(f0, f1, d0, d1, t), zero<Scalar>());
    else
        return eval_spline(f0, f1, d0, d1, t);
}

/**
 * \brief Computes a prefix sum of integrals over segments of a \a uniformly
 * sampled 1D Catmull-Rom spline interpolant
 *
 * This is useful for sampling spline segments as part of an importance
 * sampling scheme (in conjunction with \ref sample_1d)
 *
 * \param min
 *      Position of the first node
 * \param max
 *      Position of the last node
  * \param values
 *      Array containing \c size regularly spaced evaluations in the range
 *      [\c min, \c max] of the approximated function.
 * \param size
 *      Denotes the size of the \c values array
 * \param[out] out
 *      An array with \c size entries, which will be used to store the
 *      prefix sum
 * \remark
 *      The Python API lacks the \c size and \c out parameters. The former
 *      is inferred automatically from the size of the input array, and \c out
 *      is returned as a list.
 */
template <typename Scalar>
void integrate_1d(Scalar min, Scalar max, const Scalar *values,
                  uint32_t size, Scalar *out) {
    const Scalar width = (max - min) / (size - 1);
    Scalar sum = 0;
    store(out, 0);
    for (uint32_t idx = 0; idx < size - 1; ++idx) {
        GET_SPLINE_UNIFORM(idx);

        sum += ((d0 - d1) * (Scalar) (1.0 / 12.0) +
                (f0 + f1) * (Scalar) 0.5) * width;

        store(out + idx + 1, sum);
    }
}

/**
 * \brief Computes a prefix sum of integrals over segments of a \a non-uniformly
 * sampled 1D Catmull-Rom spline interpolant
 *
 * This is useful for sampling spline segments as part of an importance
 * sampling scheme (in conjunction with \ref sample_1d)
 *
 * \param nodes
 *      Array containing \c size non-uniformly spaced values denoting positions
 *      the where the function to be interpolated was evaluated. They must be
 *      provided in \a increasing order.
 * \param values
 *      Array containing function evaluations matched to the entries of
 *      \c nodes.
 * \param size
 *      Denotes the size of the \c values array
 * \param[out] out
 *      An array with \c size entries, which will be used to store the
 *      prefix sum
 * \remark
 *      The Python API lacks the \c size and \c out parameters. The former
 *      is inferred automatically from the size of the input array, and \c out
 *      is returned as a list.
 */
template <typename Scalar>
void integrate_1d(const Scalar *nodes, const Scalar *values,
                  uint32_t size, Scalar *out) {
    Scalar sum = 0;
    store(out, 0);
    for (uint32_t idx = 0; idx < size - 1; ++idx) {
        GET_SPLINE_NONUNIFORM(idx);

        sum += ((d0 - d1) * (Scalar) (1.0 / 12.0) +
                (f0 + f1) * (Scalar) 0.5) * width;

        store(out + idx + 1, sum);
    }
}

/**
 * \brief Invert a cubic spline interpolant of a \a uniformly sampled 1D function.
 * The spline interpolant must be <em>monotonically increasing</em>.
 *
 * \param min
 *      Position of the first node
 * \param max
 *      Position of the last node
 * \param values
 *      Array containing \c size regularly spaced evaluations in the range
 *      [\c min, \c max] of the approximated function.
 * \param size
 *      Denotes the size of the \c values array
 * \param y
 *      Input parameter for the inversion
 * \param eps
 *      Error tolerance (default: 1e-6f)
 * \return
 *      The spline parameter \c t such that <tt>eval_1d(..., t)=y</tt>
 */
template <typename Scalar, typename Float>
Scalar invert_1d(Float min, Float max, const Float *values, uint32_t size,
                 Scalar y, Float eps = 1e-6f) {
    using Mask = mask_t<Scalar>;
    using Index = uint32_array_t<Scalar>;

    /* Give up when given an out-of-range or NaN argument */
    Mask in_bounds_low  = y > values[0],
         in_bounds_high = y < values[size - 1],
         in_bounds      = in_bounds_low & in_bounds_high;

    /* Assuming that the lookup is out of bounds */
    Scalar out_of_bounds_value =
        select(in_bounds_high, Scalar(min), Scalar(max));

    if (unlikely(none(in_bounds)))
        return out_of_bounds_value;

    /* Map y to a spline interval by searching through the
       'values' array (which is assumed to be monotonic) */
    Index idx = math::find_interval(size,
        [&](Index idx, Mask active) {
            return gather<Scalar>(values, idx, active) <= y;
        },
        in_bounds
    );

    const Float width = Float(max - min) / (size - 1);
    GET_SPLINE_UNIFORM(idx);

    /* Invert the spline interpolant using Newton-Bisection */
    Scalar a = zero<Scalar>(), b = Scalar(1.f), t = Scalar(0.5f);

    /* Keep track all which lane is still active */
    Mask active(true);

    const Float eps_domain = eps,
                eps_value  = eps * values[size - 1];

    do {
        /* Fall back to a bisection step when t is out of bounds */
        t = select(!(t > a & t < b) & active, 0.5f * (a + b), t);

        /* Evaluate the spline and its derivative */
        Scalar value, deriv;
        std::tie(value, deriv) = eval_spline_d(f0, f1, d0, d1, t);
        value -= y;

        /* Update which lanes are still active */
        active &= (abs(value) > eps_value) & (b - a > eps_domain);

        /* Stop the iteration if converged */
        if (none_nested(active))
            break;

        /* Update the bisection bounds */
        a = select(value <= 0, t, a);
        b = select(value >  0, t, b);

        /* Perform a Newton step */
        t = select(active, t - value / deriv, t);
    } while (true);

    Scalar result = min + (idx + t) * width;

    return select(in_bounds, result, out_of_bounds_value);
}

/**
 * \brief Invert a cubic spline interpolant of a \a non-uniformly sampled 1D function.
 * The spline interpolant must be <em>monotonically increasing</em>.
 *
 * \param nodes
 *      Array containing \c size non-uniformly spaced values denoting positions
 *      the where the function to be interpolated was evaluated. They must be
 *      provided in \a increasing order.
 * \param values
 *      Array containing function evaluations matched to the entries of
 *      \c nodes.
 * \param size
 *      Denotes the size of the \c values array
 * \param y
 *      Input parameter for the inversion
 * \param eps
 *      Error tolerance (default: 1e-6f)
 * \return
 *      The spline parameter \c t such that <tt>eval_1d(..., t)=y</tt>
 */
template <typename Scalar, typename Float>
Scalar invert_1d(const Float *nodes, const Float *values, uint32_t size,
                 Scalar y, Float eps = 1e-6f) {
    using Mask = mask_t<Scalar>;
    using Index = uint32_array_t<Scalar>;

    /* Give up when given an out-of-range or NaN argument */
    Mask in_bounds_low  = y > values[0],
         in_bounds_high = y < values[size - 1],
         in_bounds      = in_bounds_low & in_bounds_high;

    /* Assuming that the lookup is out of bounds */
    Scalar out_of_bounds_value =
        select(in_bounds_high, Scalar(nodes[0]), Scalar(nodes[size - 1]));

    if (unlikely(none(in_bounds)))
        return out_of_bounds_value;

    Scalar result;

    /* Map y to a spline interval by searching through the
       'values' array (which is assumed to be monotonic) */
    Index idx = math::find_interval(size,
        [&](Index idx, Mask active) {
            return gather<Scalar>(values, idx, active) <= y;
        },
        in_bounds
    );

    GET_SPLINE_NONUNIFORM(idx);

    /* Invert the spline interpolant using Newton-Bisection */
    Scalar a = Scalar(0), b = Scalar(1), t = Scalar(0.5f);
    Scalar value, deriv;

    /* Keep track all which lane is still active */
    Mask active(true);

    const Float eps_domain = eps,
                eps_value  = eps * values[size - 1];
    do {
        /* Fall back to a bisection step when t is out of bounds */
        t = select(!(t > a & t < b) & active, 0.5f * (a + b), t);

        /* Evaluate the spline and its derivative */
        std::tie(value, deriv) = eval_spline_d(f0, f1, d0, d1, t);
        value -= y;

        /* Update which lanes are still active */
        active &= (abs(value) > eps_value) & (b - a > eps_domain);

        /* Stop the iteration if converged */
        if (none_nested(active))
            break;

        /* Update the bisection bounds */
        a = select(value <= 0, t, a);
        b = select(value >  0, t, b);

        /* Perform a Newton step */
        t = select(active, t - value / deriv, t);
    } while (true);

    result = x0 + t * width;

    return select(in_bounds, result, out_of_bounds_value);
}

/**
 * \brief Importance sample a segment of a \a uniformly sampled 1D Catmull-Rom
 * spline interpolant
 *
 * \param min
 *      Position of the first node
 * \param max
 *      Position of the last node
 * \param values
 *      Array containing \c size regularly spaced evaluations in the range [\c
 *      min, \c max] of the approximated function.
 * \param cdf
 *      Array containing a cumulative distribution function computed by \ref
 *      integrate_1d().
 * \param size
 *      Denotes the size of the \c values array
 * \param sample
 *      A uniformly distributed random sample in the interval <tt>[0,1]</tt>
 * \param eps
 *      Error tolerance (default: 1e-6f)
 * \return
 *      1. The sampled position
 *      2. The value of the spline evaluated at the sampled position
 *      3. The probability density at the sampled position (which only differs
 *         from item 2. when the function does not integrate to one)
 */
template <typename Scalar, typename Float>
std::tuple<Scalar, Scalar, Scalar>
sample_1d(Float min, Float max, const Float *values, const Float *cdf,
                 uint32_t size, Scalar sample, Float eps = 1e-6f) {
    using Mask = mask_t<Scalar>;
    using Index = uint32_array_t<Scalar>;

    const Float full_width = max - min,
                width      = full_width / (size - 1),
                inv_width  = (size - 1) / full_width,
                last       = cdf[size - 1],
                eps_domain = eps * full_width,
                eps_value  = eps * last,
                last_rcp   = (Float) 1 / last;

    /* Scale by the definite integral of the function (in case
       it is not normalized) */
    sample *= last;

    /* Map y to a spline interval by searching through the
       monotonic 'cdf' array */
    Index idx = math::find_interval(size,
        [&](Index idx, Mask active) {
            return gather<Scalar>(cdf, idx, active) <= sample;
        },
        Mask(true)
    );

    GET_SPLINE_UNIFORM(idx);

    // Re-scale the sample after having choosen the interval
    sample = (sample - gather<Scalar>(cdf, idx)) * inv_width;

    /* Importance sample linear interpolant as initial guess for 't'*/
    Scalar t_linear =
        (f0 - safe_sqrt(f0 * f0 + 2 * sample * (f1 - f0))) / (f0 - f1);
    Scalar t_const  = sample / f0;
    Scalar t = select(neq(f0, f1), t_linear, t_const);

    Scalar a = 0, b = 1, value, deriv;
    Mask active(true);
    do {
        /* Fall back to a bisection step when t is out of bounds */
        t = select(!(t > a & t < b) & active, 0.5f * (a + b), t);

        /* Evaluate the definite integral and its derivative
           (i.e. the spline) */
        std::tie(value, deriv) = eval_spline_i(f0, f1, d0, d1, t);
        value -= sample;

        /* Update which lanes are still active */
        active &= (abs(value) > eps_value) & (b - a > eps_domain);

        /* Stop the iteration if converged */
        if (none_nested(active))
            break;

        /* Update the bisection bounds */
        a = select(value <= 0, t, a);
        b = select(value >  0, t, b);

        /* Perform a Newton step */
        t = select(active, t - value / deriv, t);
    } while (true);

    return std::make_tuple(
        min + (idx + t) * width,
        deriv,
        deriv * last_rcp);
}

/**
 * \brief Importance sample a segment of a \a non-uniformly sampled 1D Catmull-Rom
 * spline interpolant
 *
 * \param nodes
 *      Array containing \c size non-uniformly spaced values denoting positions
 *      the where the function to be interpolated was evaluated. They must be
 *      provided in \a increasing order.
 * \param values
 *      Array containing function evaluations matched to the entries of \c
 *      nodes.
 * \param cdf
 *      Array containing a cumulative distribution function computed by \ref
 *      integrate_1d().
 * \param size
 *      Denotes the size of the \c values array
 * \param sample
 *      A uniformly distributed random sample in the interval <tt>[0,1]</tt>
 * \param eps
 *      Error tolerance (default: 1e-6f)
 * \return
 *      1. The sampled position
 *      2. The value of the spline evaluated at the sampled position
 *      3. The probability density at the sampled position (which only differs
 *         from item 2. when the function does not integrate to one)
 */
template <typename Scalar, typename Float>
std::tuple<Scalar, Scalar, Scalar>
sample_1d(const Float *nodes, const Float *values, const Float *cdf,
          uint32_t size, Scalar sample, Float eps = 1e-6f) {
    using Mask = mask_t<Scalar>;
    using Index = uint32_array_t<Scalar>;

    const Float last = cdf[size - 1],
                eps_domain = eps * (nodes[size - 1] - nodes[0]),
                eps_value  = eps * last,
                last_rcp   = (Float) 1 / last;

    /* Scale by the definite integral of the function (in case
       it is not normalized) */
    sample *= last;

    /* Map y to a spline interval by searching through the
       monotonic 'cdf' array */
    Index idx = math::find_interval(size,
        [&](Index idx, Mask active) {
            return gather<Scalar>(cdf, idx, active) <= sample;
        },
        Mask(true)
    );

    GET_SPLINE_NONUNIFORM(idx);

    // Re-scale the sample after having choosen the interval
    sample = (sample - gather<Scalar>(cdf, idx)) / width;

    /* Importance sample linear interpolant as initial guess for 't'*/
    Scalar t_linear =
        (f0 - safe_sqrt(f0 * f0 + 2 * sample * (f1 - f0))) / (f0 - f1);
    Scalar t_const  = sample / f0;
    Scalar t = select(neq(f0, f1), t_linear, t_const);

    Scalar a = 0, b = 1, value, deriv;
    Mask active(true);
    do {
        /* Fall back to a bisection step when t is out of bounds */
        t = select(!(t > a & t < b) & active, 0.5f * (a + b), t);

        /* Evaluate the definite integral and its derivative
           (i.e. the spline) */
        std::tie(value, deriv) = eval_spline_i(f0, f1, d0, d1, t);
        value -= sample;

        /* Update which lanes are still active */
        active &= (abs(value) > eps_value) & (b - a > eps_domain);

        /* Stop the iteration if converged */
        if (none_nested(active))
            break;

        /* Update the bisection bounds */
        a = select(value <= 0, t, a);
        b = select(value >  0, t, b);

        /* Perform a Newton step */
        t = select(active, t - value / deriv, t);
    } while (true);

    /* Return the value and PDF if requested */

    return std::make_tuple(
        x0 + width * t,
        deriv,
        deriv * last_rcp);
}

/**
 * \brief Compute weights to perform a spline-interpolated lookup on a
 * \a uniformly sampled 1D function.
 *
 * The implementation relies on Catmull-Rom splines, i.e. it uses finite
 * differences to approximate the derivatives at the endpoints of each spline
 * segment. The resulting weights are identical those internally used by \ref
 * sample_1d().
 *
 * \tparam Extrapolate
 *      Extrapolate values when \c x is out of range? (default: \c false)
 * \param min
 *      Position of the first node
 * \param max
 *      Position of the last node
 * \param size
 *      Denotes the number of function samples
 * \param x
 *      Evaluation point
 * \param[out] weights
 *      Pointer to a weight array of size 4 that will be populated
 * \remark
 *      In the Python API, the \c offset and \c weights parameters are returned
 *      as the second and third elements of a triple.
 * \return
 *      A boolean set to \c true on success and \c false when <tt>Extrapolate=false</tt>
 *      and \c x lies outside of [\c min, \c max] and an offset into the function samples
 *      associated with weights[0]
 */
template <bool Extrapolate = false,
          typename Scalar, typename Float,
          typename Int32 = int32_array_t<Scalar>,
          typename Mask = mask_t<Scalar>>
std::pair<Mask, Int32> eval_spline_weights(Float min, Float max, uint32_t size,
                                           Scalar x, Scalar *weights) {
    using Index = uint32_array_t<Scalar>;

    /* Give up when given an out-of-range or NaN argument */
    auto mask_valid = x >= min & x <= max;

    if (unlikely(!Extrapolate && none(mask_valid)))
        return std::make_pair(Mask(false), zero<Int32>());

    /* Transform 'x' so that nodes lie at integer positions */
    Scalar t = (x - min) * (Float(size - 1) / (max - min));

    /* Find the index of the left node in the queried subinterval */
    Index idx = enoki::max(Index(0), enoki::min(Index(t), Index(size - 2)));

    /* Compute the relative position within the interval */
    t -= (Scalar)(idx);
    Scalar t2 = t * t,
           t3 = t2 * t,
           w0, w1, w2, w3;

    /* Function value weights */
    w0 = zero<Scalar>();
    w1 =  2 * t3 - 3 * t2 + 1;
    w2 = -2 * t3 + 3 * t2;
    w3 = zero<Scalar>();
    Int32 offset = (Int32) idx - 1;

    /* Turn derivative weights into node weights using
       an appropriate chosen finite differences stencil */
    Scalar d0 = t3 - 2*t2 + t, d1 = t3 - t2;

    auto valid_boundary_left = idx > 0;
    w0 = select(valid_boundary_left, w0 - d0 * 0.5f, w0);
    w1 = select(valid_boundary_left, w1, w1 - d0);
    w2 = select(valid_boundary_left, w2 + d0 * 0.5f, w2 + d0);

    auto valid_boundary_right = idx + 2 < size;
    w1 = select(valid_boundary_right, w1 - d1 * 0.5f, w1 - d1);
    w2 = select(valid_boundary_right, w2, w2 + d1);
    w3 = select(valid_boundary_right, w3 + d1 * 0.5f, w3);

    store(weights, w0);
    store(weights + 1, w1);
    store(weights + 2, w2);
    store(weights + 3, w3);

    if (!Extrapolate)
        return std::make_pair(mask_valid, offset);
    else
        return std::make_pair(Mask(true), offset);
}

/**
 * \brief Compute weights to perform a spline-interpolated lookup on a
 * \a non-uniformly sampled 1D function.
 *
 * The implementation relies on Catmull-Rom splines, i.e. it uses finite
 * differences to approximate the derivatives at the endpoints of each spline
 * segment. The resulting weights are identical those internally used by \ref
 * sample_1d().
 *
 * \tparam Extrapolate
 *      Extrapolate values when \c x is out of range? (default: \c false)
 * \param nodes
 *      Array containing \c size non-uniformly spaced values denoting positions
 *      the where the function to be interpolated was evaluated. They must be
 *      provided in \a increasing order.
 * \param size
 *      Denotes the size of the \c nodes array
 * \param x
 *      Evaluation point
 * \param[out] weights
 *      Pointer to a weight array of size 4 that will be populated
 * \remark
 *      The Python API lacks the \c size parameter, which is inferred
 *      automatically from the size of the input array. The \c offset
 *      and \c weights parameters are returned as the second and third
 *      elements of a triple.
 * \return
 *      A boolean set to \c true on success and \c false when <tt>Extrapolate=false</tt>
 *      and \c x lies outside of [\c min, \c max] and an offset into the function samples
 *      associated with weights[0]
 */
template <bool Extrapolate = false,
          typename Scalar,
          typename Float,
          typename Int32 = int32_array_t<Scalar>,
          typename Mask = mask_t<Scalar>>
std::pair<Mask, Int32> eval_spline_weights(const Float* nodes, uint32_t size,
                                           Scalar x, Scalar *weights) {
    using Index = uint32_array_t<Scalar>;

    /* Give up when given an out-of-range or NaN argument */
    Mask mask_valid = (x >= nodes[0] && x <= nodes[size-1]);

    if (unlikely(!Extrapolate && none(mask_valid)))
        return std::make_pair(Mask(false), zero<Int32>());

    /* Find the index of the left node in the queried subinterval */
    Index idx = math::find_interval(size,
        [&](Index idx, Mask active) {
            return gather<Scalar>(nodes, idx, active) <= x;
        },
        mask_valid
    );

    Scalar x0 = gather<Scalar>(nodes, idx),
           x1 = gather<Scalar>(nodes, idx + 1),
           width = x1 - x0;

    /* Compute the relative position within the interval and powers of 't' */
    Scalar t  = (x - x0) / width,
           t2 = t * t,
           t3 = t2 * t,
           w0, w1, w2, w3;

    /* Function value weights */
    w0 = zero<Scalar>();
    w1 = 2*t3 - 3*t2 + 1;
    w2 = -2*t3 + 3*t2;
    w3 = zero<Scalar>();

    Int32 offset = (Int32) idx - 1;

    /* Turn derivative weights into node weights using
       an appropriate chosen finite differences stencil */
    Scalar d0 = t3 - 2*t2 + t, d1 = t3 - t2;

    auto valide_boundary_left = idx > 0;
    Scalar width_nodes_1 =
        gather<Scalar>(nodes, idx + 1) -
        gather<Scalar>(nodes, idx - 1, valide_boundary_left);

    Scalar factor = width / width_nodes_1;
    w0 = select(valide_boundary_left, w0 - (d0 * factor), w0);
    w1 = select(valide_boundary_left, w1, w1 - d0);
    w2 = select(valide_boundary_left, w2 + d0 * factor, w2 + d0);

    auto valid_boundary_right = idx + 2 < size;
    Scalar width_nodes_2 =
        gather<Scalar>(nodes, idx + 2, valid_boundary_right) -
        gather<Scalar>(nodes, idx);

    factor = width / width_nodes_2;
    w1 = select(valid_boundary_right, w1 - (d1 * factor), w1 - d1);
    w2 = select(valid_boundary_right, w2, w2 + d1);
    w3 = select(valid_boundary_right, w3 + d1 * factor, w3);

    store(weights,     w0);
    store(weights + 1, w1);
    store(weights + 2, w2);
    store(weights + 3, w3);

    if (!Extrapolate)
        return std::make_pair(mask_valid, offset);
    else
        return std::make_pair(Mask(true), offset);
}

/**
 * \brief Evaluate a cubic spline interpolant of a uniformly sampled 2D function
 *
 * This implementation relies on a tensor product of Catmull-Rom splines, i.e.
 * it uses finite differences to approximate the derivatives for each dimension
 * at the endpoints of spline patches.
 *
 * \tparam Extrapolate
 *      Extrapolate values when \c p is out of range? (default: \c false)
 * \param nodes1
 *      Arrays containing \c size1 non-uniformly spaced values denoting
 *      positions the where the function to be interpolated was evaluated
 *      on the \c X axis (in increasing order)
 * \param size1
 *      Denotes the size of the \c nodes1 array
 * \param nodes
 *      Arrays containing \c size2 non-uniformly spaced values denoting
 *      positions the where the function to be interpolated was evaluated
 *      on the \c Y axis (in increasing order)
 * \param size2
 *      Denotes the size of the \c nodes2 array
 * \param values
 *      A 2D floating point array of <tt>size1*size2</tt> cells containing
 *      irregularly spaced evaluations of the function to be interpolated.
 *      Consecutive entries of this array correspond to increments in the \c X
 *      coordinate.
 * \param x
 *      \c X coordinate of the evaluation point
 * \param y
 *      \c Y coordinate of the evaluation point
 * \remark
 *      The Python API lacks the \c size1 and \c size2 parameters, which are
 *      inferred automatically from the size of the input arrays.
 * \return
 *      The interpolated value or zero when <tt>Extrapolate=false</tt>tt> and
 *      <tt>(x,y)</tt> lies outside of the node range
 */
template <bool Extrapolate = false, typename Scalar, typename Float>
Scalar eval_2d(const Float *nodes1, uint32_t size1, const Float *nodes2,
               uint32_t size2, const Float *values, Scalar x, Scalar y) {
    using Mask = mask_t<Scalar>;
    using Index = int32_array_t<Scalar>;

    Scalar weights[2][4];
    Index offset[2];
    Mask valid_x, valid_y;

    std::tie(valid_x, offset[0]) =
        eval_spline_weights<Extrapolate>(nodes1, size1, x, weights[0]);
    std::tie(valid_y, offset[1]) =
        eval_spline_weights<Extrapolate>(nodes2, size2, y, weights[1]);

    /* Compute interpolation weights separately for each dimension */
    if (unlikely(none(valid_x & valid_y)))
        return zero<Scalar>();

    Index index = offset[1] * size1 + offset[0];
    Scalar result = 0.f;

    for (int yi = 0; yi < 4; ++yi) {
        Scalar weight_y = weights[1][yi];

        for (int xi = 0; xi < 4; ++xi) {
            Scalar weight_x  = weights[0][xi];
            Scalar weight_xy = weight_x * weight_y;

            Mask weight_valid = neq(weight_xy, zero<Scalar>());
            Scalar value = gather<Scalar>(values, index, weight_valid);

            result = fmadd(value, weight_xy, result);
            index += 1;
        }

        index += size1 - 4;
    }

    return result;
}

// =======================================================================
/*! @} */

NAMESPACE_END(spline)
NAMESPACE_END(mitsuba)
