#pragma once

#include <drjit/math.h>
#include <drjit/util.h>
#include <drjit/array_constants.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/simd.h>
#include <mitsuba/core/traits.h>

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(math)

// -----------------------------------------------------------------------
//! @{ \name Useful constants in various precisions
// -----------------------------------------------------------------------

#if (MI_ENABLE_EMBREE)
template <typename T> constexpr auto RayEpsilon = dr::Epsilon<dr::float32_array_t<T>> * 1500;
#else
template <typename T> constexpr auto RayEpsilon = dr::Epsilon<T> * 1500;
#endif
template <typename T> constexpr auto ShadowEpsilon = RayEpsilon<T> * 10;

//! @}
// -----------------------------------------------------------------------

// -----------------------------------------------------------------------
//! @{ \name Legendre functions
// -----------------------------------------------------------------------

/// Evaluate the l-th Legendre polynomial using recurrence
template <typename Value>
Value legendre_p(int l, Value x) {
    using Scalar = dr::scalar_t<Value>;
    Value l_cur = Value(0.f);

    assert(l >= 0);

    if (likely(l > 1)) {
        Value l_p_pred = Scalar(1), l_pred = x;
        Value k0 = Scalar(3), k1 = Scalar(2), k2 = Scalar(1);

        for (int ki = 2; ki <= l; ++ki) {
            l_cur = (k0 * x * l_pred - k2  * l_p_pred) / k1;
            l_p_pred = l_pred; l_pred = l_cur;
            k2 = k1; k0 += Scalar(2); k1 += Scalar(1);
        }

        return l_cur;
    } else {
        l_cur = (l == 0) ? 1 : x;
    }
    return l_cur;
}

/// Evaluate an associated Legendre polynomial using recurrence
template <typename Value>
Value legendre_p(int l, int m, Value x) {
    using Scalar = dr::scalar_t<Value>;

    Value p_mm = Scalar(1);

    if (likely(m > 0)) {
        Value somx2 = dr::sqrt((Scalar(1) - x) * (Scalar(1) + x));
        Value fact = Scalar(1);
        for (int i = 1; i <= m; i++) {
            p_mm *= (-fact) * somx2;
            fact += Scalar(2);
        }
    }

    if (unlikely(l == m))
        return p_mm;

    Value p_mmp1 = x * (Scalar(2) * m + Scalar(1)) * p_mm;
    if (unlikely(l == m + 1))
        return p_mmp1;

    Value p_ll = Scalar(0);
    for (int ll = m + 2; ll <= l; ++ll) {
        p_ll = ((Scalar(2) * ll - Scalar(1)) * x * p_mmp1 -
                (ll + m - Scalar(1)) * p_mm) / (ll - m);
        p_mm = p_mmp1;
        p_mmp1 = p_ll;
    }

    return p_ll;
}

/// Evaluate the l-th Legendre polynomial and its derivative using recurrence
template <typename Value>
std::pair<Value, Value> legendre_pd(int l, Value x) {
    using Scalar = dr::scalar_t<Value>;

    assert(l >= 0);
    Value l_cur = Scalar(0), d_cur = Scalar(0);

    if (likely(l > 1)) {
        Value l_p_pred = Scalar(1), l_pred = x,
              d_p_pred = Scalar(0), d_pred = Scalar(1);
        Scalar k0 = Scalar(3), k1 = Scalar(2), k2 = Scalar(1);

        for (int ki = 2; ki <= l; ++ki) {
            l_cur = (k0 * x * l_pred - k2 * l_p_pred) / k1;
            d_cur = d_p_pred + k0 * l_pred;
            l_p_pred = l_pred; l_pred = l_cur;
            d_p_pred = d_pred; d_pred = d_cur;
            k2 = k1; k0 += Scalar(2); k1 += Scalar(1);
        }
    } else {
        if (l == 0) {
            l_cur = Scalar(1); d_cur = Scalar(0);
        } else {
            l_cur = x; d_cur = Scalar(1);
        }
    }

    return { l_cur, d_cur };
}

