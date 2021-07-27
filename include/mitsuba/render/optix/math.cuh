#pragma once

#ifdef __CUDACC__
#define DEVICE __forceinline__ __device__

template <typename T>
DEVICE T sqr(const T& x) {
    return x * x;
}

DEVICE bool solve_quadratic(float a, float b, float c, float& x0, float&x1) {
    bool linear_case = (a == 0.f);
    // For linear eq, we require b != 0
    if (linear_case && b == 0.f)
        return false;

    x0 = x1 = -c / b;
    float discrim = fmaf(b, b, -4.f * a * c);

    // Check if the quadratic eq is solvable
    if (!linear_case && discrim < 0.f)
        return false;

    /* Numerically stable version of (-b (+/-) sqrt_discrim) / (2 * a)
     *
     * Based on the observation that one solution is always
     * accurate while the other is not. Finds the solution of
     * greater magnitude which does not suffer from loss of
     * precision and then uses the identity x1 * x2 = c / a
     */
    float temp = -0.5f * (b + copysign(sqrt(discrim), b));

    float x0p = temp / a,
          x1p = c / temp;

    // Order the results so that x0 < x1
    float x0m = min(x0p, x1p),
          x1m = max(x0p, x1p);

    x0 = (linear_case ? x0 : x0m);
    x1 = (linear_case ? x0 : x1m);

    return true;
}
#endif