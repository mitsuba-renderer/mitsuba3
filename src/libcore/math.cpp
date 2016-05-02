#include <mitsuba/core/math.h>
#include <stdexcept>
#include <cassert>

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(math)
NAMESPACE_BEGIN(detail)

/*
 * The standard normal CDF, for one random variable.
 *
 *   Author:  W. J. Cody
 *   URL:   http://www.netlib.org/specfun/erf
 *
 * This is the erfc() routine only, adapted by the
 * transform normal_cdf(u)=(erfc(-u/sqrt(2))/2;
 */
template <typename Scalar> Scalar normal_cdf(Scalar u) {
    const Scalar a[5] = {
        1.161110663653770e-002,3.951404679838207e-001,2.846603853776254e+001,
        1.887426188426510e+002,3.209377589138469e+003
    };
    const Scalar b[5] = {
        1.767766952966369e-001,8.344316438579620e+000,1.725514762600375e+002,
        1.813893686502485e+003,8.044716608901563e+003
    };
    const Scalar c[9] = {
        2.15311535474403846e-8,5.64188496988670089e-1,8.88314979438837594e00,
        6.61191906371416295e01,2.98635138197400131e02,8.81952221241769090e02,
        1.71204761263407058e03,2.05107837782607147e03,1.23033935479799725E03
    };
    const Scalar d[9] = {
        1.00000000000000000e00,1.57449261107098347e01,1.17693950891312499e02,
        5.37181101862009858e02,1.62138957456669019e03,3.29079923573345963e03,
        4.36261909014324716e03,3.43936767414372164e03,1.23033935480374942e03
    };
    const Scalar p[6] = {
        1.63153871373020978e-2,3.05326634961232344e-1,3.60344899949804439e-1,
        1.25781726111229246e-1,1.60837851487422766e-2,6.58749161529837803e-4
    };
    const Scalar q[6] = {
        1.00000000000000000e00,2.56852019228982242e00,1.87295284992346047e00,
        5.27905102951428412e-1,6.05183413124413191e-2,2.33520497626869185e-3
    };
    Scalar y, z;

    if (!std::isfinite(u))
        return (u < 0 ? 0.f : 1.f);
    y = std::abs(u);

    if (y <= (Scalar) 0.46875 * (Scalar) math::SqrtTwo_d) {
        /* evaluate erf() for |u| <= sqrt(2)*0.46875 */
        z = y*y;
        y = u*((((a[0]*z+a[1])*z+a[2])*z+a[3])*z+a[4])
             /((((b[0]*z+b[1])*z+b[2])*z+b[3])*z+b[4]);
        return (Scalar) 0.5 + y;
    }

    z = std::exp(-y*y/2)/2;
    if (y <= 4.0) {
        /* evaluate erfc() for sqrt(2)*0.46875 <= |u| <= sqrt(2)*4.0 */
        y = y/(Scalar) math::SqrtTwo_d;
        y = ((((((((c[0]*y+c[1])*y+c[2])*y+c[3])*y+c[4])*y+c[5])*y+c[6])*y+c[7])*y+c[8])
           /((((((((d[0]*y+d[1])*y+d[2])*y+d[3])*y+d[4])*y+d[5])*y+d[6])*y+d[7])*y+d[8]);
        y = z*y;
    } else {
        /* evaluate erfc() for |u| > sqrt(2)*4.0 */
        z = z*(Scalar) math::SqrtTwo_d/y;
        y = 2/(y*y);
        y = y*(((((p[0]*y+p[1])*y+p[2])*y+p[3])*y+p[4])*y+p[5])
             /(((((q[0]*y+q[1])*y+q[2])*y+q[3])*y+q[4])*y+q[5]);
        y = z*((Scalar) math::InvSqrtPi_d-y);
    }
    return (u < 0.0 ? y : 1-y);
}

/*
 * Quantile function of the standard normal distribution.
 *
 *   Author:      Peter John Acklam <pjacklam@online.no>
 *   URL:         http://home.online.no/~pjacklam
 *
 * This function is based on the MATLAB code from the address above,
 * translated to C, and adapted for our purposes.
 */
template <typename Scalar> Scalar normal_quantile(Scalar p) {
    const Scalar a[6] = {
        -3.969683028665376e+01,  2.209460984245205e+02,
        -2.759285104469687e+02,  1.383577518672690e+02,
        -3.066479806614716e+01,  2.506628277459239e+00
    };
    const Scalar b[5] = {
        -5.447609879822406e+01,  1.615858368580409e+02,
        -1.556989798598866e+02,  6.680131188771972e+01,
        -1.328068155288572e+01
    };
    const Scalar c[6] = {
        -7.784894002430293e-03, -3.223964580411365e-01,
        -2.400758277161838e+00, -2.549732539343734e+00,
        4.374664141464968e+00,  2.938163982698783e+00
    };
    const Scalar d[4] = {
        7.784695709041462e-03,  3.224671290700398e-01,
        2.445134137142996e+00,  3.754408661907416e+00
    };

    if (p <= 0)
        return -std::numeric_limits<Scalar>::infinity();
    else if (p >= 1)
        return -std::numeric_limits<Scalar>::infinity();

    Scalar q = std::min(p,1-p);
    Scalar t, u;
    if (q > (Scalar) 0.02425) {
        /* Rational approximation for central region. */
        u = q - (Scalar) 0.5;
        t = u*u;
        u = u*(((((a[0]*t+a[1])*t+a[2])*t+a[3])*t+a[4])*t+a[5])
             /(((((b[0]*t+b[1])*t+b[2])*t+b[3])*t+b[4])*t+1);
    } else {
        /* Rational approximation for tail region. */
        t = std::sqrt(-2*std::log(q));
        u = (((((c[0]*t+c[1])*t+c[2])*t+c[3])*t+c[4])*t+c[5])
            /((((d[0]*t+d[1])*t+d[2])*t+d[3])*t+1);
    }

    /* The relative error of the approximation has absolute value less
       than 1.15e-9.  One iteration of Halley's rational method (third
       order) gives full machine precision... */
    t = normal_cdf(u)-q;    /* error */
    t = t*(Scalar) math::SqrtTwoPi_d*std::exp(u*u/2);   /* f(u)/df(u) */
    u = u-t/(1+u*t/2);     /* Halley's method */

    return p > 0.5 ? -u : u;
};

