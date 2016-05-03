#pragma once

#include <mitsuba/mitsuba.h>
#include <cmath>
#include <algorithm>

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(math)

// -----------------------------------------------------------------------
//! @{ \name Useful constants in various precisions
// -----------------------------------------------------------------------

static const double E_d               = 2.71828182845904523536;
static const double Pi_d              = 3.14159265358979323846;
static const double InvPi_d           = 0.31830988618379067154;
static const double InvTwoPi_d        = 0.15915494309189533577;
static const double InvFourPi_d       = 0.07957747154594766788;
static const double SqrtPi_d          = 1.77245385090551602793;
static const double InvSqrtPi_d       = 0.56418958354775628695;
static const double SqrtTwo_d         = 1.41421356237309504880;
static const double InvSqrtTwo_d      = 0.70710678118654752440;
static const double SqrtTwoPi_d       = 2.50662827463100050242;
static const double InvSqrtTwoPi_d    = 0.39894228040143267794;
static const double Epsilon_d         = 1e-7;
#if !defined(_MSC_VER)
static const double OneMinusEpsilon_d = 0x1.fffffffffffff7p-1;
static const double RecipOverflow_d   = 0x1p-1024;
#else
static const double OneMinusEpsilon_d = 0.999999999999999888;
static const double RecipOverflow_d   = 5.56268464626800345e-309;
#endif
static const double Infinity_d        = std::numeric_limits<double>::infinity();
static const double MaxFloat_d        = std::numeric_limits<double>::max();
static const double MachineEpsilon_d  = std::numeric_limits<double>::epsilon() / 2;

static const float  E_f               = (float) E_d;
static const float  Pi_f              = (float) Pi_d;
static const float  InvPi_f           = (float) InvPi_d;
static const float  InvTwoPi_f        = (float) InvTwoPi_d;
static const float  InvFourPi_f       = (float) InvFourPi_d;
static const float  SqrtPi_f          = (float) SqrtPi_d;
static const float  InvSqrtPi_f       = (float) InvSqrtPi_d;
static const float  SqrtTwo_f         = (float) SqrtTwo_d;
static const float  InvSqrtTwo_f      = (float) InvSqrtTwo_d;
static const float  SqrtTwoPi_f       = (float) SqrtTwoPi_d;
static const float  InvSqrtTwoPi_f    = (float) InvSqrtTwoPi_d;
static const float  Epsilon_f         = 1e-4f;
#if !defined(_MSC_VER)
static const float  OneMinusEpsilon_f = 0x1.fffffep-1f;
static const float  RecipOverflow_f   = 0x1p-128f;
#else
static const float  OneMinusEpsilon_f = 0.999999940395355225f;
static const float  RecipOverflow_f   = 2.93873587705571876e-39f;
#endif
static const float  MaxFloat_f        = std::numeric_limits<float>::max();
static const float  Infinity_f        = std::numeric_limits<float>::infinity();
static const float  MachineEpsilon_f  = std::numeric_limits<float>::epsilon() / 2;

static const Float  E                 = (Float) E_d;
static const Float  Pi                = (Float) Pi_d;
static const Float  InvPi             = (Float) InvPi_d;
static const Float  InvTwoPi          = (Float) InvTwoPi_d;
static const Float  InvFourPi         = (Float) InvFourPi_d;
static const Float  SqrtPi            = (Float) SqrtPi_d;
static const Float  InvSqrtPi         = (Float) InvSqrtPi_d;
static const Float  SqrtTwo           = (Float) SqrtTwo_d;
static const Float  InvSqrtTwo        = (Float) InvSqrtTwo_d;
static const Float  SqrtTwoPi         = (Float) SqrtTwoPi_d;
static const Float  InvSqrtTwoPi      = (Float) InvSqrtTwoPi_d;
static const Float  OneMinusEpsilon   = sizeof(Float) == sizeof(double) ?
                                        OneMinusEpsilon_d : OneMinusEpsilon_f;