/// Evaluate the function legendre_pd(l+1, x) - legendre_pd(l-1, x)
template <typename Value>
std::pair<Value, Value> legendre_pd_diff(int l, Value x) {
    using Scalar = dr::scalar_t<Value>;
    assert(l >= 1);

    if (likely(l > 1)) {
        Value l_p_pred = Scalar(1), l_pred = x, l_cur = Scalar(0),
              d_p_pred = Scalar(0), d_pred = Scalar(1), d_cur = Scalar(0);
        Scalar k0 = Scalar(3), k1 = Scalar(2), k2 = Scalar(1);

        for (int ki = 2; ki <= l; ++ki) {
            l_cur = (k0 * x * l_pred - k2 * l_p_pred) / k1;
            d_cur = d_p_pred + k0 * l_pred;
            l_p_pred = l_pred; l_pred = l_cur;
            d_p_pred = d_pred; d_pred = d_cur;
            k2 = k1; k0 += Scalar(2); k1 += Scalar(1);
        }

        Value l_next = (k0 * x * l_pred - k2 * l_p_pred) / k1;
        Value d_next = d_p_pred + k0 * l_pred;

        return { l_next - l_p_pred, d_next - d_p_pred };
    } else {
        return { Scalar(0.5) * (Scalar(3) * x * x - Scalar(1)) - Scalar(1),
                 Scalar(3) * x };
    }
}

//! @}
// -----------------------------------------------------------------------

// -----------------------------------------------------------------------
//! @{ \name Miscellaneous mathematical helper functions
// -----------------------------------------------------------------------

/**
 * \brief Compare the difference in ULPs between a reference value and another
 * given floating point number
 */
template <typename T> T ulpdiff(T ref, T val) {
    constexpr T eps = dr::Epsilon<T> / 2;

    // Express mantissa wrt. same exponent
    auto [m_ref, e_ref] = dr::frexp(ref);
    auto [m_val, e_val] = dr::frexp(val);

    T diff;
    if (e_ref == e_val)
        diff = m_ref - m_val;
    else
        diff = m_ref - dr::ldexp(m_val, e_val - e_ref);

    return dr::abs(diff) / eps;
}

/// Always-positive modulo function
template <typename T> T modulo(T a, T b) {
    T result = a - (a / b) * b;
    return dr::select(result < 0, result + b, result);
}

/// Check whether the provided integer is a power of two
template <typename T> bool is_power_of_two(T i) {
    return i > 0 && (i & (i-1)) == 0;
}

/// Round an unsigned integer to the next integer power of two
template <typename T> T round_to_power_of_two(T i) {
    if (i <= 1)
        return 1;
    return T(1) << (dr::log2i(i - 1) + 1);
}

/// Ceiling of base-2 logarithm
template <typename T> T log2i_ceil(T value) {
    T result = 8 * sizeof(dr::scalar_t<T>) - 1u - dr::lzcnt(value);
    dr::masked(result, dr::neq(value & (value - 1u), 0u)) += 1u;
    return result;
}

/**
 * \brief Find an interval in an ordered set
 *
 * This function performs a binary search to find an index \c i such that
 * <tt>pred(i)</tt> is \c true and <tt>pred(i+1)</tt> is \c false, where \c pred
 * is a user-specified predicate that monotonically decreases over this range
 * (i.e. max one \c true -> \c false transition).
 *
 * The predicate will be evaluated exactly <tt>floor(log2(size)) + 1<tt> times.
 * Note that the template parameter \c Index is automatically inferred from the
 * supplied predicate, which takes an index or an index vector of type \c Index
 * as input argument and can (optionally) take a mask argument as well. In the
 * vectorized case, each vector lane can use different predicate.
 * When \c pred is \c false for all entries, the function returns \c 0, and
 * when it is \c true for all cases, it returns <tt>size-2<tt>.
 *
 * The main use case of this function is to locate an interval (i, i+1)
 * in an ordered list.
 *
 * \code
 * float my_list[] = { 1, 1.5f, 4.f, ... };
 *
 * UInt32 index = find_interval(
 *     sizeof(my_list) / sizeof(float),
 *     [](UInt32 index, dr::mask_t<UInt32> active) {
 *         return dr::gather<Float>(my_list, index, active) < x;
 *     }
 * );
 * \endcode
 */