// Copyright (C) 2006, 2007, 2008
// Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this library; see the file COPYING.  If not, write to the Free
// Software Foundation, 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
// USA.
//
// As a special exception, you may use this file as part of a free software
// library without restriction.  Specifically, if other files instantiate
// templates or use macros or inline functions from this file, or you compile
// this file and link it with other files to produce an executable, this
// file does not by itself cause the resulting executable to be covered by
// the GNU General Public License.  This exception does not however
// invalidate any other reasons why the executable file might be covered by
// the GNU General Public License.

/** @file tr1/ell_integral.tcc
 *  This is an internal header file, included by other library headers.
 *  You should not attempt to use it directly.
 */

//
// ISO C++ 14882 TR1: 5.2  Special functions
//

// Written by Edward Smith-Rowland based on:
//   (1)  B. C. Carlson Numer. Math. 33, 1 (1979)
//   (2)  B. C. Carlson, Special Functions of Applied Mathematics (1977)
//   (3)  The Gnu Scientific Library, http://www.gnu.org/software/gsl
//   (4)  Numerical Recipes in C, 2nd ed, by W. H. Press, S. A. Teukolsky,
//        W. T. Vetterling, B. P. Flannery, Cambridge University Press
//        (1992), pp. 261-269
// [5.2] Special functions

// Implementation-space details.

/**
 *   @brief Return the Carlson elliptic function @f$ R_F(x,y,z) @f$
 *          of the first kind.
 * 
 *   The Carlson elliptic function of the first kind is defined by:
 *   @f[
 *       R_F(x,y,z) = \frac{1}{2} \int_0^\infty
 *                 \frac{dt}{(t + x)^{1/2}(t + y)^{1/2}(t + z)^{1/2}}
 *   @f]
 *
 *   @param  __x  The first of three symmetric arguments.
 *   @param  __y  The second of three symmetric arguments.
 *   @param  __z  The third of three symmetric arguments.
 *   @return  The Carlson elliptic function of the first kind.
 */
template<typename _Tp>
_Tp
__ellint_rf(const _Tp __x, const _Tp __y, const _Tp __z)
{
  const _Tp __min = std::numeric_limits<_Tp>::min();
  const _Tp __lolim = _Tp(5) * __min;

  if (__x < _Tp(0) || __y < _Tp(0) || __z < _Tp(0))
    throw std::domain_error("Argument less than zero "
                                  "in __ellint_rf.");
  else if (__x + __y < __lolim || __x + __z < __lolim
        || __y + __z < __lolim)
    throw std::domain_error("Argument too small in __ellint_rf");
  else
    {
      const _Tp __c0 = _Tp(1) / _Tp(4);
      const _Tp __c1 = _Tp(1) / _Tp(24);
      const _Tp __c2 = _Tp(1) / _Tp(10);
      const _Tp __c3 = _Tp(3) / _Tp(44);
      const _Tp __c4 = _Tp(1) / _Tp(14);

      _Tp __xn = __x;
      _Tp __yn = __y;
      _Tp __zn = __z;

      const _Tp __eps = std::numeric_limits<_Tp>::epsilon();
      const _Tp __errtol = std::pow(__eps, _Tp(1) / _Tp(6));
      _Tp __mu;
      _Tp __xndev, __yndev, __zndev;

      const unsigned int __max_iter = 100;
      for (unsigned int __iter = 0; __iter < __max_iter; ++__iter)
        {
          __mu = (__xn + __yn + __zn) / _Tp(3);
          __xndev = 2 - (__mu + __xn) / __mu;
          __yndev = 2 - (__mu + __yn) / __mu;
          __zndev = 2 - (__mu + __zn) / __mu;
          _Tp __epsilon = std::max(std::abs(__xndev), std::abs(__yndev));
          __epsilon = std::max(__epsilon, std::abs(__zndev));
          if (__epsilon < __errtol)
            break;
          const _Tp __xnroot = std::sqrt(__xn);
          const _Tp __ynroot = std::sqrt(__yn);
          const _Tp __znroot = std::sqrt(__zn);
          const _Tp __lambda = __xnroot * (__ynroot + __znroot)
                             + __ynroot * __znroot;
          __xn = __c0 * (__xn + __lambda);
          __yn = __c0 * (__yn + __lambda);
          __zn = __c0 * (__zn + __lambda);
        }

      const _Tp __e2 = __xndev * __yndev - __zndev * __zndev;
      const _Tp __e3 = __xndev * __yndev * __zndev;
      const _Tp __s  = _Tp(1) + (__c1 * __e2 - __c2 - __c3 * __e3) * __e2
               + __c4 * __e3;

      return __s / std::sqrt(__mu);
    }
}


/**
 *   @brief Return the complete elliptic integral of the first kind
 *          @f$ K(k) @f$ by series expansion.
 * 
 *   The complete elliptic integral of the first kind is defined as
 *   @f[
 *     K(k) = F(k,\pi/2) = \int_0^{\pi/2}\frac{d\theta}
 *                              {\sqrt{1 - k^2sin^2\theta}}
 *   @f]
 * 
 *   This routine is not bad as long as |k| is somewhat smaller than 1
 *   but is not is good as the Carlson elliptic integral formulation.
 * 
 *   @param  __k  The argument of the complete elliptic function.
 *   @return  The complete elliptic function of the first kind.
 */