static const Float  RecipOverflow     = sizeof(Float) == sizeof(double) ?
                                        RecipOverflow_d : RecipOverflow_f;
static const Float  Epsilon           = sizeof(Float) == sizeof(double) ?
                                        Epsilon_d : Epsilon_f;
static const Float  MaxFloat          = std::numeric_limits<Float>::max();
static const Float  Infinity          = std::numeric_limits<Float>::infinity();
static const Float  MachineEpsilon    = std::numeric_limits<Float>::epsilon() / 2;

//! @}
// -----------------------------------------------------------------------

// -----------------------------------------------------------------------
//! @{ \name "Safe" mathematical functions that avoid domain errors
// -----------------------------------------------------------------------

/// Arcsine variant that gracefully handles arguments > 1 due to roundoff errors
template <typename Scalar> Scalar safe_asin(Scalar value) {
    return std::asin(std::min((Scalar) 1, std::max((Scalar) -1, value)));
}

/// Arccosine variant that gracefully handles arguments > 1 due to roundoff errors
template <typename Scalar> Scalar safe_acos(Scalar value) {
    return std::acos(std::min((Scalar) 1, std::max((Scalar) -1, value)));
}

/// Square root variant that gracefully handles arguments < 0 due to roundoff errors
template <typename Scalar> Scalar safe_sqrt(Scalar value) {
    return std::sqrt(std::max((Scalar) 0, value));
}

//! @}
// -----------------------------------------------------------------------

// -----------------------------------------------------------------------
//! @{ \name Complete and incomplete elliptic integrals
//! Caution: the 'k' factor is squared in the elliptic integral, which
//! differs from the convention of Mathematica's EllipticK etc.
// -----------------------------------------------------------------------

/// Complete elliptic integral of the first kind (double precision)
extern MTS_EXPORT_CORE double comp_ellint_1(double k);
/// Complete elliptic integral of the second kind (double precision)
extern MTS_EXPORT_CORE double comp_ellint_2(double k);
/// Complete elliptic integral of the third kind (double precision)
extern MTS_EXPORT_CORE double comp_ellint_3(double k, double nu);
/// Incomplete elliptic integral of the first kind (double precision)
extern MTS_EXPORT_CORE double ellint_1(double k, double phi);
/// Incomplete elliptic integral of the second kind (double precision)
extern MTS_EXPORT_CORE double ellint_2(double k, double phi);
/// Incomplete elliptic integral of the third kind (double precision)
extern MTS_EXPORT_CORE double ellint_3(double k, double nu, double phi);

/// Complete elliptic integral of the first kind (single precision)
extern MTS_EXPORT_CORE float comp_ellint_1(float k);
/// Complete elliptic integral of the second kind (single precision)
extern MTS_EXPORT_CORE float comp_ellint_2(float k);
/// Complete elliptic integral of the third kind (single precision)
extern MTS_EXPORT_CORE float comp_ellint_3(float k, float nu);
/// Incomplete elliptic integral of the first kind (single precision)
extern MTS_EXPORT_CORE float ellint_1(float k, float phi);
/// Incomplete elliptic integral of the second kind (single precision)
extern MTS_EXPORT_CORE float ellint_2(float k, float phi);
/// Incomplete elliptic integral of the first kind (single precision)
extern MTS_EXPORT_CORE float ellint_3(float k, float nu, float phi);

//! @}
// -----------------------------------------------------------------------

// -----------------------------------------------------------------------
//! @{ \name Bessel functions
// -----------------------------------------------------------------------

/// Exponentially scaled modified Bessel function of the first kind (order 0), double precision
extern MTS_EXPORT_CORE double i0e(double x);
/// Exponentially scaled modified Bessel function of the first kind (order 0), single precision
extern MTS_EXPORT_CORE float  i0e(float x);

//! @}
// -----------------------------------------------------------------------

// -----------------------------------------------------------------------
//! @{ \name Legendre functions
// -----------------------------------------------------------------------

