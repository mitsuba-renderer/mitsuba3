#include <mitsuba/core/quad.h>

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(quad)

template <typename FloatX>
std::pair<FloatX, FloatX> gauss_legendre(int n) {
    using Float = scalar_t<FloatX>;
    if (n < 1)
        throw std::runtime_error("gauss_legendre(): n must be >= 1");

    FloatX nodes, weights;
    set_slices(nodes, n);
    set_slices(weights, n);

    n--;

    if (n == 0) {
        nodes[0] = (Float) 0;
        weights[0] = (Float) 2;
    } else if (n == 1) {
        nodes[0] = (Float) -std::sqrt(1.0 / 3.0);
        nodes[1] = -nodes[0];
        weights[0] = weights[1] = (Float) 1;
    }

    int m = (n + 1) / 2;
    for (int i = 0; i < m; ++i) {
        // Initial guess for this root using that of a Chebyshev polynomial
        double x = -std::cos((double) (2*i + 1) / (double) (2*n + 2) * math::Pi<double>);
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

            if (std::abs(step) <=
                4 * std::abs(x) * std::numeric_limits<double>::epsilon())
                break;
        }

        std::pair<double, double> L = math::legendre_pd(n+1, x);
        weights[i] = weights[n - i] =
            (Float)(2 / ((1 - x * x) * (L.second * L.second)));
        nodes[i] = (Float) x;
        nodes[n - i] = (Float) -x;
        assert(i == 0 || (Float) x > nodes[i-1]);
    }

    if ((n % 2) == 0) {
        std::pair<double, double> L = math::legendre_pd(n+1, 0.0);
        weights[n / 2] = (Float) (2.0 / (L.second * L.second));
        nodes[n/2] = (Float) 0;
    }

    return { nodes, weights };
}

template <typename FloatX>
std::pair<FloatX, FloatX> gauss_lobatto(int n) {
    using Float = scalar_t<FloatX>;
    if (n < 2)
        throw std::runtime_error("gauss_lobatto(): n must be >= 2");

    FloatX nodes, weights;
    set_slices(nodes, n);
    set_slices(weights, n);

    n--;
    nodes[0] = -1;
    nodes[n] =  1;
    weights[0] = weights[n] = 2 / (Float) (n * (n+1));


    int m = (n + 1) / 2;
    for (int i = 1; i < m; ++i) {
        /* Initial guess for this root -- see "On the Legendre-Gauss-Lobatto Points
           and Weights" by Seymor V. Parter, Journal of Sci. Comp., Vol. 14, 4, 1999 */

        double x = -std::cos((i + 0.25) * math::Pi<double> / n -
                             3 / (8 * n * math::Pi<double> * (i + 0.25)));
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

            if (std::abs(step) <= 4 * std::abs(x) * std::numeric_limits<double>::epsilon())
                break;
        }

        double l_n = math::legendre_p(n, x);
        weights[i] = weights[n - i] = (Float) (2 / ((n * (n + 1)) * l_n * l_n));
        nodes[i] = (Float) x;
        nodes[n - i] = (Float) -x;
        assert((Float) x > nodes[i-1]);
    }

    if ((n % 2) == 0) {
        double l_n = math::legendre_p(n, 0.0);
        weights[n / 2] = (Float) (2 / ((n * (n + 1)) * l_n * l_n));
        nodes[n/2] = 0;
    }
    return { nodes, weights };
}

template <typename FloatX>
std::pair<FloatX, FloatX> composite_simpson(int n) {
    using Float = scalar_t<FloatX>;
    if (n % 2 != 1 || n < 3)
        throw std::runtime_error("composite_simpson(): n must be >= 3 and odd");

    FloatX nodes, weights;
    set_slices(nodes, n);
    set_slices(weights, n);

    n = (n - 1) / 2;

    Float h      = (Float) 2 / (Float) (2 * n),
          weight = h * (Float) (1.0 / 3.0);

    for (int i = 0; i < n; ++i) {
        Float x = -1 + h * (2*i);
        nodes[2*i]     = x;
        nodes[2*i+1]   = x+h;
        weights[2*i]   = (i == 0 ? 1 : 2) * weight;
        weights[2*i+1] = 4 * weight;
    }

    nodes[2*n] = 1;
    weights[2*n] = weight;
    return { nodes, weights };
}

template <typename FloatX>
std::pair<FloatX, FloatX> composite_simpson_38(int n) {
    using Float = scalar_t<FloatX>;
    if ((n - 1) % 3 != 0 || n < 4)
        throw std::runtime_error("composite_simpson_38(): n-1 must be divisible by 3");

    FloatX nodes, weights;
    set_slices(nodes, n);
    set_slices(weights, n);

    n = (n - 1) / 3;

    Float h      = (Float) 2 / (Float) (3 * n),
          weight = h * (Float) (3.0 / 8.0);

    for (int i = 0; i < n; ++i) {
        Float x = -1 + h * (3*i);
        nodes[3*i]     = x;
        nodes[3*i+1]   = x+h;
        nodes[3*i+2]   = x+2*h;
        weights[3*i]   = (i == 0 ? 1 : 2) * weight;
        weights[3*i+1] = 3 * weight;
        weights[3*i+2] = 3 * weight;
    }

    nodes[3*n] = 1;
    weights[3*n] = weight;
    return { nodes, weights };
}

using Float32X = DynamicArray<Packet<float>>;
using Float64X = DynamicArray<Packet<double>>;

template MTS_EXPORT_CORE std::pair<Float32X, Float32X> gauss_legendre<Float32X>(int n);
template MTS_EXPORT_CORE std::pair<Float32X, Float32X> gauss_lobatto<Float32X>(int n);
template MTS_EXPORT_CORE std::pair<Float32X, Float32X> composite_simpson<Float32X>(int n);
template MTS_EXPORT_CORE std::pair<Float32X, Float32X> composite_simpson_38<Float32X>(int n);

template MTS_EXPORT_CORE std::pair<Float64X, Float64X> gauss_legendre<Float64X>(int n);
template MTS_EXPORT_CORE std::pair<Float64X, Float64X> gauss_lobatto<Float64X>(int n);
template MTS_EXPORT_CORE std::pair<Float64X, Float64X> composite_simpson<Float64X>(int n);
template MTS_EXPORT_CORE std::pair<Float64X, Float64X> composite_simpson_38<Float64X>(int n);

NAMESPACE_END(quad)
NAMESPACE_END(mitsuba)