template<typename _Tp>
_Tp
__comp_ellint_1_series(const _Tp __k)
{

  const _Tp __kk = __k * __k;

  _Tp __term = __kk / _Tp(4);
  _Tp __sum = _Tp(1) + __term;

  const unsigned int __max_iter = 1000;
  for (unsigned int __i = 2; __i < __max_iter; ++__i)
    {
      __term *= (2 * __i - 1) * __kk / (2 * __i);
      if (__term < std::numeric_limits<_Tp>::epsilon())
        break;
      __sum += __term;
    }

  return _Tp(0.5 * math::Pi_d) * __sum;
}


/**
 *   @brief  Return the complete elliptic integral of the first kind
 *           @f$ K(k) @f$ using the Carlson formulation.
 * 
 *   The complete elliptic integral of the first kind is defined as
 *   @f[
 *     K(k) = F(k,\pi/2) = \int_0^{\pi/2}\frac{d\theta}
 *                                           {\sqrt{1 - k^2 sin^2\theta}}
 *   @f]
 *   where @f$ F(k,\phi) @f$ is the incomplete elliptic integral of the
 *   first kind.
 * 
 *   @param  __k  The argument of the complete elliptic function.
 *   @return  The complete elliptic function of the first kind.
 */
template<typename _Tp>
_Tp
__comp_ellint_1(const _Tp __k)
{

  if (std::isnan(__k))
    return std::numeric_limits<_Tp>::quiet_NaN();
  else if (std::abs(__k) >= _Tp(1))
    return std::numeric_limits<_Tp>::quiet_NaN();
  else
    return __ellint_rf(_Tp(0), _Tp(1) - __k * __k, _Tp(1));
}


/**
 *   @brief  Return the incomplete elliptic integral of the first kind
 *           @f$ F(k,\phi) @f$ using the Carlson formulation.
 * 
 *   The incomplete elliptic integral of the first kind is defined as
 *   @f[
 *     F(k,\phi) = \int_0^{\phi}\frac{d\theta}
 *                                   {\sqrt{1 - k^2 sin^2\theta}}
 *   @f]
 * 
 *   @param  __k  The argument of the elliptic function.
 *   @param  __phi  The integral limit argument of the elliptic function.
 *   @return  The elliptic function of the first kind.
 */
template<typename _Tp>
_Tp
__ellint_1(const _Tp __k, const _Tp __phi)
{

  if (std::isnan(__k) || std::isnan(__phi))
    return std::numeric_limits<_Tp>::quiet_NaN();
  else if (std::abs(__k) > _Tp(1))
    throw std::domain_error("Bad argument in __ellint_1.");
  else
    {
      //  Reduce phi to -pi/2 < phi < +pi/2.
      const int __n = std::floor(__phi / _Tp(math::Pi_d)
                               + _Tp(0.5L));
      const _Tp __phi_red = __phi
                          - __n * _Tp(math::Pi_d);

      const _Tp __s = std::sin(__phi_red);
      const _Tp __c = std::cos(__phi_red);

      const _Tp __F = __s
                    * __ellint_rf(__c * __c,
                            _Tp(1) - __k * __k * __s * __s, _Tp(1));

      if (__n == 0)
        return __F;
      else
        return __F + _Tp(2) * __n * __comp_ellint_1(__k);
    }
}


/**
 *   @brief Return the complete elliptic integral of the second kind
 *          @f$ E(k) @f$ by series expansion.
 * 
 *   The complete elliptic integral of the second kind is defined as
 *   @f[
 *     E(k,\pi/2) = \int_0^{\pi/2}\sqrt{1 - k^2 sin^2\theta}
 *   @f]
 * 
 *   This routine is not bad as long as |k| is somewhat smaller than 1
 *   but is not is good as the Carlson elliptic integral formulation.
 * 
 *   @param  __k  The argument of the complete elliptic function.
 *   @return  The complete elliptic function of the second kind.
 */
template<typename _Tp>
_Tp
__comp_ellint_2_series(const _Tp __k)
{

  const _Tp __kk = __k * __k;

  _Tp __term = __kk;
  _Tp __sum = __term;

  const unsigned int __max_iter = 1000;
  for (unsigned int __i = 2; __i < __max_iter; ++__i)
    {
      const _Tp __i2m = 2 * __i - 1;
      const _Tp __i2 = 2 * __i;
      __term *= __i2m * __i2m * __kk / (__i2 * __i2);
      if (__term < std::numeric_limits<_Tp>::epsilon())
        break;
      __sum += __term / __i2m;
    }

  return _Tp(0.5 * math::Pi_d) * (_Tp(1) - __sum);
}


/**
 *   @brief  Return the Carlson elliptic function of the second kind
 *           @f$ R_D(x,y,z) = R_J(x,y,z,z) @f$ where
 *           @f$ R_J(x,y,z,p) @f$ is the Carlson elliptic function
 *           of the third kind.
 * 
 *   The Carlson elliptic function of the second kind is defined by:
 *   @f[
 *       R_D(x,y,z) = \frac{3}{2} \int_0^\infty
 *                 \frac{dt}{(t + x)^{1/2}(t + y)^{1/2}(t + z)^{3/2}}
 *   @f]
 *
 *   Based on Carlson's algorithms:
 *   -  B. C. Carlson Numer. Math. 33, 1 (1979)
 *   -  B. C. Carlson, Special Functions of Applied Mathematics (1977)
 *   -  Numerical Recipes in C, 2nd ed, pp. 261-269,
 *      by Press, Teukolsky, Vetterling, Flannery (1992)
 *
 *   @param  __x  The first of two symmetric arguments.
 *   @param  __y  The second of two symmetric arguments.
 *   @param  __z  The third argument.
 *   @return  The Carlson elliptic function of the second kind.
 */
