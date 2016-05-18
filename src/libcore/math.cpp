#include <mitsuba/core/math.h>
#include <stdexcept>
#include <cassert>

#ifdef _MSC_VER
#  pragma warning(disable: 4305 4838 4244) // 8: conversion from 'double' to 'const float' requires a narrowing conversion, etc.
#endif

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
        return (u < 0 ? (Scalar) 0 : (Scalar) 1);
    y = std::abs(u);

    if (y <= (Scalar) 0.46875 * (Scalar) math::SqrtTwo_d) {
        /* evaluate erf() for |u| <= sqrt(2)*0.46875 */
        z = y*y;
        y = u*((((a[0]*z+a[1])*z+a[2])*z+a[3])*z+a[4])
             /((((b[0]*z+b[1])*z+b[2])*z+b[3])*z+b[4]);
        return (Scalar) 0.5 + y;
    }

    z = std::exp(-y*y/2)/2;
    if (y <= 4) {
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
    return (u < 0 ? y : 1-y);
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

    return p > (Scalar) 0.5 ? -u : u;
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
 *   @param  x  The first of three symmetric arguments.
 *   @param  y  The second of three symmetric arguments.
 *   @param  z  The third of three symmetric arguments.
 *   @return  The Carlson elliptic function of the first kind.
 */
template<typename Type>
Type
ellint_rf(const Type x, const Type y, const Type z)
{
  const Type min = std::numeric_limits<Type>::min();
  const Type lolim = Type(5) * min;

  if (x < Type(0) || y < Type(0) || z < Type(0))
    throw std::domain_error("Argument less than zero "
                                  "in ellint_rf.");
  else if (x + y < lolim || x + z < lolim
        || y + z < lolim)
    throw std::domain_error("Argument too small in ellint_rf");
  else
    {
      const Type c0 = Type(1) / Type(4);
      const Type c1 = Type(1) / Type(24);
      const Type c2 = Type(1) / Type(10);
      const Type c3 = Type(3) / Type(44);
      const Type c4 = Type(1) / Type(14);

      Type xn = x;
      Type yn = y;
      Type zn = z;

      const Type eps = std::numeric_limits<Type>::epsilon();
      const Type errtol = std::pow(eps, Type(1) / Type(6));
      Type mu;
      Type xndev, yndev, zndev;

      const unsigned int max_iter = 100;
      for (unsigned int iter = 0; iter < max_iter; ++iter)
        {
          mu = (xn + yn + zn) / Type(3);
          xndev = 2 - (mu + xn) / mu;
          yndev = 2 - (mu + yn) / mu;
          zndev = 2 - (mu + zn) / mu;
          Type epsilon = std::max(std::abs(xndev), std::abs(yndev));
          epsilon = std::max(epsilon, std::abs(zndev));
          if (epsilon < errtol)
            break;
          const Type xnroot = std::sqrt(xn);
          const Type ynroot = std::sqrt(yn);
          const Type znroot = std::sqrt(zn);
          const Type lambda = xnroot * (ynroot + znroot)
                             + ynroot * znroot;
          xn = c0 * (xn + lambda);
          yn = c0 * (yn + lambda);
          zn = c0 * (zn + lambda);
        }

      const Type e2 = xndev * yndev - zndev * zndev;
      const Type e3 = xndev * yndev * zndev;
      const Type s  = Type(1) + (c1 * e2 - c2 - c3 * e3) * e2
               + c4 * e3;

      return s / std::sqrt(mu);
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
 *   @param  k  The argument of the complete elliptic function.
 *   @return  The complete elliptic function of the first kind.
 */
template<typename Type>
Type
comp_ellint_1_series(const Type k)
{

  const Type kk = k * k;

  Type term = kk / Type(4);
  Type sum = Type(1) + term;

  const unsigned int max_iter = 1000;
  for (unsigned int i = 2; i < max_iter; ++i)
    {
      term *= (2 * i - 1) * kk / (2 * i);
      if (term < std::numeric_limits<Type>::epsilon())
        break;
      sum += term;
    }

  return Type(0.5 * math::Pi_d) * sum;
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
 *   @param  k  The argument of the complete elliptic function.
 *   @return  The complete elliptic function of the first kind.
 */
template<typename Type>
Type
comp_ellint_1(const Type k)
{

  if (std::isnan(k))
    return std::numeric_limits<Type>::quiet_NaN();
  else if (std::abs(k) >= Type(1))
    return std::numeric_limits<Type>::quiet_NaN();
  else
    return ellint_rf(Type(0), Type(1) - k * k, Type(1));
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
 *   @param  k  The argument of the elliptic function.
 *   @param  phi  The integral limit argument of the elliptic function.
 *   @return  The elliptic function of the first kind.
 */
template<typename Type>
Type
ellint_1(const Type k, const Type phi)
{

  if (std::isnan(k) || std::isnan(phi))
    return std::numeric_limits<Type>::quiet_NaN();
  else if (std::abs(k) > Type(1))
    throw std::domain_error("Bad argument in ellint_1.");
  else
    {
      //  Reduce phi to -pi/2 < phi < +pi/2.
      const int n = std::floor(phi / Type(math::Pi_d)
                               + Type(0.5L));
      const Type phi_red = phi
                          - n * Type(math::Pi_d);

      const Type s = std::sin(phi_red);
      const Type c = std::cos(phi_red);

      const Type F = s
                    * ellint_rf(c * c,
                            Type(1) - k * k * s * s, Type(1));

      if (n == 0)
        return F;
      else
        return F + Type(2) * n * comp_ellint_1(k);
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
 *   @param  k  The argument of the complete elliptic function.
 *   @return  The complete elliptic function of the second kind.
 */
template<typename Type>
Type
comp_ellint_2_series(const Type k)
{

  const Type kk = k * k;

  Type term = kk;
  Type sum = term;

  const unsigned int max_iter = 1000;
  for (unsigned int i = 2; i < max_iter; ++i)
    {
      const Type i2m = 2 * i - 1;
      const Type i2 = 2 * i;
      term *= i2m * i2m * kk / (i2 * i2);
      if (term < std::numeric_limits<Type>::epsilon())
        break;
      sum += term / i2m;
    }

  return Type(0.5 * math::Pi_d) * (Type(1) - sum);
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
 *   @param  x  The first of two symmetric arguments.
 *   @param  y  The second of two symmetric arguments.
 *   @param  z  The third argument.
 *   @return  The Carlson elliptic function of the second kind.
 */
template<typename Type>
Type
ellint_rd(const Type x, const Type y, const Type z)
{
  const Type eps = std::numeric_limits<Type>::epsilon();
  const Type errtol = std::pow(eps / Type(8), Type(1) / Type(6));
  const Type max = std::numeric_limits<Type>::max();
  const Type lolim = Type(2) / std::pow(max, Type(2) / Type(3));

  if (x < Type(0) || y < Type(0))
    throw std::domain_error("Argument less than zero "
                                  "in ellint_rd.");
  else if (x + y < lolim || z < lolim)
    throw std::domain_error("Argument too small "
                                  "in ellint_rd.");
  else
    {
      const Type c0 = Type(1) / Type(4);
      const Type c1 = Type(3) / Type(14);
      const Type c2 = Type(1) / Type(6);
      const Type c3 = Type(9) / Type(22);
      const Type c4 = Type(3) / Type(26);

      Type xn = x;
      Type yn = y;
      Type zn = z;
      Type sigma = Type(0);
      Type power4 = Type(1);

      Type mu;
      Type xndev, yndev, zndev;

      const unsigned int max_iter = 100;
      for (unsigned int iter = 0; iter < max_iter; ++iter)
        {
          mu = (xn + yn + Type(3) * zn) / Type(5);
          xndev = (mu - xn) / mu;
          yndev = (mu - yn) / mu;
          zndev = (mu - zn) / mu;
          Type epsilon = std::max(std::abs(xndev), std::abs(yndev));
          epsilon = std::max(epsilon, std::abs(zndev));
          if (epsilon < errtol)
            break;
          Type xnroot = std::sqrt(xn);
          Type ynroot = std::sqrt(yn);
          Type znroot = std::sqrt(zn);
          Type lambda = xnroot * (ynroot + znroot)
                       + ynroot * znroot;
          sigma += power4 / (znroot * (zn + lambda));
          power4 *= c0;
          xn = c0 * (xn + lambda);
          yn = c0 * (yn + lambda);
          zn = c0 * (zn + lambda);
        }

      Type ea = xndev * yndev;
      Type eb = zndev * zndev;
      Type ec = ea - eb;
      Type ed = ea - Type(6) * eb;
      Type ef = ed + ec + ec;
      Type s1 = ed * (-c1 + c3 * ed
                               / Type(3) - Type(3) * c4 * zndev * ef
                               / Type(2));
      Type s2 = zndev
               * (c2 * ef
                + zndev * (-c3 * ec - zndev * c4 - ea));

      return Type(3) * sigma + power4 * (Type(1) + s1 + s2)
                                    / (mu * std::sqrt(mu));
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
 *   @param  k  The argument of the complete elliptic function.
 *   @return  The complete elliptic function of the second kind.
 */
template<typename Type>
Type
comp_ellint_2(const Type k)
{

  if (std::isnan(k))
    return std::numeric_limits<Type>::quiet_NaN();
  else if (std::abs(k) == 1)
    return Type(1);
  else if (std::abs(k) > Type(1))
    throw std::domain_error("Bad argument in comp_ellint_2.");
  else
    {
      const Type kk = k * k;

      return ellint_rf(Type(0), Type(1) - kk, Type(1))
           - kk * ellint_rd(Type(0), Type(1) - kk, Type(1)) / Type(3);
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
 *   @param  k  The argument of the elliptic function.
 *   @param  phi  The integral limit argument of the elliptic function.
 *   @return  The elliptic function of the second kind.
 */
template<typename Type>
Type
ellint_2(const Type k, const Type phi)
{

  if (std::isnan(k) || std::isnan(phi))
    return std::numeric_limits<Type>::quiet_NaN();
  else if (std::abs(k) > Type(1))
    throw std::domain_error("Bad argument in ellint_2.");
  else
    {
      //  Reduce phi to -pi/2 < phi < +pi/2.
      const int n = (int) std::floor(phi / Type(math::Pi_d)
                                   + Type(0.5L));
      const Type phi_red = phi
                          - n * Type(math::Pi_d);

      const Type kk = k * k;
      const Type s = std::sin(phi_red);
      const Type ss = s * s;
      const Type sss = ss * s;
      const Type c = std::cos(phi_red);
      const Type cc = c * c;

      const Type E_ = s
                    * ellint_rf(cc, Type(1) - kk * ss, Type(1))
                    - kk * sss
                    * ellint_rd(cc, Type(1) - kk * ss, Type(1))
                    / Type(3);

      if (n == 0)
        return E_;
      else
        return E_ + Type(2) * n * comp_ellint_2(k);
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
 *   @param  x  The first argument.
 *   @param  y  The second argument.
 *   @return  The Carlson elliptic function.
 */
template<typename Type>
Type
ellint_rc(const Type x, const Type y)
{
  const Type min = std::numeric_limits<Type>::min();
  const Type lolim = Type(5) * min;

  if (x < Type(0) || y < Type(0) || x + y < lolim)
    throw std::domain_error("Argument less than zero "
                                  "in ellint_rc.");
  else
    {
      const Type c0 = Type(1) / Type(4);
      const Type c1 = Type(1) / Type(7);
      const Type c2 = Type(9) / Type(22);
      const Type c3 = Type(3) / Type(10);
      const Type c4 = Type(3) / Type(8);

      Type xn = x;
      Type yn = y;

      const Type eps = std::numeric_limits<Type>::epsilon();
      const Type errtol = std::pow(eps / Type(30), Type(1) / Type(6));
      Type mu;
      Type sn;

      const unsigned int max_iter = 100;
      for (unsigned int iter = 0; iter < max_iter; ++iter)
        {
          mu = (xn + Type(2) * yn) / Type(3);
          sn = (yn + mu) / mu - Type(2);
          if (std::abs(sn) < errtol)
            break;
          const Type lambda = Type(2) * std::sqrt(xn) * std::sqrt(yn)
                         + yn;
          xn = c0 * (xn + lambda);
          yn = c0 * (yn + lambda);
        }

      Type s = sn * sn
              * (c3 + sn*(c1 + sn * (c4 + sn * c2)));

      return (Type(1) + s) / std::sqrt(mu);
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
 *   @param  x  The first of three symmetric arguments.
 *   @param  y  The second of three symmetric arguments.
 *   @param  z  The third of three symmetric arguments.
 *   @param  p  The fourth argument.
 *   @return  The Carlson elliptic function of the fourth kind.
 */
template<typename Type>
Type
ellint_rj(const Type x, const Type y, const Type z, const Type p)
{
  const Type min = std::numeric_limits<Type>::min();
  const Type lolim = std::pow(Type(5) * min, Type(1)/Type(3));

  if (x < Type(0) || y < Type(0) || z < Type(0))
    throw std::domain_error("Argument less than zero "
                                  "in ellint_rj.");
  else if (x + y < lolim || x + z < lolim
        || y + z < lolim || p < lolim)
    throw std::domain_error("Argument too small "
                                  "in ellint_rj");
  else
    {
      const Type c0 = Type(1) / Type(4);
      const Type c1 = Type(3) / Type(14);
      const Type c2 = Type(1) / Type(3);
      const Type c3 = Type(3) / Type(22);
      const Type c4 = Type(3) / Type(26);

      Type xn = x;
      Type yn = y;
      Type zn = z;
      Type pn = p;
      Type sigma = Type(0);
      Type power4 = Type(1);

      const Type eps = std::numeric_limits<Type>::epsilon();
      const Type errtol = std::pow(eps / Type(8), Type(1) / Type(6));

      Type mu;
      Type xndev, yndev, zndev, pndev;

      const unsigned int max_iter = 100;
      for (unsigned int iter = 0; iter < max_iter; ++iter)
        {
          mu = (xn + yn + zn + Type(2) * pn) / Type(5);
          xndev = (mu - xn) / mu;
          yndev = (mu - yn) / mu;
          zndev = (mu - zn) / mu;
          pndev = (mu - pn) / mu;
          Type epsilon = std::max(std::abs(xndev), std::abs(yndev));
          epsilon = std::max(epsilon, std::abs(zndev));
          epsilon = std::max(epsilon, std::abs(pndev));
          if (epsilon < errtol)
            break;
          const Type xnroot = std::sqrt(xn);
          const Type ynroot = std::sqrt(yn);
          const Type znroot = std::sqrt(zn);
          const Type lambda = xnroot * (ynroot + znroot)
                             + ynroot * znroot;
          const Type alpha1 = pn * (xnroot + ynroot + znroot)
                            + xnroot * ynroot * znroot;
          const Type alpha2 = alpha1 * alpha1;
          const Type beta = pn * (pn + lambda)
                                  * (pn + lambda);
          sigma += power4 * ellint_rc(alpha2, beta);
          power4 *= c0;
          xn = c0 * (xn + lambda);
          yn = c0 * (yn + lambda);
          zn = c0 * (zn + lambda);
          pn = c0 * (pn + lambda);
        }

      Type ea = xndev * (yndev + zndev) + yndev * zndev;
      Type eb = xndev * yndev * zndev;
      Type ec = pndev * pndev;
      Type e2 = ea - Type(3) * ec;
      Type e3 = eb + Type(2) * pndev * (ea - ec);
      Type s1 = Type(1) + e2 * (-c1 + Type(3) * c3 * e2 / Type(4)
                        - Type(3) * c4 * e3 / Type(2));
      Type s2 = eb * (c2 / Type(2)
               + pndev * (-c3 - c3 + pndev * c4));
      Type s3 = pndev * ea * (c2 - pndev * c3)
               - c2 * pndev * ec;

      return Type(3) * sigma + power4 * (s1 + s2 + s3)
                                         / (mu * std::sqrt(mu));
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
 *   @param  k  The argument of the elliptic function.
 *   @param  nu  The second argument of the elliptic function.
 *   @return  The complete elliptic function of the third kind.
 */
template<typename Type>
Type
comp_ellint_3(const Type k, const Type nu)
{

  if (std::isnan(k) || std::isnan(nu))
    return std::numeric_limits<Type>::quiet_NaN();
  else if (nu == Type(1))
    return std::numeric_limits<Type>::infinity();
  else if (std::abs(k) > Type(1))
    throw std::domain_error("Bad argument in comp_ellint_3.");
  else
    {
      const Type kk = k * k;

      return ellint_rf(Type(0), Type(1) - kk, Type(1))
           - nu
           * ellint_rj(Type(0), Type(1) - kk, Type(1), Type(1) + nu)
           / Type(3);
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
 *   @param  k  The argument of the elliptic function.
 *   @param  nu  The second argument of the elliptic function.
 *   @param  phi  The integral limit argument of the elliptic function.
 *   @return  The elliptic function of the third kind.
 */
template<typename Type>
Type
ellint_3(const Type k, const Type nu, const Type phi)
{

  if (std::isnan(k) || std::isnan(nu) || std::isnan(phi))
    return std::numeric_limits<Type>::quiet_NaN();
  else if (std::abs(k) > Type(1))
    throw std::domain_error("Bad argument in ellint_3.");
  else
    {
      //  Reduce phi to -pi/2 < phi < +pi/2.
      const int n = (int) std::floor(phi / Type(math::Pi_d)
                                   + Type(0.5L));
      const Type phi_red = phi
                          - n * Type(math::Pi_d);

      const Type kk = k * k;
      const Type s = std::sin(phi_red);
      const Type ss = s * s;
      const Type sss = ss * s;
      const Type c = std::cos(phi_red);
      const Type cc = c * c;

      const Type Pi_ = s
                     * ellint_rf(cc, Type(1) - kk * ss, Type(1))
                     - nu * sss
                     * ellint_rj(cc, Type(1) - kk * ss, Type(1),
                                   Type(1) + nu * ss) / Type(3);

      if (n == 0)
        return Pi_;
      else
        return Pi_ + Type(2) * n * comp_ellint_3(k, nu);
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
        return std::make_pair((Scalar) 1, (Scalar) 0);
    } else if (l == 1) {
        return std::make_pair(x, (Scalar) 1);
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

double comp_ellint_1(double k) { return detail::comp_ellint_1(k); }
double ellint_1(double k, double phi) { return detail::ellint_1(k, phi); }
double comp_ellint_2(double k) { return detail::comp_ellint_2(k); }
double ellint_2(double k, double phi) { return detail::ellint_2(k, phi); }
double comp_ellint_3(double k, double nu) { return detail::comp_ellint_3(k, nu); }
double ellint_3(double k, double nu, double phi) { return detail::ellint_3(k, nu, phi); }

float comp_ellint_1(float k) { return detail::comp_ellint_1(k); }
float ellint_1(float k, float phi) { return detail::ellint_1(k, phi); }
float comp_ellint_2(float k) { return detail::comp_ellint_2(k); }
float ellint_2(float k, float phi) { return detail::ellint_2(k, phi); }
float comp_ellint_3(float k, float nu) { return detail::comp_ellint_3(k, nu); }
float ellint_3(float k, float nu, float phi) { return detail::ellint_3(k, nu, phi); }

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