template <typename Index, typename Predicate>
MI_INLINE Index find_interval(dr::scalar_t<Index> size,
                               const Predicate &pred) {
    return dr::binary_search<Index>(1, size - 1, pred) - 1;
}

/**
 * \brief This function computes a suitable middle point for use in the \ref bisect() function
 * 
 * To mitigate the issue of varying density of floating point numbers on the
 * number line, the floats are reinterpreted as unsigned integers. As long as
 * sign of both numbers is the same, this maps the floats to the evenly spaced
 * set of integers. The middle of these integers ensures that the space of 
 * numbers is halved on each iteration of the bisection.
 *
 * Note that this strategy does not work if the numbers have different sign.
 * In this case the function always returns 0.0 as the middle.
 */
template <typename Scalar>
Scalar middle(Scalar left, Scalar right) {
    using ScalarUInt = dr::uint_array_t<Scalar>;

    // Propagate invalid values (infinities, NaN) back to the caller
    if (!dr::isfinite(left) || !dr::isfinite(right))
        return left + right;

    // always return zero if left and right have different signs
    if (dr::sign(left) != dr::sign(right) && left != Scalar(0.0) && right != Scalar(0.0))
        return Scalar(0.0);

    // we reinterpret as UInt using the absolute value, so we store the sign
    // to reapply after interpreting the result back to Float
    bool negate = left < Scalar(0.0) || right < Scalar(0.0);
    left = dr::abs(left);
    right = dr::abs(right);
    ScalarUInt left_int = dr::reinterpret_array<ScalarUInt>(left);
    ScalarUInt right_int = dr::reinterpret_array<ScalarUInt>(right);
    ScalarUInt mid_int = (left_int + right_int) >> 1;
    Scalar mid = dr::reinterpret_array<Scalar>(mid_int);
    return negate ? -mid : mid;
}

/**
 * \brief Bisect a floating point interval given a predicate function
 *
 * This function takes an interval [\c left, \c right] and a predicate \c pred
 * as inputs. It assumes that <tt>pred(left)==true</tt> and
 * <tt>pred(right)==false</tt>. It also assumes that there is a single floating
 * point number \c t such that \c pred is \c true for all values in the range
 * [\c left, \c t] and \c false for all values in the range (\c t, \c right].
 *
 * The bisection search then finds and returns \c t by repeatedly splitting the
 * input interval. The number of iterations is roughly bounded by the number of
 * bits of the underlying floating point representation.
 */


template <typename Scalar, typename Predicate>
Scalar bisect(Scalar left, Scalar right, const Predicate &pred) {
    Scalar mid = middle(left, right);

    while (left < mid && mid < right) {
        if (pred(mid))
            left = mid;
        else
            right = mid;
        mid = middle(left, right);
    }
    return left;
}

/**
 * \brief Compute the Chi^2 statistic and degrees of freedom of the given
 * arrays while pooling low-valued entries together
 *
 * Given a list of observations counts (``obs[i]``) and expected observation
 * counts (``exp[i]``), this function accumulates the Chi^2 statistic, that is,
 * ``(obs-exp)^2 / exp`` for each element ``0, ..., n-1``.
 *
 * Minimum expected cell frequency. The Chi^2 test statistic is not useful when
 * when the expected frequency in a cell is low (e.g. less than 5), because
 * normality assumptions break down in this case. Therefore, the implementation
 * will merge such low-frequency cells when they fall below the threshold
 * specified here. Specifically, low-valued cells with ``exp[i] < pool_threshold``
 * are pooled into larger groups that are above the threshold before their
 * contents are added to the Chi^2 statistic.
 *
 * The function returns the statistic value, degrees of freedom, below-threshold
 * entries and resulting number of pooled regions.
 *
 */