template<typename _Tp>
_Tp
__ellint_rd(const _Tp __x, const _Tp __y, const _Tp __z)
{
  const _Tp __eps = std::numeric_limits<_Tp>::epsilon();
  const _Tp __errtol = std::pow(__eps / _Tp(8), _Tp(1) / _Tp(6));
  const _Tp __max = std::numeric_limits<_Tp>::max();
  const _Tp __lolim = _Tp(2) / std::pow(__max, _Tp(2) / _Tp(3));

  if (__x < _Tp(0) || __y < _Tp(0))
    throw std::domain_error("Argument less than zero "
                                  "in __ellint_rd.");
  else if (__x + __y < __lolim || __z < __lolim)
    throw std::domain_error("Argument too small "
                                  "in __ellint_rd.");
  else
    {
      const _Tp __c0 = _Tp(1) / _Tp(4);
      const _Tp __c1 = _Tp(3) / _Tp(14);
      const _Tp __c2 = _Tp(1) / _Tp(6);
      const _Tp __c3 = _Tp(9) / _Tp(22);
      const _Tp __c4 = _Tp(3) / _Tp(26);

      _Tp __xn = __x;
      _Tp __yn = __y;
      _Tp __zn = __z;
      _Tp __sigma = _Tp(0);
      _Tp __power4 = _Tp(1);

      _Tp __mu;
      _Tp __xndev, __yndev, __zndev;

      const unsigned int __max_iter = 100;
      for (unsigned int __iter = 0; __iter < __max_iter; ++__iter)
        {
          __mu = (__xn + __yn + _Tp(3) * __zn) / _Tp(5);
          __xndev = (__mu - __xn) / __mu;
          __yndev = (__mu - __yn) / __mu;
          __zndev = (__mu - __zn) / __mu;
          _Tp __epsilon = std::max(std::abs(__xndev), std::abs(__yndev));
          __epsilon = std::max(__epsilon, std::abs(__zndev));
          if (__epsilon < __errtol)
            break;
          _Tp __xnroot = std::sqrt(__xn);
          _Tp __ynroot = std::sqrt(__yn);
          _Tp __znroot = std::sqrt(__zn);
          _Tp __lambda = __xnroot * (__ynroot + __znroot)
                       + __ynroot * __znroot;
          __sigma += __power4 / (__znroot * (__zn + __lambda));
          __power4 *= __c0;
          __xn = __c0 * (__xn + __lambda);
          __yn = __c0 * (__yn + __lambda);
          __zn = __c0 * (__zn + __lambda);
        }

      _Tp __ea = __xndev * __yndev;
      _Tp __eb = __zndev * __zndev;
      _Tp __ec = __ea - __eb;
      _Tp __ed = __ea - _Tp(6) * __eb;
      _Tp __ef = __ed + __ec + __ec;
      _Tp __s1 = __ed * (-__c1 + __c3 * __ed
                               / _Tp(3) - _Tp(3) * __c4 * __zndev * __ef
                               / _Tp(2));
      _Tp __s2 = __zndev
               * (__c2 * __ef
                + __zndev * (-__c3 * __ec - __zndev * __c4 - __ea));

      return _Tp(3) * __sigma + __power4 * (_Tp(1) + __s1 + __s2)
                                    / (__mu * std::sqrt(__mu));
    }
}


/**
 *   @brief  Return the complete elliptic integral of the second kind
 *           @f$ E(k) @f$ using the Carlson formulation.
 * 
 *   The complete elliptic integral of the second kind is defined as
 *   @f[
 *     E(k,\pi/2) = \int_0^{\pi/2}\sqrt{1 - k^2 sin^2\theta}
 *   @f]
 * 
 *   @param  __k  The argument of the complete elliptic function.
 *   @return  The complete elliptic function of the second kind.
 */
template<typename _Tp>
_Tp
__comp_ellint_2(const _Tp __k)
{

  if (std::isnan(__k))
    return std::numeric_limits<_Tp>::quiet_NaN();
  else if (std::abs(__k) == 1)
    return _Tp(1);
  else if (std::abs(__k) > _Tp(1))
    throw std::domain_error("Bad argument in __comp_ellint_2.");
  else
    {
      const _Tp __kk = __k * __k;

      return __ellint_rf(_Tp(0), _Tp(1) - __kk, _Tp(1))
           - __kk * __ellint_rd(_Tp(0), _Tp(1) - __kk, _Tp(1)) / _Tp(3);
    }
}


/**
 *   @brief  Return the incomplete elliptic integral of the second kind
 *           @f$ E(k,\phi) @f$ using the Carlson formulation.
 * 
 *   The incomplete elliptic integral of the second kind is defined as
 *   @f[
 *     E(k,\phi) = \int_0^{\phi} \sqrt{1 - k^2 sin^2\theta}
 *   @f]
 * 
 *   @param  __k  The argument of the elliptic function.
 *   @param  __phi  The integral limit argument of the elliptic function.
 *   @return  The elliptic function of the second kind.
 */
template<typename _Tp>
_Tp
__ellint_2(const _Tp __k, const _Tp __phi)
{

  if (std::isnan(__k) || std::isnan(__phi))
    return std::numeric_limits<_Tp>::quiet_NaN();
  else if (std::abs(__k) > _Tp(1))
    throw std::domain_error("Bad argument in __ellint_2.");
  else
    {
      //  Reduce phi to -pi/2 < phi < +pi/2.
      const int __n = std::floor(__phi / _Tp(math::Pi_d)
                               + _Tp(0.5L));
      const _Tp __phi_red = __phi
                          - __n * _Tp(math::Pi_d);

      const _Tp __kk = __k * __k;
      const _Tp __s = std::sin(__phi_red);
      const _Tp __ss = __s * __s;
      const _Tp __sss = __ss * __s;
      const _Tp __c = std::cos(__phi_red);
      const _Tp __cc = __c * __c;

      const _Tp __E = __s
                    * __ellint_rf(__cc, _Tp(1) - __kk * __ss, _Tp(1))
                    - __kk * __sss
                    * __ellint_rd(__cc, _Tp(1) - __kk * __ss, _Tp(1))
                    / _Tp(3);

      if (__n == 0)
        return __E;
      else
        return __E + _Tp(2) * __n * __comp_ellint_2(__k);
    }
}


