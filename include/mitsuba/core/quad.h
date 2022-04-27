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
 *     Desired number of evaluation points
 *
 * \return
 *     A tuple (nodes, weights) storing the nodes and weights of the
 *     quadrature rule.
 */
template <typename Float>
std::pair<Float, Float> gauss_legendre(int n) {
    static_assert(dr::is_dynamic_v<Float>, "Template type must be dynamic!");
    using ScalarFloat = dr::scalar_t<Float>;

    if (n < 1)
        throw std::runtime_error("gauss_legendre(): n must be >= 1");

    std::vector<ScalarFloat> nodes(n), weights(n);

    n--;

    if (n == 0) {
        nodes[0] = (ScalarFloat) 0;
        weights[0] = (ScalarFloat) 2;
    } else if (n == 1) {
        nodes[0] = (ScalarFloat) -dr::sqrt(1.0 / 3.0);
        nodes[1] = -nodes[0];
        weights[0] = weights[1] = (ScalarFloat) 1;
    }

    int m = (n + 1) / 2;
    for (int i = 0; i < m; ++i) {
        // Initial guess for this root using that of a Chebyshev polynomial
        double x = -dr::cos((double) (2*i + 1) / (double) (2*n + 2) * dr::Pi<double>);
        int it = 0;

        while (true) {
            if (++it > 20)
                throw std::runtime_error(
                    "gauss_lobatto(" + std::to_string(n) +
                    "): did not converge after 20 iterations!");

            // Search for the interior roots of P_{n+1}(x) using Newton's method.
            std::pair<double, double> L = math::legendre_pd(n+1, x);
            double step = L.first / L.second;
            x -= step;

            if (dr::abs(step) <= 4 * dr::abs(x) * dr::Epsilon<double>)
                break;
        }

        std::pair<double, double> L = math::legendre_pd(n+1, x);
        weights[i] = weights[n - i] =
            (ScalarFloat)(2 / ((1 - x * x) * (L.second * L.second)));
        nodes[i] = (ScalarFloat) x;
        nodes[n - i] = (ScalarFloat) -x;
        assert(i == 0 || (ScalarFloat) x > nodes[i-1]);
    }

    if ((n % 2) == 0) {
        std::pair<double, double> L = math::legendre_pd(n+1, 0.0);
        weights[n / 2] = (ScalarFloat) (2.0 / (L.second * L.second));
        nodes[n / 2] = (ScalarFloat) 0;
    }

    return {
        dr::load<Float>(nodes.data(), nodes.size()),
        dr::load<Float>(weights.data(), weights.size())
    };
}

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
 *     Desired number of evaluation points
 *
 * \return
 *     A tuple (nodes, weights) storing the nodes and weights of the
 *     quadrature rule.
 */
template <typename Float>
std::pair<Float, Float> gauss_lobatto(int n) {
    static_assert(dr::is_dynamic_v<Float>, "Template type must be dynamic!");
    using ScalarFloat = dr::scalar_t<Float>;

    if (n < 2)
        throw std::runtime_error("gauss_lobatto(): n must be >= 2");

    std::vector<ScalarFloat> nodes(n), weights(n);

    n--;
    nodes[0] = -1;
    nodes[n] =  1;
    weights[0] = weights[n] = 2 / (ScalarFloat) (n * (n+1));


    int m = (n + 1) / 2;
    for (int i = 1; i < m; ++i) {
        /* Initial guess for this root -- see "On the Legendre-Gauss-Lobatto Points
           and Weights" by Seymor V. Parter, Journal of Sci. Comp., Vol. 14, 4, 1999 */

        double x = -dr::cos((i + 0.25) * dr::Pi<double> / n -
                            3 / (8 * n * dr::Pi<double> * (i + 0.25)));
        int it = 0;

        while (true) {
            if (++it > 20)
                throw std::runtime_error("gauss_lobatto(" + std::to_string(n) +
                                         "): did not converge after 20 iterations!");

            /* Search for the interior roots of P_n'(x) using Newton's method. The same
               roots are also shared by P_{n+1}-P_{n-1}, which is nicer to evaluate. */

            std::pair<double, double> Q = math::legendre_pd_diff(n, x);
            double step = Q.first / Q.second;
            x -= step;

            if (dr::abs(step) <= 4 * dr::abs(x) * dr::Epsilon<double>)
                break;
        }

        double l_n = math::legendre_p(n, x);
        weights[i] = weights[n - i] = (ScalarFloat) (2 / ((n * (n + 1)) * l_n * l_n));
        nodes[i] = (ScalarFloat) x;
        nodes[n - i] = (ScalarFloat) -x;
        assert((ScalarFloat) x > nodes[i-1]);
    }

    if ((n % 2) == 0) {
        double l_n = math::legendre_p(n, 0.0);
        weights[n / 2] = (ScalarFloat) (2 / ((n * (n + 1)) * l_n * l_n));
        nodes[n/2] = 0;
    }

    return {
        dr::load<Float>(nodes.data(), nodes.size()),
        dr::load<Float>(weights.data(), weights.size())
    };
}

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
 *     Desired number of evaluation points. Must be an odd number bigger than 3.
 *
 * \return
 *     A tuple (nodes, weights) storing the nodes and weights of the
 *     quadrature rule.
 */
