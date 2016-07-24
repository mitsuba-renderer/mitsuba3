#pragma once

#include <mitsuba/core/logger.h>
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
static const Float  OneMinusEpsilon   = Float(sizeof(Float) == sizeof(double) ?
                                        OneMinusEpsilon_d : (double) OneMinusEpsilon_f);
static const Float  RecipOverflow     = Float(sizeof(Float) == sizeof(double) ?
                                        RecipOverflow_d : (double) RecipOverflow_f);
static const Float  Epsilon           = Float(sizeof(Float) == sizeof(double) ?
                                        Epsilon_d : (double) Epsilon_f);
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

/// sqrt(a^2 + b^2) without range issues (like 'hypot' on compilers that support C99)
template <typename Scalar> Scalar hypot2(Scalar a, Scalar b) {
    Scalar r;
    if (std::abs(a) > std::abs(b)) {
        r = b / a;
        r = std::abs(a) * std::sqrt((Scalar) 1 + r*r);
    } else if (b != (Scalar) 0) {
        r = a / b;
        r = std::abs(b) * std::sqrt((Scalar) 1 + r*r);
    } else {
        r = (Scalar) 0;
    }
    return r;
}

//! @}
// -----------------------------------------------------------------------

// -----------------------------------------------------------------------
//! @{ \name Fast computation of both sin & cos for a given angle,
//! enabled on Linux.
// -----------------------------------------------------------------------

#if defined(_GNU_SOURCE)
    /// Fast computation of both sin & cos for a given angle
    inline void sincos(float theta, float *sin, float *cos) {
        ::sincosf(theta, sin, cos);
    }

    /// Fast computation of both sin & cos for a given angle
    inline void sincos(double theta, double *sin, double *cos) {
        ::sincos(theta, sin, cos);
    }
#else
    /// On this platform, equivalent to computing sin and cos separately
    inline void sincos(float theta, float *_sin, float *_cos) {
        *_sin = sinf(theta);
        *_cos = cosf(theta);
    }

    /// On this platform, equivalent to computing sin and cos separately
    inline void sincos(double theta, double *_sin, double *_cos) {
        *_sin = sin(theta);
        *_cos = cos(theta);
    }
#endif

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

//// Convert radians to degrees
template <typename T> T radToDeg(T value) { return value * T(180 / Pi_d); }

/// Convert degrees to radians
template <typename T> T degToRad(T value) { return value * T(Pi_d / 180); }

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
 * \brief Compare the difference in ULPs between a reference value and another
 * given floating point number
 */
template <typename T> T ulpdiff(T ref, T val) {
    const T eps = std::numeric_limits<T>::epsilon() / 2;

    /* Express mantissa wrt. same exponent */
    int e_ref, e_val;
    T m_ref = std::frexp(ref, &e_ref);
    T m_val = std::frexp(val, &e_val);

    T diff;
    if (e_ref == e_val)
        diff = m_ref - m_val;
    else
        diff = m_ref - std::ldexp(m_val, e_val-e_ref);

    return std::abs(diff) / eps;
}

/// Always-positive modulo function
template <typename T> T modulo(T a, T b) {
    T result = a - (a / b) * b;
    return (result < 0) ? result + b : result;
}

/// Efficiently compute the floor of the base-2 logarithm of an unsigned integer
template <typename T> T log2i(T value) {
    Assert(value >= 0);
#if defined(__GNUC__) && defined(__x86_64__)
    T result;
    asm ("bsr %1, %0" : "=r" (result) : "r" (value));
    return result;
#elif defined(_WIN32)
    unsigned long result;
    if (sizeof(T) <= 4)
        _BitScanReverse(&result, (unsigned long) value);
    else
        _BitScanReverse64(&result, (unsigned __int64) value);
    return T(result);
#else
    T r = 0;
    while ((value >> r) != 0)
        r++;
    return r - 1;
#endif
}

/// Check whether the provided integer is a power of two
template <typename T> bool isPowerOfTwo(T i) {
    return i > 0 && (i & (i-1)) == 0;
}

/// Round an unsigned integer to the next integer power of two
template <typename T> T roundToPowerOfTwo(T i) {
    if (i <= 1)
        return 1;
#if defined(MTS_CLZ) && defined(MTS_CLZLL)
    return T(1) << (log2i<T>(i - 1) + 1);
#else
	i--;
	i |= i >> 1;
	i |= i >> 2;
	i |= i >> 4;
	if (sizeof(T) >= 2)
        i |= i >> 8;
	if (sizeof(T) >= 4)
        i |= i >> 16;
	if (sizeof(T) >= 8)
        i |= i >> 32;
    return i + 1;
#endif
}

/// Apply the sRGB gamma curve to a floating point scalar
template <typename T>
static T gamma(T value) {
    if (value <= T(0.0031308))
        return T(12.92) * value;
    else
        return T(1.055) * std::pow(value, T(1.0/2.4)) - T(0.055);
}

/// Apply the inverse of the sRGB gamma curve to a floating point scalar
template <typename T>
static T inverseGamma(T value) {
    if (value <= T(0.04045))
        return value * T(1.0 / 12.92);
    else
        return std::pow((value + T(0.055)) * T(1.0 / 1.055), T(2.4));
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
 * domain, the returned left interval index is clamped to the range <tt>[left,
 * right-2]</tt>.
 */
template <typename Size, typename Predicate>
size_t findInterval(Size left, Size right, const Predicate &pred) {
    typedef typename std::make_signed<Size>::type SignedSize;

    Size first = left, len = right - left;
    while (len > 0) {
        Size half = len >> 1,
             middle = first + half;

        if (pred(middle)) {
            first = middle + 1;
            len -= half + 1;
        } else {
            len = half;
        }
    }

    return (Size) clamp<SignedSize>(
        (SignedSize) first - 1,
        (SignedSize) left,
        (SignedSize) right - 2
    );
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
    int it = 0;
    while (true) {
        Scalar middle = (left + right) * 0.5f;

        /* Paranoid stopping criterion */
        if (middle <= left || middle >= right) {
            middle = std::nextafter(left, right);
            if (middle == right)
                break;
        }

        if (pred(middle))
            left = middle;
        else
            right = middle;
        it++;
        if (it > 100)
            std::cout << "Now at " << it << " iterations" << std::endl;
    }

    return left;
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