/**
 *   @brief  Return the Carlson elliptic function
 *           @f$ R_C(x,y) = R_F(x,y,y) @f$ where @f$ R_F(x,y,z) @f$
 *           is the Carlson elliptic function of the first kind.
 * 
 *   The Carlson elliptic function is defined by:
 *   @f[
 *       R_C(x,y) = \frac{1}{2} \int_0^\infty
 *                 \frac{dt}{(t + x)^{1/2}(t + y)}
 *   @f]
 *
 *   Based on Carlson's algorithms:
 *   -  B. C. Carlson Numer. Math. 33, 1 (1979)
 *   -  B. C. Carlson, Special Functions of Applied Mathematics (1977)
 *   -  Numerical Recipes in C, 2nd ed, pp. 261-269,
 *      by Press, Teukolsky, Vetterling, Flannery (1992)
 *
 *   @param  __x  The first argument.
 *   @param  __y  The second argument.
 *   @return  The Carlson elliptic function.
 */
template<typename _Tp>
_Tp
__ellint_rc(const _Tp __x, const _Tp __y)
{
  const _Tp __min = std::numeric_limits<_Tp>::min();
  const _Tp __lolim = _Tp(5) * __min;

  if (__x < _Tp(0) || __y < _Tp(0) || __x + __y < __lolim)
    throw std::domain_error("Argument less than zero "
                                  "in __ellint_rc.");
  else
    {
      const _Tp __c0 = _Tp(1) / _Tp(4);
      const _Tp __c1 = _Tp(1) / _Tp(7);
      const _Tp __c2 = _Tp(9) / _Tp(22);
      const _Tp __c3 = _Tp(3) / _Tp(10);
      const _Tp __c4 = _Tp(3) / _Tp(8);

      _Tp __xn = __x;
      _Tp __yn = __y;

      const _Tp __eps = std::numeric_limits<_Tp>::epsilon();
      const _Tp __errtol = std::pow(__eps / _Tp(30), _Tp(1) / _Tp(6));
      _Tp __mu;
      _Tp __sn;

      const unsigned int __max_iter = 100;
      for (unsigned int __iter = 0; __iter < __max_iter; ++__iter)
        {
          __mu = (__xn + _Tp(2) * __yn) / _Tp(3);
          __sn = (__yn + __mu) / __mu - _Tp(2);
          if (std::abs(__sn) < __errtol)
            break;
          const _Tp __lambda = _Tp(2) * std::sqrt(__xn) * std::sqrt(__yn)
                         + __yn;
          __xn = __c0 * (__xn + __lambda);
          __yn = __c0 * (__yn + __lambda);
        }

      _Tp __s = __sn * __sn
              * (__c3 + __sn*(__c1 + __sn * (__c4 + __sn * __c2)));

      return (_Tp(1) + __s) / std::sqrt(__mu);
    }
}


/**
 *   @brief  Return the Carlson elliptic function @f$ R_J(x,y,z,p) @f$
 *           of the third kind.
 * 
 *   The Carlson elliptic function of the third kind is defined by:
 *   @f[
 *       R_J(x,y,z,p) = \frac{3}{2} \int_0^\infty
 *       \frac{dt}{(t + x)^{1/2}(t + y)^{1/2}(t + z)^{1/2}(t + p)}
 *   @f]
 *
 *   Based on Carlson's algorithms:
 *   -  B. C. Carlson Numer. Math. 33, 1 (1979)
 *   -  B. C. Carlson, Special Functions of Applied Mathematics (1977)
 *   -  Numerical Recipes in C, 2nd ed, pp. 261-269,
 *      by Press, Teukolsky, Vetterling, Flannery (1992)
 *
 *   @param  __x  The first of three symmetric arguments.
 *   @param  __y  The second of three symmetric arguments.
 *   @param  __z  The third of three symmetric arguments.
 *   @param  __p  The fourth argument.
 *   @return  The Carlson elliptic function of the fourth kind.
 */
