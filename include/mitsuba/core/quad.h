#include <mitsuba/core/math.h>

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(quad)

/**
 * \brief Computes the nodes and weights of a Gauss-Legendre quadrature
 * (aka "Gaussian quadrature") rule with the given number of evaluations.
 *
 * Integration is over the interval \f$[-1, 1]\f$. Gauss-Legendre quadrature
 * maximizes the order of exactly integrable polynomials achieves this up to
 * degree \f$2n-1\f$ (where \f$n\f$ is the number of function evaluations).
 *
 * This method is numerically well-behaved until about \f$n=200\f$
 * and then becomes progressively less accurate. It is generally not a
 * good idea to go much higher---in any case, a composite or
 * adaptive integration scheme will be superior for large \f$n\f$.
 *
 * \param n
 *     Desired number of evalution points
 * \return
 *     A tuple (nodes, weights) storing the nodes and weights of the
 *     quadrature rule.
 */
template <typename FloatX>
extern MTS_EXPORT_CORE std::pair<FloatX, FloatX> gauss_legendre(int n);

/**
 * \brief Computes the nodes and weights of a Gauss-Lobatto quadrature
 * rule with the given number of evaluations.
 *
 * Integration is over the interval \f$[-1, 1]\f$. Gauss-Lobatto quadrature
 * is preferable to Gauss-Legendre quadrature whenever the endpoints of the
 * integration domain should explicitly be included. It maximizes the order
 * of exactly integrable polynomials subject to this constraint and achieves
 * this up to degree \f$2n-3\f$ (where \f$n\f$ is the number of function
 * evaluations).
 *
 * This method is numerically well-behaved until about \f$n=200\f$
 * and then becomes progressively less accurate. It is generally not a
 * good idea to go much higher---in any case, a composite or
 * adaptive integration scheme will be superior for large \f$n\f$.
 *
 * \param n
 *     Desired number of evalution points
 * \return
 *     A tuple (nodes, weights) storing the nodes and weights of the
 *     quadrature rule.
 */
template <typename FloatX>
extern MTS_EXPORT_CORE std::pair<FloatX, FloatX> gauss_lobatto(int n);

/**
 * \brief Computes the nodes and weights of a composite Simpson quadrature
 * rule with the given number of evaluations.
 *
 * Integration is over the interval \f$[-1, 1]\f$, which will be split into
 * \f$(n-1) / 2\f$ sub-intervals with overlapping endpoints. A 3-point
 * Simpson rule is applied per interval, which is exact for polynomials of
 * degree three or less.
 *
 * \param n
 *     Desired number of evalution points. Must be an odd number bigger than 3.
 * \return
 *     A tuple (nodes, weights) storing the nodes and weights of the
 *     quadrature rule.
 */
template <typename FloatX>
extern MTS_EXPORT_CORE std::pair<FloatX, FloatX> composite_simpson(int n);

/**
 * \brief Computes the nodes and weights of a composite Simpson 3/8 quadrature
 * rule with the given number of evaluations.
 *
 * Integration is over the interval \f$[-1, 1]\f$, which will be split into
 * \f$(n-1) / 3\f$ sub-intervals with overlapping endpoints. A 4-point
 * Simpson rule is applied per interval, which is exact for polynomials of
 * degree four or less.
 *
 * \param n
 *     Desired number of evalution points. Must be an odd number bigger than 3.
 * \return
 *     A tuple (nodes, weights) storing the nodes and weights of the
 *     quadrature rule.
 */
template <typename FloatX>
extern MTS_EXPORT_CORE std::pair<FloatX, FloatX> composite_simpson_38(int n);

NAMESPACE_END(quad)
NAMESPACE_END(mitsuba)