template <typename Float>
std::pair<Float, Float> composite_simpson(int n) {
    static_assert(dr::is_dynamic_v<Float>, "Template type must be dynamic!");
    using ScalarFloat = dr::scalar_t<Float>;

    if (n % 2 != 1 || n < 3)
        throw std::runtime_error("composite_simpson(): n must be >= 3 and odd");

    std::vector<ScalarFloat> nodes(n), weights(n);

    n = (n - 1) / 2;

    ScalarFloat h      = (ScalarFloat) 2 / (ScalarFloat) (2 * n),
                weight = h * (ScalarFloat) (1.0 / 3.0);

    for (int i = 0; i < n; ++i) {
        ScalarFloat x  = -1 + h * (2*i);
        nodes[2*i]     = x;
        nodes[2*i+1]   = x+h;
        weights[2*i]   = (i == 0 ? 1 : 2) * weight;
        weights[2*i+1] = 4 * weight;
    }

    nodes[2*n] = 1;
    weights[2*n] = weight;

    return {
        dr::load<Float>(nodes.data(), nodes.size()),
        dr::load<Float>(weights.data(), weights.size())
    };
}

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
 *     Desired number of evaluation points. Must be an odd number bigger than 3.
 *
 * \return
 *     A tuple (nodes, weights) storing the nodes and weights of the
 *     quadrature rule.
 */
template <typename Float>
std::pair<Float, Float> composite_simpson_38(int n) {
    static_assert(dr::is_dynamic_v<Float>, "Template type must be dynamic!");
    using ScalarFloat = dr::scalar_t<Float>;

    if ((n - 1) % 3 != 0 || n < 4)
        throw std::runtime_error("composite_simpson_38(): n-1 must be divisible by 3");

    std::vector<ScalarFloat> nodes(n), weights(n);

    n = (n - 1) / 3;

    ScalarFloat h      = (ScalarFloat) 2 / (ScalarFloat) (3 * n),
                weight = h * (ScalarFloat) (3.0 / 8.0);

    for (int i = 0; i < n; ++i) {
        ScalarFloat x  = -1 + h * (3*i);
        nodes[3*i]     = x;
        nodes[3*i+1]   = x+h;
        nodes[3*i+2]   = x+2*h;
        weights[3*i]   = (i == 0 ? 1 : 2) * weight;
        weights[3*i+1] = 3 * weight;
        weights[3*i+2] = 3 * weight;
    }

    nodes[3*n] = 1;
    weights[3*n] = weight;

    return {
        dr::load<Float>(nodes.data(), nodes.size()),
        dr::load<Float>(weights.data(), weights.size())
    };
}

/**
 * \brief Computes the Chebyshev nodes, i.e. the roots of the Chebyshev
 * polynomials of the first kind
 *
 * The output array contains positions on the interval \f$[-1, 1]\f$.
 *
 * \param n
 *     Desired number of points
 */
template <typename Float> Float chebyshev(int n) {
    using ScalarFloat = dr::scalar_t<Float>;
    static_assert(dr::is_dynamic_v<Float>, "Template type must be dynamic!");
    ScalarFloat eps = 1 / ScalarFloat(2 * n);
    return -dr::cos(dr::linspace<Float>(eps, 1 - eps, n) * dr::Pi<Float>);
}

NAMESPACE_END(quad)
NAMESPACE_END(mitsuba)