template<typename _Tp>
_Tp
__ellint_rj(const _Tp __x, const _Tp __y, const _Tp __z, const _Tp __p)
{
  const _Tp __min = std::numeric_limits<_Tp>::min();
  const _Tp __lolim = std::pow(_Tp(5) * __min, _Tp(1)/_Tp(3));

  if (__x < _Tp(0) || __y < _Tp(0) || __z < _Tp(0))
    throw std::domain_error("Argument less than zero "
                                  "in __ellint_rj.");
  else if (__x + __y < __lolim || __x + __z < __lolim
        || __y + __z < __lolim || __p < __lolim)
    throw std::domain_error("Argument too small "
                                  "in __ellint_rj");
  else
    {
      const _Tp __c0 = _Tp(1) / _Tp(4);
      const _Tp __c1 = _Tp(3) / _Tp(14);
      const _Tp __c2 = _Tp(1) / _Tp(3);
      const _Tp __c3 = _Tp(3) / _Tp(22);
      const _Tp __c4 = _Tp(3) / _Tp(26);

      _Tp __xn = __x;
      _Tp __yn = __y;
      _Tp __zn = __z;
      _Tp __pn = __p;
      _Tp __sigma = _Tp(0);
      _Tp __power4 = _Tp(1);

      const _Tp __eps = std::numeric_limits<_Tp>::epsilon();
      const _Tp __errtol = std::pow(__eps / _Tp(8), _Tp(1) / _Tp(6));

      _Tp __mu;
      _Tp __xndev, __yndev, __zndev, __pndev;

      const unsigned int __max_iter = 100;
      for (unsigned int __iter = 0; __iter < __max_iter; ++__iter)
        {
          __mu = (__xn + __yn + __zn + _Tp(2) * __pn) / _Tp(5);
          __xndev = (__mu - __xn) / __mu;
          __yndev = (__mu - __yn) / __mu;
          __zndev = (__mu - __zn) / __mu;
          __pndev = (__mu - __pn) / __mu;
          _Tp __epsilon = std::max(std::abs(__xndev), std::abs(__yndev));
          __epsilon = std::max(__epsilon, std::abs(__zndev));
          __epsilon = std::max(__epsilon, std::abs(__pndev));
          if (__epsilon < __errtol)
            break;
          const _Tp __xnroot = std::sqrt(__xn);
          const _Tp __ynroot = std::sqrt(__yn);
          const _Tp __znroot = std::sqrt(__zn);
          const _Tp __lambda = __xnroot * (__ynroot + __znroot)
                             + __ynroot * __znroot;
          const _Tp __alpha1 = __pn * (__xnroot + __ynroot + __znroot)
                            + __xnroot * __ynroot * __znroot;
          const _Tp __alpha2 = __alpha1 * __alpha1;
          const _Tp __beta = __pn * (__pn + __lambda)
                                  * (__pn + __lambda);
          __sigma += __power4 * __ellint_rc(__alpha2, __beta);
          __power4 *= __c0;
          __xn = __c0 * (__xn + __lambda);
          __yn = __c0 * (__yn + __lambda);
          __zn = __c0 * (__zn + __lambda);
          __pn = __c0 * (__pn + __lambda);
        }

      _Tp __ea = __xndev * (__yndev + __zndev) + __yndev * __zndev;
      _Tp __eb = __xndev * __yndev * __zndev;
      _Tp __ec = __pndev * __pndev;
      _Tp __e2 = __ea - _Tp(3) * __ec;
      _Tp __e3 = __eb + _Tp(2) * __pndev * (__ea - __ec);
      _Tp __s1 = _Tp(1) + __e2 * (-__c1 + _Tp(3) * __c3 * __e2 / _Tp(4)
                        - _Tp(3) * __c4 * __e3 / _Tp(2));
      _Tp __s2 = __eb * (__c2 / _Tp(2)
               + __pndev * (-__c3 - __c3 + __pndev * __c4));
      _Tp __s3 = __pndev * __ea * (__c2 - __pndev * __c3)
               - __c2 * __pndev * __ec;

      return _Tp(3) * __sigma + __power4 * (__s1 + __s2 + __s3)
                                         / (__mu * std::sqrt(__mu));
    }
}


/**
 *   @brief Return the complete elliptic integral of the third kind
 *          @f$ \Pi(k,\nu) = \Pi(k,\nu,\pi/2) @f$ using the
 *          Carlson formulation.
 * 
 *   The complete elliptic integral of the third kind is defined as
 *   @f[
 *     \Pi(k,\nu) = \int_0^{\pi/2}
 *                   \frac{d\theta}
 *                 {(1 - \nu \sin^2\theta)\sqrt{1 - k^2 \sin^2\theta}}
 *   @f]
 * 
 *   @param  __k  The argument of the elliptic function.
 *   @param  __nu  The second argument of the elliptic function.
 *   @return  The complete elliptic function of the third kind.
 */
template<typename _Tp>
_Tp
__comp_ellint_3(const _Tp __k, const _Tp __nu)
{

  if (std::isnan(__k) || std::isnan(__nu))
    return std::numeric_limits<_Tp>::quiet_NaN();
  else if (__nu == _Tp(1))
    return std::numeric_limits<_Tp>::infinity();
  else if (std::abs(__k) > _Tp(1))
    throw std::domain_error("Bad argument in __comp_ellint_3.");
  else
    {
      const _Tp __kk = __k * __k;

      return __ellint_rf(_Tp(0), _Tp(1) - __kk, _Tp(1))
           - __nu
           * __ellint_rj(_Tp(0), _Tp(1) - __kk, _Tp(1), _Tp(1) + __nu)
           / _Tp(3);
    }
}


/**
 *   @brief Return the incomplete elliptic integral of the third kind
 *          @f$ \Pi(k,\nu,\phi) @f$ using the Carlson formulation.
 * 
 *   The incomplete elliptic integral of the third kind is defined as
 *   @f[
 *     \Pi(k,\nu,\phi) = \int_0^{\phi}
 *                       \frac{d\theta}
 *                            {(1 - \nu \sin^2\theta)
 *                             \sqrt{1 - k^2 \sin^2\theta}}
 *   @f]
 * 
 *   @param  __k  The argument of the elliptic function.
 *   @param  __nu  The second argument of the elliptic function.
 *   @param  __phi  The integral limit argument of the elliptic function.
 *   @return  The elliptic function of the third kind.
 */
template<typename _Tp>
_Tp
__ellint_3(const _Tp __k, const _Tp __nu, const _Tp __phi)
{

  if (std::isnan(__k) || std::isnan(__nu) || std::isnan(__phi))
    return std::numeric_limits<_Tp>::quiet_NaN();
  else if (std::abs(__k) > _Tp(1))
    throw std::domain_error("Bad argument in __ellint_3.");
  else
    {
      //  Reduce phi to -pi/2 < phi < +pi/2.
      const int __n = std::floor(__phi / _Tp(math::Pi_d)
                               + _Tp(0.5L));
      const _Tp __phi_red = __phi
                          - __n * _Tp(math::Pi_d);

      const _Tp __kk = __k * __k;
      const _Tp __s = std::sin(__phi_red);
      const _Tp __ss = __s * __s;
      const _Tp __sss = __ss * __s;
      const _Tp __c = std::cos(__phi_red);
      const _Tp __cc = __c * __c;

      const _Tp __Pi = __s
                     * __ellint_rf(__cc, _Tp(1) - __kk * __ss, _Tp(1))
                     - __nu * __sss
                     * __ellint_rj(__cc, _Tp(1) - __kk * __ss, _Tp(1),
                                   _Tp(1) + __nu * __ss) / _Tp(3);

      if (__n == 0)
        return __Pi;
      else
        return __Pi + _Tp(2) * __n * __comp_ellint_3(__k, __nu);
    }
}


