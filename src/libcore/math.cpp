#include <mitsuba/core/math.h>
#include <enoki/morton.h>
#include <stdexcept>
#include <cassert>

#ifdef _MSC_VER
#  pragma warning(disable: 4305 4838 4244) // 8: conversion from 'double' to 'const float' requires a narrowing conversion, etc.
#endif

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(math)
NAMESPACE_BEGIN(detail)
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
template<typename Type> Type ellint_rf(const Type x, const Type y, const Type z) {
  const Type min = std::numeric_limits<Type>::min();
  const Type lolim = Type(5) * min;

  if (x < Type(0) || y < Type(0) || z < Type(0))
    throw std::domain_error("Argument less than zero "
                                  "in ellint_rf.");
  else if (x + y < lolim || x + z < lolim
        || y + z < lolim)
    throw std::domain_error("Argument too small in ellint_rf");
  else {
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
      for (unsigned int iter = 0; iter < max_iter; ++iter) {
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
template<typename Type> Type comp_ellint_1_series(const Type k) {

  const Type kk = k * k;

  Type term = kk / Type(4);
  Type sum = Type(1) + term;

  const unsigned int max_iter = 1000;
  for (unsigned int i = 2; i < max_iter; ++i) {
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
template<typename Type> Type comp_ellint_1(const Type k) {
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
template<typename Type> Type ellint_1(const Type k, const Type phi) {
  if (std::isnan(k) || std::isnan(phi))
    return std::numeric_limits<Type>::quiet_NaN();
  else if (std::abs(k) > Type(1))
    throw std::domain_error("Bad argument in ellint_1.");
  else {
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
template<typename Type> Type comp_ellint_2_series(const Type k) {
  const Type kk = k * k;

  Type term = kk;
  Type sum = term;

  const unsigned int max_iter = 1000;
  for (unsigned int i = 2; i < max_iter; ++i) {
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
template<typename Type> Type ellint_rd(const Type x, const Type y, const Type z) {
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
  else {
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
      for (unsigned int iter = 0; iter < max_iter; ++iter) {
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
template<typename Type> Type comp_ellint_2(const Type k) {
  if (std::isnan(k))
    return std::numeric_limits<Type>::quiet_NaN();
  else if (std::abs(k) == 1)
    return Type(1);
  else if (std::abs(k) > Type(1))
    throw std::domain_error("Bad argument in comp_ellint_2.");
  else {
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
template<typename Type> Type ellint_2(const Type k, const Type phi) {
  if (std::isnan(k) || std::isnan(phi))
    return std::numeric_limits<Type>::quiet_NaN();
  else if (std::abs(k) > Type(1))
    throw std::domain_error("Bad argument in ellint_2.");
  else {
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
template<typename Type> Type ellint_rc(const Type x, const Type y) {
  const Type min = std::numeric_limits<Type>::min();
  const Type lolim = Type(5) * min;

  if (x < Type(0) || y < Type(0) || x + y < lolim)
    throw std::domain_error("Argument less than zero "
                                  "in ellint_rc.");
  else {
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
      for (unsigned int iter = 0; iter < max_iter; ++iter) {
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
template<typename Type> Type ellint_rj(const Type x, const Type y, const Type z, const Type p) {
  const Type min = std::numeric_limits<Type>::min();
  const Type lolim = std::pow(Type(5) * min, Type(1)/Type(3));

  if (x < Type(0) || y < Type(0) || z < Type(0))
    throw std::domain_error("Argument less than zero "
                                  "in ellint_rj.");
  else if (x + y < lolim || x + z < lolim
        || y + z < lolim || p < lolim)
    throw std::domain_error("Argument too small "
                                  "in ellint_rj");
  else {
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
      for (unsigned int iter = 0; iter < max_iter; ++iter) {
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
template<typename Type> Type comp_ellint_3(const Type k, const Type nu) {
  if (std::isnan(k) || std::isnan(nu))
    return std::numeric_limits<Type>::quiet_NaN();
  else if (nu == Type(1))
    return std::numeric_limits<Type>::infinity();
  else if (std::abs(k) > Type(1))
    throw std::domain_error("Bad argument in comp_ellint_3.");
  else {
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
template<typename Type> Type ellint_3(const Type k, const Type nu, const Type phi) {
  if (std::isnan(k) || std::isnan(nu) || std::isnan(phi))
    return std::numeric_limits<Type>::quiet_NaN();
  else if (std::abs(k) > Type(1))
    throw std::domain_error("Bad argument in ellint_3.");
  else {
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

NAMESPACE_END(detail)

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


template MTS_EXPORT_CORE Float  legendre_p(int, Float);
template MTS_EXPORT_CORE FloatP legendre_p(int, FloatP);
template MTS_EXPORT_CORE Float  legendre_p(int, int, Float);
template MTS_EXPORT_CORE FloatP legendre_p(int, int, FloatP);
template MTS_EXPORT_CORE std::pair<Float, Float>   legendre_pd(int, Float);
template MTS_EXPORT_CORE std::pair<FloatP, FloatP> legendre_pd(int, FloatP);
template MTS_EXPORT_CORE std::pair<Float, Float>   legendre_pd_diff(int, Float);
template MTS_EXPORT_CORE std::pair<FloatP, FloatP> legendre_pd_diff(int, FloatP);

template MTS_EXPORT_CORE std::tuple<mask_t<Float>,  Float,  Float>  solve_quadratic(Float,  Float,  Float);
template MTS_EXPORT_CORE std::tuple<mask_t<FloatP>, FloatP, FloatP> solve_quadratic(FloatP, FloatP, FloatP);

NAMESPACE_END(math)
NAMESPACE_END(mitsuba)

NAMESPACE_BEGIN(enoki)

using mitsuba::UInt32P;
using mitsuba::UInt64P;

template MTS_EXPORT_CORE uint32_t morton_encode(Array<uint32_t, 2>);
template MTS_EXPORT_CORE UInt32P  morton_encode(Array<UInt32P, 2>);
template MTS_EXPORT_CORE uint32_t morton_encode(Array<uint32_t, 3>);
template MTS_EXPORT_CORE UInt32P  morton_encode(Array<UInt32P, 3>);

template MTS_EXPORT_CORE uint64_t morton_encode(Array<uint64_t, 2>);
template MTS_EXPORT_CORE UInt64P  morton_encode(Array<UInt64P, 2>);
template MTS_EXPORT_CORE uint64_t morton_encode(Array<uint64_t, 3>);
template MTS_EXPORT_CORE UInt64P  morton_encode(Array<UInt64P, 3>);

template MTS_EXPORT_CORE Array<uint32_t, 2> morton_decode(uint32_t);
template MTS_EXPORT_CORE Array<UInt32P,  2> morton_decode(UInt32P);
template MTS_EXPORT_CORE Array<uint32_t, 3> morton_decode(uint32_t);
template MTS_EXPORT_CORE Array<UInt32P,  3> morton_decode(UInt32P);

template MTS_EXPORT_CORE Array<uint64_t, 2> morton_decode(uint64_t);
template MTS_EXPORT_CORE Array<UInt64P,  2> morton_decode(UInt64P);
template MTS_EXPORT_CORE Array<uint64_t, 3> morton_decode(uint64_t);
template MTS_EXPORT_CORE Array<UInt64P,  3> morton_decode(UInt64P);

NAMESPACE_END(enoki)