template <typename Scalar> std::tuple<Scalar, size_t, size_t, size_t>
chi2(const Scalar *obs, const Scalar *exp, Scalar pool_threshold, size_t n) {
    Scalar chsq = 0, pooled_obs = 0, pooled_exp = 0;
    size_t dof = 0, n_pooled_in = 0, n_pooled_out = 0;

    for (size_t i = 0; i<n; ++i) {
        if (exp[i] == 0 && obs[i] == 0)
            continue;

        if (exp[i] < pool_threshold) {
            pooled_obs += obs[i];
            pooled_exp += exp[i];
            n_pooled_in++;

            if (pooled_exp > pool_threshold) {
                Scalar diff = pooled_obs - pooled_exp;
                chsq += (diff*diff) / pooled_exp;
                pooled_obs = pooled_exp = 0;
                ++n_pooled_out; ++dof;
            }
        } else {
            Scalar diff = obs[i] - exp[i];
            chsq += (diff*diff) / exp[i];
            ++dof;
        }
    }

    return { chsq, dof - 1, n_pooled_in, n_pooled_out };
}

/**
 * \brief Solve a quadratic equation of the form a*x^2 + b*x + c = 0.
 * \return \c true if a solution could be found
 */
template <typename Value>
MI_INLINE std::tuple<dr::mask_t<Value>, Value, Value>
solve_quadratic(const Value &a, const Value &b, const Value &c) {
    using Scalar = dr::scalar_t<Value>;
    using Mask = dr::mask_t<Value>;

    /* Is this perhaps a linear equation? */
    Mask linear_case = dr::eq(a, Scalar(0));

    /* If so, we require b != 0 */
    Mask valid_linear = linear_case && dr::neq(b, Scalar(0));

    /* Initialize solution with that of linear equation */
    Value x0, x1;
    x0 = x1 = -c / b;

    /* Check if the quadratic equation is solvable */
    Value discrim = dr::fmsub(b, b, Scalar(4) * a * c);
    Mask valid_quadratic = !linear_case && (discrim >= Scalar(0));

    if (likely(dr::any_or<true>(valid_quadratic))) {
        Value sqrt_discrim = dr::sqrt(discrim);

        /* Numerically stable version of (-b (+/-) sqrt_discrim) / (2 * a)
         *
         * Based on the observation that one solution is always
         * accurate while the other is not. Finds the solution of
         * greater magnitude which does not suffer from loss of
         * precision and then uses the identity x1 * x2 = c / a
         */
        Value temp = -Scalar(0.5) * (b + dr::copysign(sqrt_discrim, b));

        Value x0p = temp / a,
              x1p = c / temp;

        /* Order the results so that x0 < x1 */
        Value x0m = dr::minimum(x0p, x1p),
              x1m = dr::maximum(x0p, x1p);

        x0 = dr::select(linear_case, x0, x0m);
        x1 = dr::select(linear_case, x0, x1m);
    }

    return { valid_linear || valid_quadratic, x0, x1 };
}

//! @}
// -----------------------------------------------------------------------

template <typename Array, size_t... Index, typename Value = dr::value_t<Array>>
DRJIT_INLINE Array sample_shifted(const Value &sample, std::index_sequence<Index...>) {
    const Array shift(Index / dr::scalar_t<Array>(Array::Size)...);

    Array value = Array(sample) + shift;
    value[value > Value(1)] -= Value(1);

    return value;
}

/**
 * \brief Map a uniformly distributed sample to an array of samples with shifts
 *
 * Given a floating point value \c x on the interval <tt>[0, 1]</tt> return a
 * floating point array with values <tt>[x, x+offset, x+2*offset, ...]</tt>,
 * where \c offset is the reciprocal of the array size. Entries that become
 * greater than 1.0 wrap around to the other side of the unit interval.
 *
 * This operation is useful to implement a type of correlated stratification in
 * the context of Monte Carlo integration.
 */
template <typename Array> Array sample_shifted(const dr::value_t<Array> &sample) {
    return sample_shifted<Array>(
        sample, std::make_index_sequence<Array::Size>());
}

NAMESPACE_END(math)
NAMESPACE_END(mitsuba)