/*
   A subset of cephes math routines
   Redistributed under the BSD license with permission of the author, see
   https://github.com/deepmind/torch-cephes/blob/master/LICENSE.txt
*/

template <typename Scalar> Scalar chbevl(Scalar x, const Scalar *array, int n) {
    const Scalar *p = array;
    Scalar b0 = *p++;
    Scalar b1 = 0;
    Scalar b2;
    int i = n - 1;

    do {
        b2 = b1;
        b1 = b0;
        b0 = x * b1  -  b2  + *p++;
    } while (--i);

    return (Scalar) 0.5 * (b0-b2);
}

template <typename Scalar> Scalar i0e(Scalar x) {
    /* Chebyshev coefficients for exp(-x) I0(x)
     * in the interval [0,8].
     *
     * lim(x->0){ exp(-x) I0(x) } = 1.
     */
    static const Scalar A[] = {
        -4.41534164647933937950E-18, 3.33079451882223809783E-17,
        -2.43127984654795469359E-16, 1.71539128555513303061E-15,
        -1.16853328779934516808E-14, 7.67618549860493561688E-14,
        -4.85644678311192946090E-13, 2.95505266312963983461E-12,
        -1.72682629144155570723E-11, 9.67580903537323691224E-11,
        -5.18979560163526290666E-10, 2.65982372468238665035E-9,
        -1.30002500998624804212E-8,  6.04699502254191894932E-8,
        -2.67079385394061173391E-7,  1.11738753912010371815E-6,
        -4.41673835845875056359E-6,  1.64484480707288970893E-5,
        -5.75419501008210370398E-5,  1.88502885095841655729E-4,
        -5.76375574538582365885E-4,  1.63947561694133579842E-3,
        -4.32430999505057594430E-3,  1.05464603945949983183E-2,
        -2.37374148058994688156E-2,  4.93052842396707084878E-2,
        -9.49010970480476444210E-2,  1.71620901522208775349E-1,
        -3.04682672343198398683E-1,  6.76795274409476084995E-1
    };

    /* Chebyshev coefficients for exp(-x) sqrt(x) I0(x)
     * in the inverted interval [8,infinity].
     *
     * lim(x->inf){ exp(-x) sqrt(x) I0(x) } = 1/sqrt(2pi).
     */
    static const Scalar B[] = {
        -7.23318048787475395456E-18, -4.83050448594418207126E-18,
         4.46562142029675999901E-17,  3.46122286769746109310E-17,
        -2.82762398051658348494E-16, -3.42548561967721913462E-16,
         1.77256013305652638360E-15,  3.81168066935262242075E-15,
        -9.55484669882830764870E-15, -4.15056934728722208663E-14,
         1.54008621752140982691E-14,  3.85277838274214270114E-13,
         7.18012445138366623367E-13, -1.79417853150680611778E-12,
        -1.32158118404477131188E-11, -3.14991652796324136454E-11,
         1.18891471078464383424E-11,  4.94060238822496958910E-10,
         3.39623202570838634515E-9,   2.26666899049817806459E-8,
         2.04891858946906374183E-7,   2.89137052083475648297E-6,
         6.88975834691682398426E-5,   3.36911647825569408990E-3,
         8.04490411014108831608E-1
    };

    if (x < 0)
        x = -x;
    if (x <= 8) {
        Scalar y = (x * (Scalar) 0.5) - (Scalar) 2.0;
        return chbevl(y, A, 30);
    } else {
        return chbevl((Scalar) 32.0/x - (Scalar) 2.0, B, 25) / std::sqrt(x);
    }
}

template <typename Scalar> Scalar erfinv(Scalar x) {
    // Based on "Approximating the erfinv function" by Mark Giles
    Scalar w = -std::log(((Scalar) 1 - x)*((Scalar) 1 + x));
    Scalar p;
    if (w < (Scalar) 5) {
        w = w - (Scalar) 2.5;
        p = (Scalar) 2.81022636e-08;
        p = (Scalar) 3.43273939e-07 + p*w;
        p = (Scalar) -3.5233877e-06 + p*w;
        p = (Scalar) -4.39150654e-06 + p*w;
        p = (Scalar) 0.00021858087 + p*w;
        p = (Scalar) -0.00125372503 + p*w;
        p = (Scalar) -0.00417768164 + p*w;
        p = (Scalar) 0.246640727 + p*w;
        p = (Scalar) 1.50140941 + p*w;
    } else {
        w = std::sqrt(w) - (Scalar) 3;
        p = (Scalar) -0.000200214257;
        p = (Scalar) 0.000100950558 + p*w;
        p = (Scalar) 0.00134934322 + p*w;
        p = (Scalar) -0.00367342844 + p*w;
        p = (Scalar) 0.00573950773 + p*w;
        p = (Scalar) -0.0076224613 + p*w;
        p = (Scalar) 0.00943887047 + p*w;
        p = (Scalar) 1.00167406 + p*w;
        p = (Scalar) 2.83297682 + p*w;
    }
    return p*x;
}

template <typename Scalar> Scalar erf(Scalar x) {
    const Scalar a1 = (const Scalar)  0.254829592;
    const Scalar a2 = (const Scalar) -0.284496736;
    const Scalar a3 = (const Scalar)  1.421413741;
    const Scalar a4 = (const Scalar) -1.453152027;
    const Scalar a5 = (const Scalar)  1.061405429;
    const Scalar p  = (const Scalar)  0.3275911;

    // Save the sign of x
    Scalar sign = math::signum(x);
    x = std::abs(x);

    // A&S formula 7.1.26
    Scalar t = (Scalar) 1.0 / ((Scalar) 1.0 + p*x);
    Scalar y = (Scalar) 1.0 - (((((a5*t + a4)*t) + a3)*t + a2)*t + a1)*t*std::exp(-x*x);

    return sign*y;
}