/// Evaluate the l-th Legendre polynomial using recurrence, single precision
extern MTS_EXPORT_CORE float legendre_p(int l, float x);

/// Evaluate the l-th Legendre polynomial using recurrence, double precision
extern MTS_EXPORT_CORE double legendre_p(int l, double x);

/// Evaluate the an associated Legendre polynomial using recurrence, single precision
extern MTS_EXPORT_CORE float legendre_p(int l, int m, float x);

/// Evaluate the an associated Legendre polynomial using recurrence, double precision
extern MTS_EXPORT_CORE double legendre_p(int l, int m, double x);

/// Evaluate the l-th Legendre polynomial and its derivative using recurrence, single precision
extern MTS_EXPORT_CORE std::pair<float, float> legendre_pd(int l, float x);

/// Evaluate the l-th Legendre polynomial and its derivative using recurrence, double precision
extern MTS_EXPORT_CORE std::pair<double, double> legendre_pd(int l, double x);

/// Evaluate the function <tt>legendre_pd(l+1, x) - legendre_pd(l-1, x)</tt>, single precision
extern MTS_EXPORT_CORE std::pair<float, float> legendre_pd_diff(int l, float x);

/// Evaluate the function <tt>legendre_pd(l+1, x) - legendre_pd(l-1, x)</tt>, double precision
extern MTS_EXPORT_CORE std::pair<double, double> legendre_pd_diff(int l, double x);

//! @}
// -----------------------------------------------------------------------

// -----------------------------------------------------------------------
//! @{ \name Miscellaneous mathematical helper functions
// -----------------------------------------------------------------------

/// Simple signum function -- note that it returns the FP sign of the input (and never zero)
template <typename Scalar> Scalar signum(Scalar value) {
    return std::copysign((Scalar) 1, value);
}

/// Generic range clamping function
template <typename Scalar>  Scalar clamp(Scalar value, Scalar min, Scalar max) {
    return (value < min) ? min :
           (value > max ? max : value);
}

/**
 * \brief Find an interval in an ordered set
 *
 * This function is very similar to \c std::upper_bound, but it uses a functor
 * rather than an actual array to permit working with procedurally defined
 * data. It returns the index \c i such that pred(i) is \c true and pred(i+1)
 * is \c false.
 *
 * This function is primarily used to locate an interval (i, i+1) for linear
 * interpolation, hence its name. To avoid issues out of bounds accesses, and
 * to deal with predicates that evaluate to \c true or \c false on the entire
 * domain, the returned left interval index is clamped to the range <tt>[0,
 * size-2]</tt>.
 */
template <typename Predicate>
size_t findInterval(size_t size, const Predicate &pred) {
    size_t first = 0, len = size;
    while (len > 0) {
        size_t half = len >> 1, middle = first + half;
        if (pred(middle)) {
            first = middle + 1;
            len -= half + 1;
        } else {
            len = half;
        }
    }
    return (size_t) clamp<ssize_t>((ssize_t) first - 1, 0, size - 2);
}

/// Quantile function of the standard normal distribution (double precision)
extern MTS_EXPORT_CORE double normal_quantile(double p);

/// Quantile function of the standard normal distribution (single precision)
extern MTS_EXPORT_CORE float normal_quantile(float p);

/// Cumulative distribution function of the standard normal distribution (double precision)
extern MTS_EXPORT_CORE double normal_cdf(double p);

/// Cumulative distribution function of the standard normal distribution (single precision)
extern MTS_EXPORT_CORE float normal_cdf(float p);

/// Error function (double precision)
extern MTS_EXPORT_CORE double erf(double p);

/// Error function (single precision)
extern MTS_EXPORT_CORE float erf(float p);

/// Inverse error function (double precision)
extern MTS_EXPORT_CORE double erfinv(double p);

/// Inverse error function (single precision)
extern MTS_EXPORT_CORE float erfinv(float p);

//! @}
// -----------------------------------------------------------------------

NAMESPACE_END(math)
NAMESPACE_END(mitsuba)