/// Evaluate the l-th Legendre polynomial using recurrence
template <typename Scalar>
Scalar legendre_p(int l, Scalar x) {
    assert(l >= 0);

    if (l == 0) {
        return 1;
    } else if (l == 1) {
        return x;
    } else {
        Scalar Lppred = 1, Lpred = x, Lcur = 0;

        for (int k = 2; k <= l; ++k) {
            Lcur = ((2*k-1) * x * Lpred - (k - 1) * Lppred) / k;
            Lppred = Lpred; Lpred = Lcur;
        }

        return Lcur;
    }
}

/// Evaluate an associated Legendre polynomial using recurrence
template <typename Scalar>
Scalar legendre_p(int l, int m, Scalar x) {
    Scalar p_mm = 1;

    if (m > 0) {
        Scalar somx2 = std::sqrt((1 - x) * (1 + x));
        Scalar fact = 1;
        for (int i=1; i<=m; i++) {
            p_mm *= (-fact) * somx2;
            fact += 2;
        }
    }

    if (l == m)
        return p_mm;

    Scalar p_mmp1 = x * (2*m + 1) * p_mm;
    if (l == m+1)
        return p_mmp1;

    Scalar p_ll = 0;
    for (int ll=m+2; ll <= l; ++ll) {
        p_ll = ((2*ll-1)*x*p_mmp1 - (ll+m-1) * p_mm) / (ll-m);
        p_mm = p_mmp1;
        p_mmp1 = p_ll;
    }

    return p_ll;
}

/// Evaluate the l-th Legendre polynomial and its derivative using recurrence
template <typename Scalar>
std::pair<Scalar, Scalar> legendre_pd(int l, Scalar x) {
    assert(l >= 0);

    if (l == 0) {
        return std::make_pair(1, 0);
    } else if (l == 1) {
        return std::make_pair(x, 1);
    } else {
        Scalar Lppred = 1, Lpred = x, Lcur = 0,
               Dppred = 0, Dpred = 1, Dcur = 0;

        for (int k = 2; k <= l; ++k) {
            Lcur = ((2*k-1) * x * Lpred - (k - 1) * Lppred) / k;
            Dcur = Dppred + (2*k-1) * Lpred;
            Lppred = Lpred; Lpred = Lcur;
            Dppred = Dpred; Dpred = Dcur;
        }

        return std::make_pair(Lcur, Dcur);
    }
}

/// Evaluate the function legendre_pd(l+1, x) - legendre_pd(l-1, x)
template <typename Scalar>
static std::pair<Scalar, Scalar> legendre_pd_diff(int l, Scalar x) {
    assert(l >= 1);

    if (l == 1) {
        return std::make_pair((Scalar) 0.5 * (3*x*x-1) - 1, 3*x);
    } else {
        Scalar Lppred = 1, Lpred = x, Lcur = 0,
               Dppred = 0, Dpred = 1, Dcur = 0;

        for (int k = 2; k <= l; ++k) {
            Lcur = ((2*k-1) * x * Lpred - (k-1) * Lppred) / k;
            Dcur = Dppred + (2*k-1) * Lpred;
            Lppred = Lpred; Lpred = Lcur;
            Dppred = Dpred; Dpred = Dcur;
        }

        Scalar Lnext = ((2*l+1) * x * Lpred - l * Lppred) / (l+1);
        Scalar Dnext = Dppred + (2*l+1) * Lpred;

        return std::make_pair(Lnext - Lppred, Dnext - Dppred);
    }
}


NAMESPACE_END(detail)

double normal_cdf(double v) { return detail::normal_cdf(v); }
float  normal_cdf(float v) { return detail::normal_cdf(v); }
double normal_quantile(double v) { return detail::normal_quantile(v); }
float  normal_quantile(float v) { return detail::normal_quantile(v); }

double comp_ellint_1(double k) { return detail::__comp_ellint_1(k); }
double ellint_1(double k, double phi) { return detail::__ellint_1(k, phi); }
double comp_ellint_2(double k) { return detail::__comp_ellint_2(k); }
double ellint_2(double k, double phi) { return detail::__ellint_2(k, phi); }
double comp_ellint_3(double k, double nu) { return detail::__comp_ellint_3(k, nu); }
double ellint_3(double k, double nu, double phi) { return detail::__ellint_3(k, nu, phi); }

float comp_ellint_1(float k) { return detail::__comp_ellint_1(k); }
float ellint_1(float k, float phi) { return detail::__ellint_1(k, phi); }
float comp_ellint_2(float k) { return detail::__comp_ellint_2(k); }
float ellint_2(float k, float phi) { return detail::__ellint_2(k, phi); }
float comp_ellint_3(float k, float nu) { return detail::__comp_ellint_3(k, nu); }
float ellint_3(float k, float nu, float phi) { return detail::__ellint_3(k, nu, phi); }

float i0e(float x) { return detail::i0e(x); }
double i0e(double x) { return detail::i0e(x); }
float erf(float x) { return detail::erf(x); }
double erf(double x) { return detail::erf(x); }
float erfinv(float x) { return detail::erfinv(x); }
double erfinv(double x) { return detail::erfinv(x); }

float legendre_p(int l, float x) { return detail::legendre_p(l, x); }
double legendre_p(int l, double x) { return detail::legendre_p(l, x); }
float legendre_p(int l, int m, float x) { return detail::legendre_p(l, m, x); }
double legendre_p(int l, int m, double x) { return detail::legendre_p(l, m, x); }
std::pair<float, float> legendre_pd(int l, float x) { return detail::legendre_pd(l, x); }
std::pair<double, double> legendre_pd(int l, double x) { return detail::legendre_pd(l, x); }
std::pair<float, float> legendre_pd_diff(int l, float x) { return detail::legendre_pd_diff(l, x); }
std::pair<double, double> legendre_pd_diff(int l, double x) { return detail::legendre_pd_diff(l, x); }

NAMESPACE_END(math)
NAMESPACE_END(mitsuba)
